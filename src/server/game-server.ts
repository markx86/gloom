import { WebSocketServer } from "ws";
import { Game, DT } from "./game"
import { Client } from "./client";
import { getEnvIntOrDefault } from "./util";
import Logger from "./logger";

export const WSS_PORT = getEnvIntOrDefault("WSS_PORT", 8492, 0, 0xFFFF);
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
    const playerToken = view.getUint32(0, true);
    const gameId = view.getUint32(4, true);
    const magic = view.getUint32(8, true);
    if (magic === HANDSHAKE_MAGIC) {
      ws.removeAllListeners();
      new Client(ws, playerToken, gameId);
      Logger.success("Handshake with player successful (token %s)", playerToken.toString(16));
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
}, DT);
