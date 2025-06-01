import Logger from "./logger.ts";
import { Database } from "sqlite3";
import { randomBytes, hash } from "node:crypto";

export const USERNAME_MAX_LEN = 24;
export const USERNAME_MIN_LEN = 3;
export const PASSWORD_MAX_LEN = 64;
export const PASSWORD_MIN_LEN = 8;
export const SESSION_ID_LEN = 32;
export const GAME_ID_LEN = 8;

const SALT_LEN = 8;

const DATABASE_PATH = process.env.DATABASE ?? ":memory:";

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
    session_id BINARY(${SESSION_ID_LEN}) PRIMARY KEY,
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

export function getUsernameBySessionId(sessionId: Buffer, callback: (username: string) => undefined) {
  db.get<{ username: string }>(
    `
    SELECT username
    FROM sessions
    WHERE session_id = ? AND expiration_timestamp > unixepoch()
    `, sessionId,
    function (err, row) {
      if (err != null) {
        throw err;
      }
      callback(row?.username);
    }
  );
}

function computePasswordHash(password: string, salt: Buffer): Buffer<ArrayBufferLike> {
  const passwordBuffer = Buffer.from(password);
  return hash('sha256', Buffer.concat([passwordBuffer, salt]), "buffer");
}

export function registerUser(username: string, password: string, callback: (success: boolean) => undefined) {
  const salt = randomBytes(SALT_LEN);
  const passwordHash = computePasswordHash(password, salt);

  db.run(
    `
    INSERT INTO users (username, password, salt)
    VALUES (?, ?, ?)
    `, [username, passwordHash, salt],
    function (err) {
      let rv = true;
      if (err != null) {
        if (err["errno"] === 19) {
          rv = false;
        } else {
          Logger.error(
            "An error occurred when creating a user (username = %s, password = %s, salt = %s)",
            username, passwordHash.toString("hex"), salt.toString("hex")
          );
          throw err;
        }
      }
      callback(rv);
    }
  );
}

export function checkUserCrendentials(username: string, password: string, callback: (success: boolean) => undefined) {
  db.get<{ password: Buffer, salt: Buffer }>(
    `
    SELECT password, salt
    FROM users
    WHERE username = ?
    `, username,
    function (err, row) {
      if (err != null) {
        throw err;
      } else if (row != null) {
        // check if the passwords match
        const storedHash = row.password;
        const salt = row.salt;
        const passwordHash = computePasswordHash(password, salt);
        callback(Buffer.compare(storedHash, passwordHash) === 0);
      } else {
        callback(false);
      }
    }
  );
}

export function setUserSession(username: string, sessionId: Buffer, expirationTimestamp: number) {
  db.run(
    `
    INSERT INTO sessions (session_id, username, expiration_timestamp)
    VALUES ($sessionId, $username, $expirationTimestamp)
    ON CONFLICT(username) DO UPDATE SET session_id = $sessionId, expiration_timestamp = $expirationTimestamp
    `, { $sessionId: sessionId, $username: username, $expirationTimestamp: expirationTimestamp },
    function (err) {
      if (err != null) {
        Logger.error(
          "An error occurred during login (sessionId = %s, username = %s, expirationTimestamp = %d)",
          sessionId.toString("hex"), username, expirationTimestamp
        );
        throw err;
      }
    }
  );
}

export function invalidateSession(sessionId: Buffer) {
  db.run(
    `
    DELETE FROM sessions
    WHERE session_id = ?
    `, sessionId,
    function (err) {
      if (err != null || this.changes === 0) {
        Logger.warning("Could not remove session from database");
        Logger.warning(err?.message ?? "Session is not in database");
      }
    }
  );
}

export function refreshSession(currentSessionId: Buffer, newSessionId: Buffer, expirationTimestamp: number) {
  db.run(
    `
    UPDATE sessions
    SET session_id = ?, expiration_timestamp = ?
    WHERE session_id = ?
    `, [newSessionId, expirationTimestamp, currentSessionId],
    function (err) {
      if (err != null) {
        throw err;
      } else if (this.changes === 0) {
        throw new Error("Could not update session ID");
      }
    }
  );
}
