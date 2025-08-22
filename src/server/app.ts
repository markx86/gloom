#!/usr/bin/env node

import http from "node:http";
import https from "node:https";
import Stream from "node:stream";
import { readFileSync } from "node:fs";

import Logger from "./logger.ts";
import { closeDb } from "./database.ts";
import { getEnvStringOrDefault } from "./util.ts";
import { HTTP_PORT, HTTPS_PORT } from "./ports.ts";
import { app } from "./http-server.ts"
import { wss } from "./game-server.ts"

const httpsOptions = {
  key: readFileSync(getEnvStringOrDefault("HTTPS_KEY", "./cert.key")),
  cert: readFileSync(getEnvStringOrDefault("HTTPS_CERT", "./cert.pem"))
};

process.on("SIGINT", () => process.exit(0));

process.on("exit", closeDb);
process.on("uncaughtException", (e) => {
  Logger.error("Unhandled exception: %O", e);
  closeDb();
  process.exit(-1);
});

function handleUpgrade(request: http.IncomingMessage, socket: Stream.Duplex, head: Buffer) {
  wss.handleUpgrade(request, socket, head, (ws, request) => wss.emit("connection", ws, request));
}

const httpServer = http.createServer(app);
httpServer.listen(HTTP_PORT);
httpServer.on("upgrade", handleUpgrade);

const httpsServer = https.createServer(httpsOptions, app);
httpsServer.listen(HTTPS_PORT);
httpsServer.on("upgrade", handleUpgrade);
