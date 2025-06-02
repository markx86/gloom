#!/usr/bin/env node

process.on("SIGINT", () => process.exit(0));

import { closeDb } from "./database.ts";
import { Game } from "./game.ts";

const shutdown = () => {
  Game.destroyAll();
  closeDb();
};

process.on("exit", shutdown);
process.on("uncaughtException", shutdown);

import "./http-server.ts";
import "./game-server.ts";

