#!/usr/bin/env node

import { WebSocketServer } from "ws";
import Logger from "./logger";

const wss = new WebSocketServer({
  port: 8492,
  perMessageDeflate: false,
});

wss.on("connection", (ws, request) => {
  ws.on("error", Logger.error);
  ws.on("message", Logger.info);
});
