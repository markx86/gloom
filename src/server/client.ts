import Logger from "./logger";
import { WebSocket } from "ws";
import { PlayerSprite } from "./sprite";
import { GamePacketType, ServerPacket, HelloPacket, UpdatePacket, DestroyPacket, GamePacket, WaitPacket } from "./packet";
import { Peer } from "./broadcast";

const MAX_PACKET_DROP = 10;

export class Client extends Peer {
  private player: PlayerSprite;
  private ws: WebSocket;
  private clientSequence: number;
  private serverSequence: number;

  public sendPacket(pkt: ServerPacket) {
    const seq = this.serverSequence++;
    this.ws.send(pkt.getRaw(seq));
  }

  private checkPacket(type: GamePacketType, sequence: number, playerToken: number): boolean {
    if (type >= GamePacketType.MAX) {
      // invalid packet type
      Logger.error("Invalid client packet type");
      return false;
    }

    if (sequence >= this.clientSequence && sequence - this.clientSequence < MAX_PACKET_DROP) {
      this.clientSequence = sequence + 1;
    } else {
      // invalid packet sequence
      Logger.error("Expected sequence number %d, got %d", this.clientSequence, sequence);
      return false;
    }

    if (this.player.token !== playerToken) {
      Logger.error("Invalid player token! Got %s expected %s", playerToken.toString(16), this.player.token.toString(16));
      return false;
    } else if (type !== GamePacketType.READY && !this.inBroadcastGroup()) {
      Logger.error("Player is not in any game!");
      return false;
    }
    
    return true;
  }

  private handleReadyPacket(packet: GamePacket) {
    const ready = packet.popU8() != 0;
    this.player.setReady(ready);
  }

  private handleLeavePacket(_packet: GamePacket) {
    Logger.trace("Got request to leave game");
    this.ws.close();
  }

  private handleUpdatePacket(packet: GamePacket) {
    // discard update packets if the player is dead
    if (this.player.getHealth() <= 0) {
      Logger.trace("Player is dead %d (token: %s), discarding update packet", this.player.id, this.player.token.toString(16));
      return;
    }
    // discard update packets if the game hasn't started
    if (!this.player.game.isPlaying()) {
      Logger.trace("Game hasn't started, discarding update packet for player %d (token: %s)", this.player.id, this.player.token.toString(16));
      return;
    }

    const keys = packet.popU32();
    const rot  = packet.popF32();
    const ts = packet.popF32();
    Logger.trace("keys = %s, rot = %f, ts = %f", keys.toString(16), rot, ts);

    this.player.processUpdatePacket(keys, rot);
    this.broadcastPacket(new UpdatePacket(this.player, ts), true);
  }

  private handleFirePacket(_packet: GamePacket) {
    Logger.trace("Got fire packet");
    this.player.fireBullet();
  }

  private handlePacket(type: GamePacketType, sequence: number, playerToken: number, packet: GamePacket) {
    if (!this.checkPacket(type, sequence, playerToken)) {
      return;
    }

    switch (type) {
      case GamePacketType.READY:  {
        this.handleReadyPacket(packet);
        // if this is the first ready packet we receive from the client,
        // assume that they're ready to receive map data
        if (sequence === 0) {
          Logger.trace("Sending HELLO to player %s", this.player.token.toString(16));
          this.sendPacket(new HelloPacket(this.player));
          this.sendPacket(new WaitPacket(this.player.game));
        }
        break;
      }
      case GamePacketType.LEAVE:  { this.handleLeavePacket(packet); break; }
      case GamePacketType.UPDATE: { this.handleUpdatePacket(packet); break; }
      case GamePacketType.FIRE:   { this.handleFirePacket(packet); break; }
    }
  }

  public constructor(ws: WebSocket, player: PlayerSprite) {
    super();

    this.ws = ws;
    this.player = player;
    this.serverSequence = this.clientSequence = 0;

    this.ws.on("error", Logger.error);

    // handle WebSocket close event
    this.ws.on("close", () => {
      const game = this.player.game;
      if (game.isWaiting() || game.isReady()) {
        game.removePlayer(this.player);
      }
      this.removeFromBroadcastGroup();
    });

    // handle WebSocket message event
    this.ws.on("message", (data, isBinary) => {
      Logger.trace("Data is binary: %s", isBinary);
      Logger.trace("Data is: %O", data);
      if (!isBinary || !(data instanceof Buffer)) {
        return;
      }
      try {
        const packet = new GamePacket(data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength));

        const typeAndSeq = packet.popU32();
        const type = (typeAndSeq >> 30) & 3;
        const sequence = (typeAndSeq & 0x3FFFFFFF);

        const playerToken = packet.popU32();

        Logger.trace("Packet type: %s", type.toString(16));
        Logger.trace("Sequence number: %s", sequence);
        Logger.trace("Player token: %s", playerToken.toString(16));

        this.handlePacket(type, sequence, playerToken, packet);
      } catch (e) {
        Logger.error("Unhandled exception when parsing packet for player #%s", this.player.id.toString(16));
        Logger.error(e);
      }
    });

    this.registerToBroadcastGroup(this.player.game.id);
  }
}
