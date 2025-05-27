#!/usr/bin/env node

import { WebSocketServer } from "ws";
import { Game, GameMap } from "./game"
import { Client } from "./client";
import Logger from "./logger";

import { randomBytes, hash } from "node:crypto";
import { Database } from "sqlite3";
import express from "express";
import cookieParser from "cookie-parser"

const HTTP_PORT = 8080;
const WSS_PORT = 8492;

const USERNAME_MAX_LEN = 24
const USERNAME_MIN_LEN = 3
const PASSWORD_MAX_LEN = 64
const PASSWORD_MIN_LEN = 8
const SALT_LEN = 8

const SESSION_LIFETIME = 24 * 60 * 60;
const SESSION_ID_LEN = 32;
const COOKIE_SECRET = process.env.COOKIE_SECRET ?? randomBytes(32).toString("hex");
const DATABASE_PATH = process.env.DATABASE ?? ":memory:";


process.on("SIGINT", () => process.exit(0));

/*
 * ##########################
 * # DATABASE RELATED STUFF #
 * ##########################
 */
const db = new Database(DATABASE_PATH);
// close the db on exit
const closeDb = () => {
  Logger.info("Closing database...");
  db.close(() => {})
};
process.on("exit", closeDb);
process.on("uncaughtException", closeDb);

db.exec(`
  CREATE TABLE IF NOT EXISTS users (
    username VARCHAR(${USERNAME_MAX_LEN}) PRIMARY KEY,
    password BINARY(32) NOT NULL,
    salt BINARY(${SALT_LEN}) NOT NULL
  )
`);

db.exec(`
  CREATE TABLE IF NOT EXISTS sessions (
    session_id BINARY(32) PRIMARY KEY,
    username VARCHAR(${USERNAME_MAX_LEN}) NOT NULL UNIQUE,
    expiration_timestamp BIGINT NOT NULL
  )
`);

// cleanup sessions every 15 minutes
setInterval(() => {
  db.run(
    `
    DELETE FROM sessions
    WHERE expiration_timestamp <= unixepoch()
    `,
    function (err) {
      if (err) {
        Logger.error("Could not expire old sessions");
        Logger.error(err.message);
      }
    }
  );
}, 15 * 60 * 1000);

/*
 * #############################
 * # HTTP SERVER RELATED STUFF #
 * #############################
 */

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
  db.get<{ username: string }>(
    `
    SELECT username
    FROM sessions
    WHERE session_id = ? AND expiration_timestamp > unixepoch()
    `, sessionId,
    function (err, row) {
      if (err || row == null || row.username == null) {
        res.status(403).send();
      } else {
        res.locals.username = row.username;
        res.locals.sessionId = sessionId;
        next();
      }
    }
  );
});
app.use("/api", express.json());

function computePasswordHash(password: string, salt: Buffer): Buffer<ArrayBufferLike> {
  const passwordBuffer = Buffer.from(password);
  return hash('sha256', Buffer.concat([passwordBuffer, salt]), "buffer");
}

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

