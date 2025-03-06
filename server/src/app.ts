#!/usr/bin/env node

import { WebSocketServer, WebSocket } from "ws";
import Logger from "./logger";

const games = new Map<number, Array<PlayerConnection | null>>();

function getGameByID(gameId: number): Array<PlayerConnection | null> | null {
  const playersList = games.get(gameId);
  if (!playersList) {
    Logger.error("No game exists with ID %s", gameId.toString(16));
    return null;
  }
  return playersList;
}

const wss = new WebSocketServer({
  port: 8492,
  perMessageDeflate: false,
});

enum GamePacketType {
  GPKT_JOIN,
  GPKT_LEAVE,
  GPKT_UPDATE
};

enum ServerPacketType {
  SPKT_HELLO,
  SPKT_UPDATE,
  SPKT_BYE,
  SPKT_MAX
}

const PLAYER_RUN_SPEED = 3.5;
const MAX_PLAYERS = 16;
const MAX_PACKET_DROP = 5;

class GameMap {
  private compressed: Uint8Array | undefined;
  private tiles: Uint8Array;
  private width: number;
  private height: number;

  public static create(width: number, height: number, tiles: ArrayLike<number>): GameMap | null {
    if (width * height != tiles.length) {
      Logger.error("Trying to create map of size %dx%d with only %d tiles!", width, height, tiles.length);
      return null;
    }
    return new GameMap(width, height, new Uint8Array(tiles));
  }

  private constructor(width: number, height: number, tiles: Uint8Array) {
    this.width = width;
    this.height = height;
    this.tiles = tiles;
  }

  public getCompressedData(): Uint8Array {
    if (!this.compressed) {
      const compressed = this.tiles.reduce((running, tileValue, tileIndex) => {
        tileValue &= 3;
        const bitPos = (tileIndex & 3) << 1;
        if (bitPos === 0) {
          running.push(tileValue);
        } else {
          running[tileIndex >> 2] |= tileValue << bitPos;
        }
        return running;
      }, new Array<number>())
      this.compressed = new Uint8Array(compressed);
    }
    return this.compressed;
  }

  public getWidth(): number {
    return this.width;
  }

  public getHeight(): number {
    return this.height;
  }

  public testBlockAt(x: number, y: number) {
    x = Math.floor(x);
    if (x >= this.width || x < 0) {
      return true;
    }
    y = Math.floor(y);
    if (y >= this.height || y < 0) {
      return true;
    }
    return this.tiles[x + y * this.width] !== 0;
  }
}

const testMap = GameMap.create(8, 8, [
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 0, 0, 0, 1, 0, 0, 1,
  1, 0, 0, 0, 1, 0, 0, 1,
  1, 0, 0, 0, 1, 0, 0, 1,
  1, 0, 1, 1, 1, 1, 0, 1,
  1, 0, 0, 0, 0, 0, 0, 1,
  1, 0, 0, 0, 0, 0, 0, 1,
  1, 1, 1, 1, 1, 1, 1, 1
]);

games.set(0xcafebabe, []);

class PlayerTracker {
  private r: number;
  private x: number;
  private y: number;
  private vx: number;
  private vy: number;

  public constructor() {
    this.r = 0;
    this.x = this.y = 1.5;
    this.vx = this.vy = 0;
  }

  public update(delta: number) {
    if (!testMap) {
      return;
    }
    // FIXME: proper collision detection
    const nowX = this.x;
    const nowY = this.y;
    const newX = this.x + this.vx * delta;
    const newY = this.y + this.vy * delta;
    if (!testMap.testBlockAt(newX, nowY)) {
      this.x = newX;
    }
    if (!testMap.testBlockAt(nowX, newY)) {
      this.y = newY;
    }
  }

  public acknowledgeUpdatePacket(x: number, y: number, rot: number, keys: number): boolean {
    const dist = Math.pow(x - this.x, 2) + Math.pow(y - this.y, 2);
    if (dist > 0.25) {
      // the player has moved way too much between updates
      return false;
    }

    // FIXME: limit angle between updates
    /*
    let minTheta = Math.min(this.r, rot) / Math.PI;
    let maxTheta = Math.max(this.r, rot) / Math.PI;
    if (maxTheta - minTheta > 1.0) {
      maxTheta -= 1.0;
    }
    if (maxTheta - minTheta > 0.25) {
      // The player has rotate too much between updates
      return false;
    }
    */

    this.x = x;
    this.y = y;

    const longDir = ((keys & 0x00ff) !== 0 ? 1 : 0) - ((keys & 0xff00) !== 0 ? 1 : 0);
    keys >>= 16;
    const sideDir = ((keys & 0x00ff) !== 0 ? 1 : 0) - ((keys & 0xff00) !== 0 ? 1 : 0);

    const longDirX = Math.cos(rot);
    const longDirY = Math.sin(rot);

    const sideDirX = -longDirY;
    const sideDirY = +longDirX;

    const dirX = longDirX * longDir + sideDirX * sideDir;
    const dirY = longDirY * longDir + sideDirY * sideDir;

    this.vx = dirX * PLAYER_RUN_SPEED;
    this.vy = dirY * PLAYER_RUN_SPEED;

    if (longDir !== 0 && sideDir !== 0) {
      this.vx *= Math.SQRT1_2;
      this.vy *= Math.SQRT1_2;
    }

    return true;
  }

