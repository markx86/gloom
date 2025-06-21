import Logger from "./logger";
import { WebSocket } from "ws";
import { Game, PlayerHolder, PlayerSprite } from "./game";
import { GamePacketType, ServerPacket, HelloPacket, UpdatePacket, DestroyPacket, GamePacket, WaitPacket } from "./packet";
import { Peer } from "./broadcast";

const MAX_PACKET_DROP = 10;

export class Client extends Peer implements PlayerHolder {
  private gameId: number;
  private playerToken: number;
  private player: PlayerSprite | null;
  private ws: WebSocket;
  private clientSequence: number;
  private serverSequence: number;

  public unsetPlayer(): void {
    this.player = null;
  }

  public getToken(): number {
    return this.playerToken;
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

    if (this.playerToken !== playerToken) {
      Logger.error("Invalid player token! Got %s expected %s", playerToken.toString(16), this.playerToken?.toString(16));
      return false;
    } else if (type !== GamePacketType.READY && !this.inBroadcastGroup()) {
      Logger.error("Player is not in any game!");
      return false;
    }
    
    return true;
  }

  private _handleReadyPacket(): boolean {
    if (this.player != null) {
      Logger.error("Player sent ready packet, after it had already sent one!");
      return false;
    }

    const game = Game.getById(this.gameId);
    if (game == null) {
      Logger.error("No game with that ID");
      return false;
    }

    const player = game.newPlayer(this);
    if (player == null) {
      return false;
    }

    this.player = player;

    Logger.success("Client (%s) joined game (%s) with ID %d", this.playerToken.toString(16), this.player.game.id.toString(16), this.player.id);

    this.registerToBroadcastGroup(game.id);
    this.sendPacket(new HelloPacket(this.player));
    this.sendPacket(new WaitPacket(game));

    return true;
  }

  private handleReadyPacket(_packet: GamePacket) {
    if (!this._handleReadyPacket()) {
      this.ws.close();
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

    const keys = packet.popU32();
    const rot  = packet.popF32();
    const ts = packet.popF32();
    Logger.trace("keys = %s, rot = %f, ts = %f", keys.toString(16), rot, ts);

    this.player.processUpdatePacket(keys, rot);
    this.broadcastPacket(new UpdatePacket(this.player, ts), true);
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
      case GamePacketType.READY:   { this.handleReadyPacket(packet); break; }
      case GamePacketType.LEAVE:  { this.handleLeavePacket(packet); break; }
      case GamePacketType.UPDATE: { this.handleUpdatePacket(packet); break; }
      case GamePacketType.FIRE:   { this.handleFirePacket(packet); break; }
    }
  }

  public constructor(ws: WebSocket, playerToken: number, gameId: number) {
    super();

    this.ws = ws;
    this.playerToken = playerToken;
    this.gameId = gameId;
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
        Logger.trace("Client was not in game, deallocating player");
        Game.getById(this.gameId)?.deallocatePlayer(this.playerToken);
      }
      this.unsetPlayer();
      this.removeFromBroadcastGroup();
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
