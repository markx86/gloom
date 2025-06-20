#!/usr/bin/env node

import Logger from "./logger.ts";

process.on("SIGINT", () => process.exit(0));

import { closeDb } from "./database.ts";
import { Game } from "./game.ts";

const shutdown = () => {
  Game.destroyAll();
  closeDb();
};

process.on("exit", shutdown);
process.on("uncaughtException", shutdown);

import { HTTP_PORT } from "./http-server.ts";
import { WSS_PORT } from "./game-server.ts";

if (HTTP_PORT === WSS_PORT) {
  Logger.error("HTTP_PORT cannot be the same as WSS_PORT");
  process.exit(-1);
}
