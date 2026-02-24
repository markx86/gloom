#!/usr/bin/env node

import http from "node:http";
import https from "node:https";
import Stream from "node:stream";
import { readFileSync } from "node:fs";

import Logger from "./logger";
import { initDb, closeDb } from "./database";
import { getEnvStringOrDefault } from "./util";
import { HTTP_PORT, HTTPS_PORT } from "./ports";
import { app } from "./http-server";
import { wss } from "./game-server";

const INADDR_ANY = "0.0.0.0";

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

initDb();

function handleUpgrade(request: http.IncomingMessage, socket: Stream.Duplex, head: Buffer) {
  if (request.url === "/game") {
    wss.handleUpgrade(request, socket, head, (ws, request) => wss.emit("connection", ws, request));
  } else {
    socket.write("HTTP/1.1 404 Not Found\r\n\r\n");
  }
}

const httpServer = http.createServer(app);
httpServer.listen(HTTP_PORT, INADDR_ANY);
httpServer.on("upgrade", handleUpgrade);

const httpsServer = https.createServer(httpsOptions, app);
httpsServer.listen(HTTPS_PORT, INADDR_ANY);
httpsServer.on("upgrade", handleUpgrade);
