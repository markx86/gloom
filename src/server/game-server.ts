import { WebSocketServer, WebSocket } from "ws";
import { Game, DT } from "./game"
import { Client } from "./client";
import Logger from "./logger";

const HANDSHAKE_MAGIC = 0xBADC0FFE

export const wss = new WebSocketServer({
  noServer: true,
  perMessageDeflate: false,
});

function createClient(ws: WebSocket, playerToken: number, gameId: number): boolean {
  const game = Game.getById(gameId);
  if (game == null) {
    Logger.error("No game with that ID");
    return false;
  }
  
  const player = game.newPlayer(playerToken);
  if (player == null) {
    return false;
  }

  Logger.success("Client (%s) joined game (%s) with ID %d", playerToken.toString(16), gameId.toString(16), player.id);
  new Client(ws, player);
  return true;
}

wss.on("connection", ws => {
  Logger.trace("Player connected");
  ws.on("message", (data, isBinary) => {
    if (!isBinary || !(data instanceof Buffer) || data.byteLength !== 12 /* Handshake packet is 12 bytes big */) {
      return;
    }
    ws.removeAllListeners();

    const view = new DataView(data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength));
    const playerToken = view.getUint32(0, true);
    const gameId = view.getUint32(4, true);
    const magic = view.getUint32(8, true);

    if (magic === HANDSHAKE_MAGIC && createClient(ws, playerToken, gameId)) {
      Logger.success("Handshake with player successful (token %s)", playerToken.toString(16));
    } else {
      Logger.error("Handshake failed (invalid magic, got %s expected %s)", magic.toString(16), HANDSHAKE_MAGIC.toString(16));
      ws.close();
    }
  });
});

// Update loop.
let timestamp = performance.now();
setInterval(() => {
  const newTimestamp = performance.now();
  const delta = (newTimestamp - timestamp) / 1000.0;
  Game.tickAll(delta);
  timestamp = newTimestamp;
}, DT);
