import { WebSocketServer } from "ws";
import { Game } from "./game"
import { Client } from "./client";
import Logger from "./logger";

const WSS_PORT = 8492;
const HANDSHAKE_MAGIC = 0xBADC0FFE

const wss = new WebSocketServer({
  port: WSS_PORT,
  perMessageDeflate: false,
});

wss.on("connection", ws => {
  Logger.trace("Player connected");
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
      Logger.success("Handshake with player successful (token %s)", token.toString(16));
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
