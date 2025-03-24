import Logger from "./logger";
import { WebSocket } from "ws";
import { Game, PlayerSprite } from "./game";
import { GamePacketType, Packet, HelloPacket, UpdatePacket, CreatePacket, DestroyPacket } from "./packet";

const MAX_PACKET_DROP = 10;

export class Client {
  private token: number | undefined;
  private player: PlayerSprite | undefined;
  private broadcastGroup: Array<Client> | undefined;
  private ws: WebSocket;
  private clientSequence: number;
  private serverSequence: number;

  private static broadcastGroups = new Map<number, Array<Client>>();

  private registerToBroadcastGroup(gameId: number) {
    const group = Client.broadcastGroups.get(gameId);
    if (!group) {
      Client.broadcastGroups.set(gameId, new Array<Client>());
      // retry or something, idk ¯\_(ツ)_/¯
      this.registerToBroadcastGroup(gameId);
      return;
    }
    group.push(this);
    this.broadcastGroup = group;
  }

  private removeFromBroadcastGroup() {
    if (!this.broadcastGroup) {
      Logger.warning("Tried to remove client from broadcast group, but the client is not in any group!")
      return;
    }
    const index = this.broadcastGroup.indexOf(this);
    if (index >= 0) {
      this.broadcastGroup.splice(index, 1);
      this.broadcastGroup = undefined; // reset the broadcast group
    }
  }

  private broadcastPacket(pkt: Packet, includeSelf: boolean = false) {
    if (!this.broadcastGroup) {
      Logger.warning("Tried to broadcast even though the client is not registered to any group!");
      return;
    }
    this.broadcastGroup.forEach(peer => {
      if (!includeSelf && peer === this) {
        return;
      }
      peer.sendPacket(pkt);
    });
  }

  private sendPacket(pkt: Packet) {
    const seq = this.serverSequence++;
    this.ws.send(pkt.getRaw(seq));
  }

  public getPlayer(): PlayerSprite | undefined {
    return this.player;
  }

  public getToken(): number | undefined {
    return this.token;
  }
  
  private handlePacket(type: GamePacketType, sequence: number, playerToken: number, view: DataView) {
    if (sequence >= this.clientSequence && sequence - this.clientSequence < MAX_PACKET_DROP) {
      this.clientSequence = sequence + 1;
    } else {
      // invalid packet sequence
      Logger.error("Expected sequence number %d, got %d", this.clientSequence, sequence);
      return;
    }

    if (type >= GamePacketType.GPKT_MAX) {
      // invalid packet type
      Logger.error("Invalid client packet type");
      return;
    }

    switch (type) {
      case GamePacketType.GPKT_JOIN: {
        if (this.token != null) {
          Logger.error("Player sent hello packet, after it had already sent one!");
          return;
        }
        if (sequence != 0) {
          Logger.warning("Sequence number for player %s's JOIN packet is not 0", playerToken.toString(16));
        }

        const gameId = view.getUint32(8, true);
        Logger.info("Got request to join game with ID: %s", gameId.toString(16));

        const game = Game.getByID(gameId);
        if (!game) {
          return;
        }

        const player = game.newPlayer();
        if (!player) {
          return;
        }

        this.token = playerToken;
        this.player = player;

        Logger.info("Client (%s) joined game (%s) with ID %d", this.token.toString(16), this.player.game.id.toString(16), this.player.id);

        this.registerToBroadcastGroup(game.id);
        this.sendPacket(new HelloPacket(this.player.id, game));
        this.broadcastPacket(new CreatePacket(this.player), false);
        break;
      }
  
      case GamePacketType.GPKT_LEAVE: {
        Logger.info("Got request to leave game");
        if (!this.player) {
          Logger.error("Client is not currently in any game!");
          return;
        }
        this.broadcastPacket(new DestroyPacket(this.player));
        this.removeFromBroadcastGroup();
        this.player.game.removePlayer(this.player);
        this.player = undefined;
        this.token = undefined;
        break;
      }
  
      case GamePacketType.GPKT_UPDATE: {
        Logger.info("Got update packet");
        if (!this.player || this.token == null) {
          Logger.error("Connection not initialized! Did we miss the JOIN packet?");
          break;
        }

        const x = view.getFloat32(8, true);
        const y = view.getFloat32(12, true);
        const rot = view.getFloat32(16, true);
        const keys = view.getUint32(20, true);
        Logger.info("pos = (%f, %f), rot = %f, keys = %s", x, y, rot, keys.toString(16));

        const includeSelf = !this.player.acknowledgeUpdatePacket(x, y, rot, keys);

        this.broadcastPacket(
          new UpdatePacket(this.player),
          includeSelf
        );
        break;
      }
    }
  }

  public constructor(ws: WebSocket) {
    this.ws = ws;
    this.serverSequence = this.clientSequence = 0;

    this.ws.on("error", Logger.error);

    // handle WebSocket close event
    this.ws.on("close", () => {
      Logger.info("Client disconnected");
      if (this.player?.game) {
        Logger.info("Removing player with ID %d from game %s", this.player.id, this.player.game.id.toString(16));
        this.player.game.removePlayer(this.player);
        this.broadcastPacket(new DestroyPacket(this.player), false);
      } else {
        Logger.info("Client was not in game, nothing to do");
      }
    });

    // handle WebSocket message event
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
