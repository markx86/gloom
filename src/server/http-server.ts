import Logger from "./logger.ts";
import { randomBytes } from "node:crypto";
import {
  USERNAME_MAX_LEN, USERNAME_MIN_LEN,
  PASSWORD_MAX_LEN, PASSWORD_MIN_LEN,
  SESSION_ID_LEN,
  getUsernameBySessionId,
  registerUser,
  checkUserCrendentials,
  setUserSession,
  invalidateSession,
  refreshSession
} from "./database.ts";
import express from "express";
import cookieParser from "cookie-parser";

const HTTP_PORT = 8080;

const SESSION_LIFETIME = 24 * 60 * 60;
const COOKIE_SECRET = process.env.COOKIE_SECRET ?? randomBytes(32).toString("hex");

const app = express();

app.get("/", (_, res) => { res.sendFile("index.html", { root: "static/html" }); });
app.use("/static", express.static("static"));

app.use("/api", cookieParser(COOKIE_SECRET))
app.use("/api", (req, res, next) => {
  if (req.path === "/register" || req.path === "/login") {
    next();
    return;
  }

  // get session cookie
  const sessionCookie = req.signedCookies.session;
  if (sessionCookie == null || typeof(sessionCookie) !== "string") {
    res.status(403).send();
    return;
  }

  // retrieve session data
  const sessionId = Buffer.from(sessionCookie, "base64url");
  try {
    getUsernameBySessionId(sessionId, (username) => {
      if (username == null) {
        res.status(403).send();
      } else {
        res.locals.username = username;
        res.locals.sessionId = sessionId;
        next();
      }
    });
  } catch (e) {
    Logger.error(e.message);
    res.status(500).send();
  }
});
app.use("/api", express.json());

function getSessionCookieExpirationTimestamp(): number {
  return Math.floor(Date.now() / 1e3) + SESSION_LIFETIME;
}

function getSessionCookieOptions(lifetime: number = 0): express.CookieOptions {
  return {
    signed: true,
    httpOnly: true,
    sameSite: "strict",
    path: "/api",
    maxAge: lifetime * 1e3
  };
}

function setSessionCookie(res: express.Response, sessionId: Buffer) {
  res.cookie("session", sessionId.toString("base64url"), getSessionCookieOptions(SESSION_LIFETIME));
}

function clearSessionCookie(res: express.Response) {
  res.cookie("session", "", getSessionCookieOptions());
}

function generateSessionId(): Buffer { return randomBytes(SESSION_ID_LEN); }

function checkUsernameLength(username: string): number {
  if (username.length < USERNAME_MIN_LEN) {
    return -1;
  } else if (username.length > USERNAME_MAX_LEN) {
    return +1;
  } else {
    return 0;
  }
}

function checkPasswordLength(password: string): number {
  if (password.length < PASSWORD_MIN_LEN) {
    return -1;
  } else if (password.length > PASSWORD_MAX_LEN) {
    return +1;
  } else {
    return 0;
  }
}

app.post("/api/register", (req, res) => {
  let rc: number;

  const data = req.body;
  if (data == null || data.username == null || data.password == null) {
    res.status(400).send({ message: "Invalid request body!" });
    return;
  }

  // do username validation
  const username = data.username;
  rc = checkUsernameLength(username);
  if (rc > 0) {
    res.status(400).send({ message: `Username too long! The maximum length is ${USERNAME_MAX_LEN} characters.` });
    return;
  } else if (rc < 0) {
    res.status(400).send({ message: `Username too short! The minimum length is ${USERNAME_MIN_LEN} characters.` });
    return;
  }

  // do password validation
  const password = data.password;
  rc = checkPasswordLength(password);
  if (rc > 0) {
    res.status(400).send({ message: `Password too long! The maximum length is ${PASSWORD_MAX_LEN} characters.` });
    return;
  } else if (rc < 0) {
    res.status(400).send({ message: `Password too short! The minimum length is ${PASSWORD_MIN_LEN} characters.` });
    return;
  } else if (
    !(password.match(/[!"£$%&/()=?^\-\.+#@\\|~]/g)?.length > 0) ||
    !(password.match(/[0-9]/g).length > 0)
  ) {
    res.status(400).send({ message: "The password must contain at least one special character and one number." });
    return;
  }

  try {
    registerUser(username, password, (success) => {
      if (success) {
        Logger.info("Registered user: %s", username);
        res.status(200).send();
      } else {
        res.status(400).send({ message: "That username is already taken!" });
      }
    });
  } catch (e) {
    Logger.error(e.message);
    res.status(500).send();
  }
});

app.post("/api/login", (req, res) => {
  const data = req.body;
  if (data == null || data.username == null || data.password == null) {
    res.status(400).send({ message: "Invalid request body!" });
    return;
  }

  // basic username validation
  const username = data.username;
  if (checkUsernameLength(username) !== 0) {
    res.status(403).send();
    return;
  }

  // basic password validation
  const password = data.password;
  if (checkPasswordLength(password) !== 0) {
    res.status(403).send();
    return;
  }

  try {
    checkUserCrendentials(username, password, (success) => {
      if (success) {
        Logger.info("Logging in user: %s", username);
        // generate session id and compute expiry date
        const sessionId = generateSessionId();
        const expirationTimestamp = getSessionCookieExpirationTimestamp();
        // store session
        setUserSession(username, sessionId, expirationTimestamp);
        // set session cookie
        setSessionCookie(res, sessionId);
        res.status(200).send();
      } else {
        res.status(403).send();
      }
    });
  } catch (e) {
    Logger.error(e.message);
    res.status(500).send();
  }
});

app.get("/api/logout", (_, res) => {
  Logger.info("Logging out user: %s", res.locals.username);
  invalidateSession(res.locals.sessionId);
  clearSessionCookie(res);
  res.status(200).send();
});

app.get("/api/refresh-session", (_, res) => {
  // if got here that means the session cookie is valid
  try {
    Logger.info("Refreshing session for user: %s", res.locals.username);
    const expirationTimestamp = getSessionCookieExpirationTimestamp();
    const currentSessionId = res.locals.sessionId;
    const newSessionId = generateSessionId();
    refreshSession(currentSessionId, newSessionId, expirationTimestamp);
    setSessionCookie(res, newSessionId);
    res.status(200).send();
  } catch (e) {
    Logger.error(e.message);
    res.status(500).send();
  }
});

app.listen(HTTP_PORT);
