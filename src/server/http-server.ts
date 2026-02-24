import {
  USERNAME_MAX_LEN, USERNAME_MIN_LEN,
  PASSWORD_MAX_LEN, PASSWORD_MIN_LEN,
  SESSION_ID_LEN,
  MAP_NAME_MIN_LEN, MAP_NAME_MAX_LEN
} from "./database";
import * as db from "./database";
import Logger from "./logger";
import { GameMap, Maps } from "./map"
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
app.use("/api", async (req, res, next) => {
  if (req.path === "/register" || req.path === "/login") {
    next();
    return;
  }

  // Get session cookie.
  const sessionCookie = req.signedCookies.session;
  if (typeof(sessionCookie) !== "string") {
    res.status(401).send();
    return;
  }

  // Retrieve session data.
  const sessionId = Buffer.from(sessionCookie, "base64url");

  try {
    const username = await db.getUsernameBySessionId(sessionId);
    if (username == null) {
      res.status(401).send();
    } else {
      res.locals.username = username;
      res.locals.sessionId = sessionId;
      next();
    }
  } catch(error) {
    Logger.error(error.message);
    res.status(500).send();
  }
});
app.use("/api", express.json());

function getSessionCookieExpirationTimestamp(): number {
  // TODO: Fix 2038 problem
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

app.post("/api/register", async (req, res) => {
  let rc: number;

  const data = req.body;
  if (typeof(data?.username) !== "string" || typeof(data?.password) !== "string") {
    res.status(400).json({ message: "Invalid request body!" });
    return;
  }

  // Do username validation.
  const username = data.username.trim();
  rc = checkUsernameLength(username);
  if (rc > 0) {
    res.status(400).json({ message: `Username too long! The maximum length is ${USERNAME_MAX_LEN} characters.` });
    return;
  } else if (rc < 0) {
    res.status(400).json({ message: `Username too short! The minimum length is ${USERNAME_MIN_LEN} characters.` });
    return;
  }

  // Do password validation.
  const password = data.password;
  rc = checkPasswordLength(password);
  if (rc > 0) {
    res.status(400).json({ message: `Password too long! The maximum length is ${PASSWORD_MAX_LEN} characters.` });
    return;
  } else if (rc < 0) {
    res.status(400).json({ message: `Password too short! The minimum length is ${PASSWORD_MIN_LEN} characters.` });
    return;
  } else if (
    !(password.match(/[!'"£$%&/()=?^\-\.+#@\\|~]/g)?.length > 0) ||
    !(password.match(/[0-9]/g).length > 0)
  ) {
    res.status(400).json({ message: "The password must contain at least one special character and one number." });
    return;
  }

  try {
    const success = await db.createUser(username, password);
    if (success) {
      Logger.trace("Registered user %s", username);
      res.status(200).send();
    } else {
      res.status(400).json({ message: "That username is already taken!" });
    }
  } catch(error) {
    Logger.error(error.message);
    res.status(500).send();
  };
});

app.post("/api/login", async (req, res) => {
  const data = req.body;
  if (typeof(data?.username) !== "string" || typeof(data?.password) !== "string") {
    res.status(400).json({ message: "Invalid request body!" });
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
    const success = await db.checkUserCrendentials(username, password);
    if (success) {
      Logger.trace("Logging in user %s", username);
      // Generate session id and compute expiry date.
      const sessionId = generateSessionId();
      const expirationTimestamp = getSessionCookieExpirationTimestamp();
      // Store session.
      await db.setUserSession(username, sessionId, expirationTimestamp);
      // Set session cookie.
      setSessionCookie(res, sessionId);
      res.status(200).send();
    } else {
      res.status(401).send(errorResponse);
    }
  } catch (error) {
    Logger.error(error.message);
    res.status(500).send();
  }
});

app.get("/api/logout", async (_, res) => {
  Logger.trace("Logging out user %s", res.locals.username);
  try {
    await db.deleteSession(res.locals.sessionId);
    clearSessionCookie(res);
    res.status(200).send();
  } catch(error) {
    Logger.error("Could not invalidate session!");
    Logger.error(error.message);
    res.status(500).send();
  }
});

app.get("/api/stats", async (_, res) => {
  const username = res.locals.username;
  Logger.trace("Fetching leaderboard for user %s", username);
  try {
    const result = await db.getStatsAndLeaderboard(username);
    if (result == null) {
      res.status(404).send();
    } else {
      const [userStats, leaderboard] = result;
      res.status(200).json({
        userStats,
        leaderboard
      });
    }
  } catch (error) {
    Logger.error(error.message);
    res.status(500).send();
  }
});

app.get("/api/session/validate", (_, res) => {
  res.status(200).json({ username: res.locals.username });
});

app.get("/api/session/refresh", async (_, res) => {
  // If got here that means the session cookie is valid.
  Logger.trace("Refreshing session for user %s", res.locals.username);
  const expirationTimestamp = getSessionCookieExpirationTimestamp();
  const currentSessionId = res.locals.sessionId;
  const newSessionId = generateSessionId();
  try {
    await db.updateSession(currentSessionId, newSessionId, expirationTimestamp);
    setSessionCookie(res, newSessionId);
    res.status(200).json({ username: res.locals.username });
  } catch(error) {
    Logger.error(error.message);
    res.status(500).send();
  }
});

app.get("/api/game/id", (_, res) => {
  const username = res.locals.username;
  const game = Game.getByCreator(username);
  if (game == null) {
    res.status(404).send();
  } else {
    res.status(200).json({ gameId: game.id });
  }
});

app.get("/api/game/create", (_, res) => {
  const creator = res.locals.username;
  if (Game.getByCreator(creator) != null) {
    res.status(403).json({
      message: "You already have a game running.",
    });
    return;
  }
  
  const gameId = Game.create(creator, Maps.random());
  if (gameId == null) {
    res.status(503).json({
      message: "The server is overloaded. Please try again later."
    });
  } else {
    res.status(200).json({ gameId });
  }
});

app.post("/api/game/join", (req, res) => {
  const data = req.body;
  if (typeof(data?.gameId) !== "number") {
    res.status(400).json({ message: "Invalid request body." });
    return;
  }

  const gameId = data.gameId;
  const game = Game.getById(gameId);
  if (game == null) {
    res.status(404).json({ message: "No game found." });
    return;
  }

  const playerToken = game.allocatePlayer(res.locals.username);
  if (typeof(playerToken) === "string") {
    res.status(403).json({ message: playerToken });
  } else {
    res.status(200).json({ playerToken });
  }
});

app.get("/api/map/list", async (_, res) => {
  try {
    const maps = await db.getUserMapList(res.locals.username);
    res.status(200).json(maps);
  } catch (error) {
    Logger.error("Could not get map list for user '%s'", res.locals.username);
    Logger.error(error);
    res.status(500).send();
  }
});

app.post("/api/map/delete", async (req, res) => {
  const mapId = req.body?.mapId;
  if (typeof(mapId) !== "number") {
    res.status(400).json({ message: "Invalid map ID!" });
    return;
  }

  try {
    if (await db.deleteMap(mapId, res.locals.username)) {
      res.status(200).send();
    } else {
      res.status(404).json({ message: `Could not find map with ID ${mapId} that belongs to you!` });
    }
  } catch (error) {
    Logger.error("Could not delete map with ID %d", mapId);
    Logger.error(error);
    res.status(500).send();
  }
});

app.post("/api/map/create", async (req, res) => {
  const mapName = req.body?.mapName;

  if (typeof(mapName) !== "string") {
    res.status(400).json({ message: "Invalid map name." });
    return;
  } else if (mapName.length < MAP_NAME_MIN_LEN) {
    res.status(400).json({ message: `Map name is too short. It must be at least ${MAP_NAME_MIN_LEN} characters.` })
    return;
  } else if (mapName.length > MAP_NAME_MAX_LEN) {
    res.status(400).json({ message: `Map name is too long. The maximum length is ${MAP_NAME_MAX_LEN} characters.` })
    return;
  }

  try {
    const mapId = await db.createMap(mapName, res.locals.username);
    if (mapId == null) {
      res.status(400).json({ message: "That map already exists!" });
    } else {
      res.status(200).json({ mapId });
    }
  } catch (error) {
    Logger.error("Could not create map with name '%s'", mapName);
    Logger.error(error);
    res.status(500).send();
  }
});

app.post("/api/map/update", async (req, res) => {
  const data = req.body;

  const mapId = data?.mapId;
  const mapData = data?.mapData;
  if (typeof(mapId) !== "number" || typeof(mapData) !== "string") {
    res.status(400).json({ message: "Invalid request body." });
    return;
  }

  try {
    const map = GameMap.deserialize(mapData);
    if (map.check()) {
      // NOTE: Maybe we can avoid reserializing the map?
      if (await db.updateMap(mapId, map.serialize())) {
        res.status(200).send();
      } else {
        res.status(404).json({ message: `No map with ID ${mapId} exists` })
      }
    } else {
      res.status(400).json({ message: "Invalid map! Not all players can reach each other." })
    }
  } catch (error) {
    Logger.error("Could not deserialize map %d", mapId);
    Logger.error(error);
    res.status(400).json({ message: "Invalid map data!" });
  }
});
