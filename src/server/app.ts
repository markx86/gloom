#!/usr/bin/env node

import Logger from "./logger.ts";

process.on("SIGINT", () => process.exit(0));

import { closeDb } from "./database.ts";

process.on("exit", closeDb);
process.on("uncaughtException", (e) => {
  Logger.error("Unhandled exception: %O", e);
  closeDb();
});

import { HTTP_PORT } from "./http-server.ts";
import { WSS_PORT } from "./game-server.ts";

if (HTTP_PORT === WSS_PORT) {
  Logger.error("HTTP_PORT cannot be the same as WSS_PORT");
  process.exit(-1);
}