app.post("/api/register", (req, res) => {
  const data = req.body;
  if (data == null || data.username == null || data.password == null) {
    res.status(400).send({ message: "Invalid request body!" });
    return;
  }

  // do username validation
  const username = data.username;
  if (username.length < USERNAME_MIN_LEN) {
    res.status(400).send({ message: `Username too short! The minimum length is ${USERNAME_MIN_LEN} characters.` });
    return;
  }
  if (username.length > USERNAME_MAX_LEN) {
    res.status(400).send({ message: `Username too long! The maximum length is ${USERNAME_MAX_LEN} characters.` });
    return;
  }

  // do password validation
  const password = data.password;
  if (password.length < PASSWORD_MIN_LEN) {
    res.status(400).send({ message: `Password too short! The minimum length is ${PASSWORD_MIN_LEN} characters.` });
    return;
  }
  if (password.length > PASSWORD_MAX_LEN) {
    res.status(400).send({ message: `Password too long! The maximum length is ${PASSWORD_MAX_LEN} characters.` });
    return;
  }
  if (
    !(password.match(/[!"£$%&/()=?^\-\.+#@\\|~]/g)?.length > 0) ||
    !(password.match(/[0-9]/g).length > 0)
  ) {
    res.status(400).send({ message: "The password must contain at least one special character and one number." });
    return;
  }

  const salt = randomBytes(SALT_LEN);
  const passwordHash = computePasswordHash(password, salt);

  db.run(
    `
    INSERT INTO users (username, password, salt)
    VALUES (?, ?, ?)
    `, [username, passwordHash, salt],
    function (err) {
      if (err != null) {
        if (err["errno"] === 19) {
          res.status(400).send({ message: "That username is already taken!" });
        } else {
          Logger.error(
            "An error occurred when creating a user (username = %s, password = %s, salt = %s)",
            username, passwordHash.toString("hex"), salt.toString("hex")
          );
          Logger.error(err.message);
          res.status(500).send();
        }
      } else {
        Logger.info("Registered user: %s", username);
        res.status(200).send();
      }
    }
  );
});

app.post("/api/login", (req, res) => {
  const data = req.body;
  if (data == null || data.username == null || data.password == null) {
    res.status(400).send({ message: "Invalid request body!" });
    return;
  }

  // basic username validation
  const username = data.username;
  if (username.length > USERNAME_MAX_LEN || username.length < USERNAME_MIN_LEN) {
    res.status(403).send();
    return;
  }

  // basic password validation
  const password = data.password;
  if (password.length > PASSWORD_MAX_LEN || password.length < PASSWORD_MIN_LEN) {
    res.status(403).send();
    return;
  }

  db.get<{ password: Buffer, salt: Buffer }>(
    `
    SELECT password, salt
    FROM users
    WHERE username = ?
    `, username,
    function (err, row) {
      if (err || row == null || row.password == null || row.salt == null) {
        res.status(403).send();
        return;
      }
      // check if the passwords match
      const storedHash = row.password;
      const salt = row.salt;

      const passwordHash = computePasswordHash(password, salt);

      if (Buffer.compare(storedHash, passwordHash) !== 0) {
        res.status(403).send();
        return;
      }

      // generate session id and compute expiry date
      const sessionId = generateSessionId();
      const expirationTimestamp = getSessionCookieExpirationTimestamp();

      // store session
      db.run(
        `
        INSERT INTO sessions (session_id, username, expiration_timestamp)
        VALUES ($sessionId, $username, $expirationTimestamp)
        ON CONFLICT(username) DO UPDATE SET session_id = $sessionId, expiration_timestamp = $expirationTimestamp
        `, { $sessionId: sessionId, $username: username, $expirationTimestamp: expirationTimestamp },
        function (err) {
          if (err) {
            Logger.error(
              "An error occurred during login (sessionId = %s, username = %s, expirationTimestamp = %d)",
              sessionId.toString("hex"), username, expirationTimestamp
            );
            Logger.error(err.message);
            res.status(500).send();
          } else {
            Logger.info("Logging in user: %s", username);
            // set cookie
            setSessionCookie(res, sessionId);
            res.status(200).send();
          }
        }
      );
    }
  );
});

app.get("/api/logout", (_, res) => {
  clearSessionCookie(res);
  // also attempt to remove session from database
  db.run(
    `DELETE FROM sessions
    WHERE session_id = ?`,
    res.locals.sessionId,
    function (err) {
      if (err) {
        Logger.warning("Could not remove session from database");
        Logger.warning(err.message);
      }
      Logger.info("Logging out user: %s", res.locals.username);
      res.status(200).send();
    }
  );
});

app.get("/api/refresh-session", (_, res) => {
  // if got here that means the session cookie is valid
  const expirationTimestamp = getSessionCookieExpirationTimestamp();
  const sessionId = res.locals.sessionId;
  const newSessionId = generateSessionId();
  db.run(
    `
    UPDATE sessions
    SET session_id = ?, expiration_timestamp = ?
    WHERE session_id = ?`,
    [newSessionId, expirationTimestamp, sessionId],
    function (err) {
      if (err) {
        Logger.error("Could not update session expiration timestamp, invalidating...");
        res.status(403).send();
      } else {
        Logger.info("Refreshing session for user: %s", res.locals.username);
        setSessionCookie(res, newSessionId);
        res.status(200).send();
      }
    }
  );
});

app.listen(HTTP_PORT);


/*
 * #############################
 * # GAME SERVER RELATED STUFF #
 * #############################
 */

const testMap = new GameMap(8, 8, [
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 0, 0, 0, 1, 0, 0, 1,
  1, 0, 0, 0, 1, 0, 0, 1,
  1, 0, 0, 0, 1, 0, 0, 1,
  1, 0, 1, 1, 1, 1, 0, 1,
  1, 0, 0, 0, 0, 0, 0, 1,
  1, 0, 0, 0, 0, 0, 0, 1,
  1, 1, 1, 1, 1, 1, 1, 1
]);

Game.create(0xCAFEBABE, testMap);

const HANDSHAKE_MAGIC = 0xBADC0FFE

const wss = new WebSocketServer({
  port: WSS_PORT,
  perMessageDeflate: false,
});

wss.on("connection", ws => {
  Logger.info("Player connected");
  ws.on("message", (data, isBinary) => {
    if (!isBinary || !(data instanceof Buffer)) {
      return;
    }
    const view = new DataView(data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength));
    const token = view.getUint32(0, true);
    const magic = view.getUint32(4, true);
    if (magic === HANDSHAKE_MAGIC) {
      ws.removeAllListeners();
      new Client(ws, token);
      Logger.info("Handshake with player successful (token %s)", token.toString(16));
    } else {
      ws.close();
      Logger.error("Handshake failed (invalid magic, got %s expected %s)", magic.toString(16), HANDSHAKE_MAGIC.toString(16));
    }
  });
});

// update loop
let timestamp = performance.now();
setInterval(() => {
  const newTimestamp = performance.now();
  const delta = (newTimestamp - timestamp) / 1000.0;
  Game.tickAll(delta);
  timestamp = newTimestamp;
}, 1000 / 60);
