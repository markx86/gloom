import { Pool, types } from "pg";
import { randomBytes, hash } from "node:crypto";

import Logger from "./logger";
import { MAP_BASE64_SIZE } from "./map";

export const MAP_NAME_MAX_LEN = 32;
export const MAP_NAME_MIN_LEN = 3;
export const USERNAME_MAX_LEN = 24;
export const USERNAME_MIN_LEN = 3;
export const PASSWORD_MAX_LEN = 64;
export const PASSWORD_MIN_LEN = 8;
export const SESSION_ID_LEN = 32;
export const GAME_ID_LEN = 8;

const SALT_LEN = 8;
const PASSWORD_HASH_LEN = 32;

const pool = new Pool();

export type UserStats = { username: string, wins: number, kills: number, deaths: number, games: number, score: number };

export const closeDb = () => {
  Logger.trace("Closing database...");
  pool.end();
};

// Log unhandled exceptions.
pool.on("error", Logger.error);

export async function initDb() {
  // Users table.
  await pool.query(`
    CREATE TABLE IF NOT EXISTS users (
      username VARCHAR(${USERNAME_MAX_LEN}) PRIMARY KEY,
      password CHAR(${PASSWORD_HASH_LEN * 2}) NOT NULL,
      salt CHAR(${SALT_LEN * 2}) NOT NULL
    )
  `);

  // Sessions table.
  await pool.query(`
    CREATE TABLE IF NOT EXISTS sessions (
      session_id CHAR(${SESSION_ID_LEN * 2}) PRIMARY KEY,
      username VARCHAR(${USERNAME_MAX_LEN}) NOT NULL UNIQUE,
      expiration_timestamp INT NOT NULL,
      FOREIGN KEY (username) REFERENCES users(username)
    )
  `);
  
  // Stats table.
  await pool.query(`
    CREATE TABLE IF NOT EXISTS stats (
      username VARCHAR(${USERNAME_MAX_LEN}) PRIMARY KEY,
      wins INT NOT NULL,
      kills INT NOT NULL,
      deaths INT NOT NULL,
      games INT NOT NULL,
      FOREIGN KEY (username) REFERENCES users(username)
    )
  `);
  
  await pool.query(`
    CREATE TABLE IF NOT EXISTS maps (
      map_id SERIAL PRIMARY KEY,
      map_name VARCHAR(${MAP_NAME_MAX_LEN}) NOT NULL,
      creator VARCHAR(${USERNAME_MAX_LEN}) NOT NULL,
      map_data CHAR(${MAP_BASE64_SIZE}),
      FOREIGN KEY (creator) REFERENCES users(username),
      CONSTRAINT UC_map_name_creator UNIQUE (map_name, creator)
    )
  `);

  clearExpiredSessions();
  // Cleanup sessions every 15 minutes.
  setInterval(clearExpiredSessions, 15 * 60 * 1000);
}

async function clearExpiredSessions() {
  try {
    await pool.query(`
      DELETE FROM sessions
      WHERE expiration_timestamp <= EXTRACT(epoch FROM now())
    `);
  } catch (err) {
    Logger.error("Could not expire old sessions");
    Logger.error(err.message);
  }
}

export async function getUsernameBySessionId(sessionId: Buffer): Promise<string | undefined> {
  const res = await pool.query(`
    SELECT username
    FROM sessions
    WHERE session_id = $1 AND expiration_timestamp > EXTRACT(epoch FROM now())
    `, [sessionId.toString("hex")]
  );
  if (res.rowCount === 1) {
    return res.rows[0].username;
  }
}

function computePasswordHash(password: string, salt: Buffer): Buffer<ArrayBufferLike> {
  const passwordBuffer = Buffer.from(password);
  return hash('sha256', Buffer.concat([passwordBuffer, salt]), "buffer");
}

export async function createUser(username: string, password: string): Promise<boolean> {
  const salt = randomBytes(SALT_LEN);
  const passwordHash = computePasswordHash(password, salt);

  try {
    await pool.query(`
      INSERT INTO users (username, password, salt)
      VALUES ($1, $2, $3)
      `, [username, passwordHash.toString("hex"), salt.toString("hex")]
    );
    return true;
  } catch (err) {
    if (err.code === "23505") {
      return false;
    }
    Logger.error(
      "An error occurred when creating a user (username = '%s', password = %s, salt = %s)",
      username, passwordHash.toString("hex"), salt.toString("hex")
    );
    throw err;
  }
}

