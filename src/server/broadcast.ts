import { ServerPacket } from "./packet"

export abstract class Peer {
  private broadcastGroup: BroadcastGroup | undefined;

  protected registerToBroadcastGroup(gameId: number) {
    this.broadcastGroup = BroadcastGroup.get(gameId);
    this.broadcastGroup.add(this);
  }

  protected removeFromBroadcastGroup() {
    if (this.broadcastGroup?.remove(this)) {
      this.broadcastGroup = undefined;
    }
  }

  protected broadcastPacket(packet: ServerPacket, includeSelf: boolean) {
    this.broadcastGroup?.send(packet, includeSelf ? null : this);
  }

  protected inBroadcastGroup(): boolean {
    return this.broadcastGroup != null;
  }

  public abstract sendPacket(pkt: ServerPacket): void;
}

export class BroadcastGroup {
  private peers: Array<Peer>;

  private static broadcastGroups = new Map<number, BroadcastGroup>();

  public constructor() {
    this.peers = new Array<Peer>();
  }

  public add(socket: Peer) {
    this.peers.push(socket);
  }

  public remove(socket: Peer): boolean {
    const index = this.peers.indexOf(socket);
    if (index < 0) {
      return false;
    }
    this.peers.splice(index, 1);
    return true;
  }

  public send(pkt: ServerPacket, self: Peer | null = null) {
    this.peers.forEach(socket => {
      if (self !== socket) {
        socket.sendPacket(pkt);
      }
    })
  }

  public static get(gameId: number): BroadcastGroup {
    const group = this.broadcastGroups.get(gameId);
    if (!group) {
      this.broadcastGroups.set(gameId, new BroadcastGroup());
      return BroadcastGroup.get(gameId);
    }
    return group;
  }
}
