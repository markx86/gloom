#!/usr/bin/env node

import { WebSocketServer } from "ws";
import { Game, GameMap } from "./game"
import { Client } from "./client";
import Logger from "./logger";

const wss = new WebSocketServer({
  port: 8492,
  perMessageDeflate: false,
});

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

Game.create(0xcafebabe, testMap);

wss.on("connection", ws => {
  Logger.info("Player connected");
  new Client(ws)
});

// update loop
let timestamp = performance.now();
setInterval(() => {
  const newTimestamp = performance.now();
  const delta = (newTimestamp - timestamp) / 1000.0;
  Game.tickAll(delta);
  timestamp = newTimestamp;
}, 1000 / 100);
