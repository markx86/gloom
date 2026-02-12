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
} from "./database";
import Logger from "./logger";
import { Maps } from "./map"
import { Game } from "./game";
import { getEnvStringOrDefault } from "./util";

import { randomBytes } from "node:crypto";
import express from "express";
import cookieParser from "cookie-parser";

const COOKIE_SECRET = getEnvStringOrDefault("COOKIE_SECRET", randomBytes(32).toString("hex"));
const SESSION_LIFETIME = 24 * 60 * 60;

export const app = express();

app.get("/", (_, res) => { res.sendFile("index.html", { root: "static/html" }); });
app.use("/static", express.static("static"));

app.use("/api", cookieParser(COOKIE_SECRET))
app.use("/api", (req, res, next) => {
  if (req.path === "/register" || req.path === "/login") {
    next();
    return;
  }

  // Get session cookie.
  const sessionCookie = req.signedCookies.session;
  if (sessionCookie == null || typeof(sessionCookie) !== "string") {
    res.status(401).send();
    return;
  }

  // Retrieve session data.
  const sessionId = Buffer.from(sessionCookie, "base64url");
  try {
    getUsernameBySessionId(sessionId, (username) => {
      if (username == null) {
        res.status(401).send();
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
  if (data == null || typeof(data.username) !== "string" || typeof(data.password) !== "string") {
    res.status(400).send({ message: "Invalid request body!" });
    return;
  }

  // Do username validation.
  const username = data.username.trim();
  rc = checkUsernameLength(username);
  if (rc > 0) {
    res.status(400).send({ message: `Username too long! The maximum length is ${USERNAME_MAX_LEN} characters.` });
    return;
  } else if (rc < 0) {
    res.status(400).send({ message: `Username too short! The minimum length is ${USERNAME_MIN_LEN} characters.` });
    return;
  }

  // Do password validation.
  const password = data.password;
  rc = checkPasswordLength(password);
  if (rc > 0) {
    res.status(400).send({ message: `Password too long! The maximum length is ${PASSWORD_MAX_LEN} characters.` });
    return;
  } else if (rc < 0) {
    res.status(400).send({ message: `Password too short! The minimum length is ${PASSWORD_MIN_LEN} characters.` });
    return;
  } else if (
    !(password.match(/[!'"£$%&/()=?^\-\.+#@\\|~]/g)?.length > 0) ||
    !(password.match(/[0-9]/g).length > 0)
  ) {
    res.status(400).send({ message: "The password must contain at least one special character and one number." });
    return;
  }

  try {
    registerUser(username, password, (success) => {
      if (success) {
        Logger.trace("Registered user: %s", username);
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
  if (data == null || typeof(data.username) !== "string" || typeof(data.password) !== "string") {
    res.status(400).send({ message: "Invalid request body!" });
    return;
  }

  const errorResponse = { message: "Invalid credentials" };

  // Basic username validation.
  const username = data.username.trim();
  if (checkUsernameLength(username) !== 0) {
    res.status(401).send(errorResponse);
    return;
  }

  // Basic password validation.
  const password = data.password;
  if (checkPasswordLength(password) !== 0) {
    res.status(401).send(errorResponse);
    return;
  }

  try {
    checkUserCrendentials(username, password, (success) => {
      if (success) {
        Logger.trace("Logging in user: %s", username);
        // Generate session id and compute expiry date.
        const sessionId = generateSessionId();
        const expirationTimestamp = getSessionCookieExpirationTimestamp();
        // Store session.
        setUserSession(username, sessionId, expirationTimestamp);
        // Set session cookie.
        setSessionCookie(res, sessionId);
        res.status(200).send();
      } else {
        res.status(401).send(errorResponse);
      }
    });
  } catch (e) {
    Logger.error(e.message);
    res.status(500).send();
  }
});

app.get("/api/logout", (_, res) => {
  Logger.trace("Logging out user: %s", res.locals.username);
  invalidateSession(res.locals.sessionId);
  clearSessionCookie(res);
  res.status(200).send();
});

app.get("/api/session/validate", (_, res) => {
  res.status(200).send({ username: res.locals.username });
});

app.get("/api/session/refresh", (_, res) => {
  // If got here that means the session cookie is valid.
  try {
    Logger.trace("Refreshing session for user: %s", res.locals.username);
    const expirationTimestamp = getSessionCookieExpirationTimestamp();
    const currentSessionId = res.locals.sessionId;
    const newSessionId = generateSessionId();
    refreshSession(currentSessionId, newSessionId, expirationTimestamp);
    setSessionCookie(res, newSessionId);
    res.status(200).send({ username: res.locals.username });
  } catch (e) {
    Logger.error(e.message);
    res.status(500).send();
  }
});

app.get("/api/game/id", (_, res) => {
  const username = res.locals.username;
  const game = Game.getByCreator(username);
  if (game == null) {
    res.status(404).send();
  } else {
    res.status(200).send({ gameId: game.id });
  }
});

app.get("/api/game/create", (_, res) => {
  const creator = res.locals.username;
  if (Game.getByCreator(creator) != null) {
    res.status(403).send({
      message: "You already have a game running.",
    });
    return;
  }
  
  const gameId = Game.create(creator, Maps.random());
  if (gameId == null) {
    res.status(503).send({
      message: "The server is overloaded. Please try again later."
    });
  } else {
    res.status(200).send({ gameId });
  }
});

app.post("/api/game/join", (req, res) => {
  const data = req.body;
  if (data == null || typeof(data.gameId) !== "number") {
    res.status(400).send({ message: "Invalid request body." });
    return;
  }

  const gameId = data.gameId;
  const game = Game.getById(gameId);
  if (game == null) {
    res.status(404).send({ message: "No game found." });
    return;
  }

  const playerToken = game.allocatePlayer(res.locals.username);
  if (typeof(playerToken) === "string") {
    res.status(403).send({ message: playerToken });
  } else {
    res.status(200).send({ playerToken });
  }
});