export async function checkUserCrendentials(username: string, password: string): Promise<boolean> {
  const res = await pool.query(`
    SELECT password, salt
    FROM users
    WHERE username = $1
    `, [username]
  );
  if (res.rowCount === 1) {
    const row = res.rows[0];
    // Binary data is stored as a hex string, that's why we use Buffer.from(.., "hex")
    const storedHash = Buffer.from(row.password, "hex");
    const salt = Buffer.from(row.salt, "hex");
    const passwordHash = computePasswordHash(password, salt);
    return Buffer.compare(storedHash, passwordHash) === 0;
  } else {
    return false;
  }
}

export async function setUserSession(username: string, sessionId: Buffer, expirationTimestamp: number) {
  try {
    await pool.query(`
      INSERT INTO sessions (session_id, username, expiration_timestamp)
      VALUES ($1, $2, $3)
      ON CONFLICT(username)
      DO UPDATE SET session_id = excluded.session_id, expiration_timestamp = excluded.expiration_timestamp
      `, [sessionId.toString("hex"), username, expirationTimestamp]
    );
  } catch (err) {
    Logger.error(
      "An error occurred during when storing user session (sessionId = %s, username = %s, expirationTimestamp = %d)",
      sessionId.toString("hex"), username, expirationTimestamp
    );
    throw err;
  }
}

export async function deleteSession(sessionId: Buffer) {
  try {
    await pool.query(
      `
      DELETE FROM sessions
      WHERE session_id = $1
      `, [sessionId.toString("hex")]
    );
  } catch (err) {
    Logger.warning("Could not remove session from database");
    Logger.warning(err?.message ?? "Session is not in database");
  }
}

export async function updateSession(currentSessionId: Buffer, newSessionId: Buffer, expirationTimestamp: number) {
  const res = await pool.query(`
    UPDATE sessions
    SET session_id = $1, expiration_timestamp = $2
    WHERE session_id = $3
    `, [newSessionId.toString("hex"), expirationTimestamp, currentSessionId.toString("hex")]
  );
  if (res.rowCount == null || res.rowCount === 0) {
    throw new Error("Could not update session ID");
  }
}

export async function updateUserStats(username: string, kills: number, isDead: boolean) {
  const won = !isDead ? 1 : 0;
  const died = isDead ? 1 : 0;
  await pool.query(`
    INSERT INTO stats (username, wins, kills, deaths, games)
    VALUES ($1, $2, $3, $4, 1)
    ON CONFLICT(username)
    DO UPDATE
    SET wins = stats.wins + excluded.wins, kills = stats.kills + excluded.kills, deaths = stats.deaths + excluded.deaths, games = stats.games + 1
    `, [username, won, kills, died]
  );
}

export async function getStatsAndLeaderboard(username: string): Promise<[UserStats, Array<UserStats>] | undefined> {
  const res = await pool.query<UserStats>(`
    SELECT username, wins, kills, deaths, games, ((wins + kills) * 1000 / (deaths + games)) as score
    FROM stats
    ORDER BY username = $1, score DESC
    LIMIT 11
    `, [username]
  );
  if (res.rowCount == null || res.rowCount === 0) {
    return;
  }
  const userStats = res.rows.find((stats) => stats.username === username)!;
  const leaderboard = res.rows.sort((statsA, statsB) => statsB.score - statsA.score).slice(0, 10);
  return [userStats, leaderboard];
}

export async function getUserMapList(username: string): Promise<Array<{mapId: number, mapName: string, mapData: string}>> {
  const res = await pool.query<{ map_id: number, map_name: string, map_data: string }>(`
    SELECT map_id, map_name, map_data FROM maps WHERE creator = $1
    `, [username]
  );
  if (res.rowCount == null || res.rowCount === 0) {
    return [];
  } else {
    return res.rows.map((row) => {
      return {
        mapId: row.map_id,
        mapName: row.map_name,
        mapData: row.map_data
      }
    });
  }
}

export async function deleteMap(mapId: number, creator: string): Promise<boolean> {
  const res = await pool.query(`
    DELETE FROM maps
    WHERE map_id = $1 AND creator = $2
    `, [mapId, creator]
  );
  return (res.rowCount != null && res.rowCount === 1);
}

export async function createMap(mapName: string, creator: string): Promise<number | undefined> {
  try {
    const res = await pool.query<{ map_id: number }>(`
      INSERT INTO maps (map_name, creator)
      VALUES ($1, $2)
      RETURNING map_id
      `, [mapName, creator]
    );
    if (res.rowCount == null || res.rowCount !== 1) {
      throw new Error("Database returned 0 rows");
    } else {
      return res.rows[0].map_id;
    }
  } catch (err) {
    if (err.code !== "23505") {
      throw err;
    }
  }
}

export async function updateMap(mapId: number, mapData: string): Promise<boolean> {
  const res = await pool.query(`
    UPDATE maps
    SET map_data = $1
    WHERE map_id = $2
    `, [mapData, mapId]
  );
  return res.rowCount != null && res.rowCount === 1;
}