  public getR(): number {
    return this.r;
  }

  public getX(): number {
    return this.x;
  }

  public getY(): number {
    return this.y;
  }

  public getVX(): number {
    return this.vx;
  }

  public getVY(): number {
    return this.vy;
  }
}

const SPKT_BASE_SIZES = new Array<number>(ServerPacketType.SPKT_MAX);
SPKT_BASE_SIZES[ServerPacketType.SPKT_HELLO] = 4 + 4 * 2;
SPKT_BASE_SIZES[ServerPacketType.SPKT_UPDATE] = 1 + 4 + 4 * 2 + 4 * 2;
SPKT_BASE_SIZES[ServerPacketType.SPKT_BYE] = 0;

class Packet {
  private bytes: Uint8Array;
  private view: DataView;
  private offset: number;
  private header: number;

  public static alloc(type: ServerPacketType, extraSize?: number): Packet | null {
    if (type >= ServerPacketType.SPKT_MAX) {
      Logger.error("Invalid server packet type %o", type);
      return null;
    }
    
    const packetSize = 4 + SPKT_BASE_SIZES[type] + (extraSize ?? 0);

    return new Packet(type, packetSize);
  }

  private constructor(type: ServerPacketType, size: number) {
    this.bytes = new Uint8Array(size);
    this.view = new DataView(this.bytes.buffer);
    this.offset = 4;
    this.header = (type & 3) << 30;
  }

  public getRaw(n: number): ArrayBuffer {
    this.view.setUint32(0, this.header | (n & 0x3FFFFFFF), true);
    return this.bytes;
  }

  public pushU8(value: number) {
    this.view.setUint8(this.offset, value);
    this.offset += 1;
  }

  public pushU32(value: number) {
    this.view.setUint32(this.offset, value, true);
    this.offset += 4;
  }

  public pushF32(value: number) {
    this.view.setFloat32(this.offset, value, true);
    this.offset += 4;
  }

  public pushBytes(mapData: Uint8Array) {
    this.bytes.set(mapData, this.offset);
    this.offset += mapData.byteLength;
  }
}

/**
 * @param numOfSteps: Total number steps to get color, means total colors
 * @param step: The step number, means the order of the color
 */
function rainbow(numOfSteps: number, step: number): number {
  // This function generates vibrant, "evenly spaced" colours (i.e. no clustering). This is ideal for creating easily distinguishable vibrant markers in Google Maps and other apps.
  // Adam Cole, 2011-Sept-14
  // HSV to RBG adapted from: http://mjijackson.com/2008/02/rgb-to-hsl-and-rgb-to-hsv-color-model-conversion-algorithms-in-javascript
  let r, g, b;
  let h = step / numOfSteps;
  let i = ~~(h * 6);
  let f = h * 6 - i;
  let q = 1 - f;
  switch(i % 6){
    case 0: r = 1; g = f; b = 0; break;
    case 1: r = q; g = 1; b = 0; break;
    case 2: r = 0; g = 1; b = f; break;
    case 3: r = 0; g = q; b = 1; break;
    case 4: r = f; g = 0; b = 1; break;
    case 5: r = 1; g = 0; b = q; break;
    default: r = g = b = 0; break;
  }
  r = (~ ~(r * 255)) & 0xFF;
  g = (~ ~(g * 255)) & 0xFF;
  b = (~ ~(b * 255)) & 0xFF;
  return (r << 16) | (g << 8) | (b);
}

class PlayerConnection {
  private ws: WebSocket
  private gameId: number | undefined;
  private playerId: number | undefined;
  private playerToken: number | undefined;
  private sequence: number | undefined;
  public tracker: PlayerTracker | undefined;

  public sendPacket(pkt: Packet) {
    if (this.sequence == null) {
      // huh?
      return;
    }
    const seq = this.sequence++;
    this.ws.send(pkt.getRaw(seq));
  }
  
