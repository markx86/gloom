#!/usr/bin/env node

import { WebSocketServer } from "ws";
import { Game, GameMap } from "./game"
import { Client } from "./client";
import Logger from "./logger";

import express from "express";
import path from "path";

/*
 * #############################
 * # HTTP SERVER RELATED STUFF #
 * #############################
 */
 
const app = express();
const port = 8080;
const staticRoot = { root: "static" };

function getPagePath(name: string): string {
  return path.join("html", `${name}.html`);
}

// This is just a nicer static middleware for HTML pages
app.get("/", (_, res) => res.sendFile(getPagePath("index"), staticRoot));
app.get("/:page", (req, res) => res.sendFile(getPagePath(req.params.page), staticRoot));

app.use("/static", express.static("static"))

app.listen(port);


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
  port: 8492,
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
