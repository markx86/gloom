import Logger from "./logger";
import { WebSocket } from "ws";
import { Game, PlayerHolder, PlayerSprite } from "./game";
import { GamePacketType, ServerPacket, HelloPacket, UpdatePacket, DestroyPacket, GamePacket, WaitPacket, TerminatePacket } from "./packet";
import { Peer } from "./broadcast";

const MAX_PACKET_DROP = 10;

export class Client extends Peer implements PlayerHolder {
  private token: number;
  private player: PlayerSprite | null;
  private ws: WebSocket;
  private clientSequence: number;
  private serverSequence: number;

  public unsetPlayer(): void {
    this.player = null;
  }

  public getToken(): number {
    return this.token;
  }

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

    if (this.token !== playerToken) {
      Logger.error("Invalid player token! Got %s expected %s", playerToken.toString(16), this.token?.toString(16));
      return false;
    } else if (type !== GamePacketType.JOIN && !this.inBroadcastGroup()) {
      Logger.error("Player is not in any game!");
      return false;
    }
    
    return true;
  }

  private _handleJoinPacket(packet: GamePacket): boolean {
    if (this.player != null) {
      Logger.error("Player sent hello packet, after it had already sent one!");
      return false;
    }

    const gameId = packet.popU32();
    Logger.trace("Got request to join game with ID: %s", gameId.toString(16));

    const game = Game.getById(gameId);
    if (game == null) {
      Logger.error("No game with that ID");
      return false;
    }

    const player = game.newPlayer(this);
    if (player == null) {
      return false;
    }

    this.player = player;

    Logger.success("Client (%s) joined game (%s) with ID %d", this.token.toString(16), this.player.game.id.toString(16), this.player.id);

    this.registerToBroadcastGroup(game.id);
    this.sendPacket(new HelloPacket(this.player));
    this.sendPacket(new WaitPacket(game));

    return true;
  }

  private handleJoinPacket(packet: GamePacket) {
    if (!this._handleJoinPacket(packet)) {
      this.sendPacket(new TerminatePacket());
    }
  }

  private handleLeavePacket(_packet: GamePacket) {
    Logger.trace("Got request to leave game");
    if (!this.inBroadcastGroup()) {
      // NOTE: unreachable
      return;
    }

    this.removeFromBroadcastGroup();
    this.player?.game.removePlayer(this.player);
    this.unsetPlayer();
  }

  private handleUpdatePacket(packet: GamePacket) {
    Logger.trace("Got update packet");
    if (this.player == null) {
      // NOTE: unreachable
      return;
    }

    const ts   = packet.popF32();
    const x    = packet.popF32();
    const y    = packet.popF32();
    const rot  = packet.popF32();
    const keys = packet.popU32();
    Logger.trace("pos = (%f, %f), rot = %f, keys = %s", x, y, rot, keys.toString(16));

    const [ack, delta] = this.player.acknowledgeUpdatePacket(ts, x, y, rot, keys);
    Logger.trace("delta = %f, ack = %s", delta, ack);

    this.broadcastPacket(
      new UpdatePacket(this.player),
      !ack
    );

    if (ack) {
      this.player.tick(delta);
    } else {
      Logger.warning("Update packet not acknowledged");
    }
  }

  private handleFirePacket(_packet: GamePacket) {
    Logger.trace("Got fire packet");
    if (this.player == null) {
      // NOTE: unreachable
      return;
    }

    this.player.fireBullet();
  }

  private handlePacket(type: GamePacketType, sequence: number, playerToken: number, packet: GamePacket) {
    if (!this.checkPacket(type, sequence, playerToken)) {
      return;
    }

    switch (type) {
      case GamePacketType.JOIN:   { this.handleJoinPacket(packet); break; }
      case GamePacketType.LEAVE:  { this.handleLeavePacket(packet); break; }
      case GamePacketType.UPDATE: { this.handleUpdatePacket(packet); break; }
      case GamePacketType.FIRE:   { this.handleFirePacket(packet); break; }
    }
  }

  public constructor(ws: WebSocket, token: number) {
    super();

    this.ws = ws;
    this.token = token;
    this.player = null;
    this.serverSequence = this.clientSequence = 0;

    this.ws.on("error", Logger.error);

    // handle WebSocket close event
    this.ws.on("close", () => {
      Logger.trace("Client disconnected");
      if (this.player?.game) {
        Logger.success("Removing player with ID %d from game %s", this.player.id, this.player.game.id.toString(16));
        this.player.game.removePlayer(this.player);
        this.broadcastPacket(new DestroyPacket(this.player), false);
      } else {
        Logger.trace("Client was not in game, nothing to do");
      }
    });

    // handle WebSocket message event
    this.ws.on("message", (data, isBinary) => {
      Logger.trace("Data is binary: %s", isBinary);
      Logger.trace("Data is: %O", data);
      if (!isBinary || !(data instanceof Buffer)) {
        return;
      }
      const packet = new GamePacket(data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength));
      
      const typeAndSeq = packet.popU32();
      const type = (typeAndSeq >> 30) & 3;
      const sequence = (typeAndSeq & 0x3FFFFFFF);

      const playerToken = packet.popU32();
      
      Logger.trace("Packet type: %s", type.toString(16));
      Logger.trace("Sequence number: %s", sequence);
      Logger.trace("Player token: %s", playerToken.toString(16));

      this.handlePacket(type, sequence, playerToken, packet);
    });
  }
}