  private handlePacket(type: GamePacketType, sequence: number, playerToken: number, view: DataView) {
    if (this.sequence != null && Math.abs(sequence - this.sequence) > MAX_PACKET_DROP) {
      // invalid packet sequence
      Logger.error("Expected sequence number %d, got %d", this.sequence, sequence);
      return;
    } else if (this.sequence == null || this.sequence <= sequence) {
      this.sequence = sequence + 1;
    }

    switch (type) {
      case GamePacketType.GPKT_JOIN: {
        if (this.playerToken != null) {
          Logger.error("Player sent hello packet, after it had already sent one!");
          return;
        }
        if (sequence != 0) {
          Logger.warning("Sequence number for player %s's JOIN packet is not 0", playerToken.toString(16));
        }

        const gameId = view.getUint32(8, true);
        Logger.info("Got request to join game with ID: %s", gameId.toString(16));

        const playersList = getGameByID(gameId);
        if (!playersList) {
          return;
        }

        this.gameId = gameId;
        this.playerToken = playerToken;
        this.tracker = new PlayerTracker();
        this.playerId = playersList.push(this) - 1;

        Logger.info("Player (%s) joined game (%s) with ID %d", this.playerToken.toString(16), this.gameId.toString(16), this.playerId);

        // make TS happy :)
        if (!testMap) {
          return;
        }

        const mapData = testMap.getCompressedData();

        const nSprites = playersList.reduce((count, player) => (player ? count + 1 : count), 0);
        const extraSize = mapData.byteLength + nSprites * (4 + 4 * 2);
        const pkt = Packet.alloc(ServerPacketType.SPKT_HELLO, extraSize);
        if (!pkt) {
          return;
        }

        pkt.pushU32((this.playerId << 24) | (nSprites & 0x00FFFFFF))
        pkt.pushU32(testMap.getWidth());
        pkt.pushU32(testMap.getHeight());
        playersList.forEach(player => {
          if (!player || player.playerId == null || !player.tracker) {
            return;
          }
          const color = rainbow(32, Math.floor(Math.random() * 32));
          const idAndColor = (player.playerId << 24) | color;
          pkt.pushU32(idAndColor);
          pkt.pushF32(player.tracker.getX());
          pkt.pushF32(player.tracker.getY());
        });
        pkt.pushBytes(mapData);

        this.sendPacket(pkt);
        break;
      }
  
      case GamePacketType.GPKT_LEAVE: {
        Logger.info("Got request to leave game");
        if (this.gameId == null) {
          Logger.error("Player is not currently in any game!");
          return;
        }
        break;
      }
  
      case GamePacketType.GPKT_UPDATE: {
        Logger.info("Got update packet");
        if (!this.tracker || this.gameId == null || this.playerId == null || this.playerToken == null) {
          Logger.error("Connection not initialized! Did we miss the JOIN packet?");
          return;
        }

        const x = view.getFloat32(8, true);
        const y = view.getFloat32(12, true);
        const rot = view.getFloat32(16, true);
        const keys = view.getUint32(20, true);

        const includePlayer = !this.tracker.acknowledgeUpdatePacket(x, y, rot, keys);

        const playersList = getGameByID(this.gameId);
        if (!playersList) {
          return;
        }

        // generate sprite_update structure
        const pkt = Packet.alloc(ServerPacketType.SPKT_UPDATE);
        if (!pkt) {
          return;
        }
        pkt.pushU8(this.playerId);
        pkt.pushF32(this.tracker.getR());
        pkt.pushF32(this.tracker.getX());
        pkt.pushF32(this.tracker.getY());
        pkt.pushF32(this.tracker.getVX());
        pkt.pushF32(this.tracker.getVY());

        playersList.forEach(player => {
          if (player && (player.playerId !== this.playerId || includePlayer))
            player.sendPacket(pkt);
        });
        break;
      }
    }  
  }

  public constructor(ws: WebSocket) {
    this.ws = ws;
    this.ws.on("error", Logger.error);
    this.ws.on("close", () => {
      Logger.info("Client disconnected");
      if (this.playerId != null && this.gameId != null) {
        Logger.info("Removing player with ID %d from game %s", this.playerId, this.gameId.toString(16));
        const playersList = getGameByID(this.gameId);
        if (playersList) {
          playersList[this.playerId] = null;
          // if (playersList.reduce((isEmpty, player) => isEmpty && !player, true)) {
          // }
        }
      } else {
        Logger.info("Client was not in game, nothing to do");
      }
    });
    this.ws.on("message", (data, isBinary) => {
      Logger.info("Data is binary: %s", isBinary);
      Logger.info("Data is: %O", data);
      if (!isBinary || !(data instanceof Buffer)) {
        return;
      }
      const view = new DataView(data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength));
      
      const typeAndSeq = view.getUint32(0, true);
      const type = (typeAndSeq >> 30) & 3;
      const sequence = (typeAndSeq & 0x3FFFFFFF);

      const playerToken = view.getUint32(4, true);
      
      Logger.info("Packet type: %s", type.toString(16));
      Logger.info("Sequence number: %s", sequence);
      Logger.info("Player token: %s", playerToken.toString(16));

      this.handlePacket(type, sequence, playerToken, view);
    });
  }
}

wss.on("connection", ws => {
  Logger.info("Player connected");
  new PlayerConnection(ws)
});

// update loop
let timestamp = performance.now();
setInterval(() => {
  const newTimestamp = performance.now();
  const delta = (newTimestamp - timestamp) / 1000.0;
  for (let playerList of games.values()) {
    playerList.forEach(player => player?.tracker?.update(delta))
  }
  timestamp = newTimestamp;
}, 1000 / 100);
