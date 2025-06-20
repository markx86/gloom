import { BulletSprite, Game, GameSprite, PlayerSprite } from "./game";

export enum GamePacketType {
  READY,
  LEAVE,
  UPDATE,
  FIRE,
  MAX
};

export enum ServerPacketType {
  HELLO,
  UPDATE,
  CREATE,
  DESTROY,
  WAIT,
  TERMINATE,
  MAX
}

export class GamePacket {
  private offset: number;
  private view: DataView;

  public constructor(data: ArrayBuffer) {
    this.offset = 0;
    this.view = new DataView(data);
  }

  private ensureData(size: number) {
    if (this.offset + size > this.view.byteLength) {
      throw new Error("Out of bounds in packet");
    }
  }

  public popF32(): number {
    this.ensureData(4);
    const value = this.view.getFloat32(this.offset, true);
    this.offset += 4;
    return value;
  }

  public popU32(): number {
    this.ensureData(4);
    const value = this.view.getUint32(this.offset, true);
    this.offset += 4;
    return value;
  }

  public popU8(): number {
    this.ensureData(1);
    const value = this.view.getUint8(this.offset);
    this.offset += 1;
    return value;
  }
}

export abstract class ServerPacket {
  private bytes: Uint8Array;
  private view: DataView;
  private offset: number;
  private header: number;

  protected constructor(type: ServerPacketType, extraSize: number = 0) {
    if (type >= ServerPacketType.MAX) {
      throw new Error("Invalid server packet type: " + type);
    }
    this.bytes = new Uint8Array(extraSize + 4); // 4 is the size of the header in bytes
    this.view = new DataView(this.bytes.buffer);
    this.offset = 4;
    this.header = (type & 7) << 29;
  }

  public getRaw(n: number): ArrayBuffer {
    this.view.setUint32(0, this.header | (n & 0x1FFFFFFF), true);
    return this.bytes;
  }

  private ensureSpace(size: number) {
    if (this.offset + size > this.view.byteLength) {
      throw new Error("Out of bounds in packet");
    }
  }

  protected pushU8(value: number) {
    this.ensureSpace(1);
    this.view.setUint8(this.offset, value);
    this.offset += 1;
  }

  protected pushU32(value: number) {
    this.ensureSpace(4);
    this.view.setUint32(this.offset, value, true);
    this.offset += 4;
  }

  protected pushVec2U(x: number, y: number) {
    this.pushU32(x);
    this.pushU32(y);
  }

  protected pushF32(value: number) {
    this.ensureSpace(4);
    this.view.setFloat32(this.offset, value, true);
    this.offset += 4;
  }

  protected pushVec2F(x: number, y: number) {
    this.pushF32(x);
    this.pushF32(y);
  }

  protected pushBytes(bytes: Uint8Array) {
    this.ensureSpace(bytes.byteLength);
    this.bytes.set(bytes, this.offset);
    this.offset += bytes.byteLength;
  }

  protected pushSpriteTransform(sprite: GameSprite) {
    this.pushF32(sprite.getR());
    this.pushVec2F(sprite.getX(), sprite.getY());
    this.pushVec2F(sprite.getVX(), sprite.getVY());
  }

  protected pushSpriteInit(sprite: GameSprite) {
    this.pushSpriteDescriptor(sprite);
    this.pushSpriteTransform(sprite);
  }

  protected pushSpriteUpdate(sprite: GameSprite) {
    this.pushU8(sprite.id);
    this.pushSpriteTransform(sprite);
  }

  protected pushSpriteDescriptor(sprite: GameSprite, actor: GameSprite | undefined = undefined) {
    const owner = (sprite instanceof BulletSprite) ? sprite.owner.id : 0;
    const coll = actor?.id ?? 0;
    const desc =
      ((coll & 0xff) << 24)
      | ((owner & 0xff) << 16)
      | ((sprite.id & 0xff) << 8)
      | (sprite.type & 0xff);
    this.pushU32(desc);
  }
}

const SIZEOF_STRUCT_SPRITE_INIT =
    1      // type
  + 1      // id
  + 1      // owner
  + 1      // generic field
  + 4      // rotation
  + 4 * 2  // position
  + 4 * 2; // velocity
const SIZEOF_STRUCT_SPRITE_UPDATE =
    1      // sprite id
  + 4      // rotation
  + 4 * 2  // position
  + 4 * 2; // velocity

export class HelloPacket extends ServerPacket {
  public constructor(player: PlayerSprite) {
    const game = player.game;
    const size =
        1                                                // number of sprites
      + 1                                                // this sprite id
      + 4 * 2                                            // map size (width and height)
      + game.map.getSizeInBytes()                        // size of map data
      + game.sprites.length * SIZEOF_STRUCT_SPRITE_INIT; // size of sprite data
    super(ServerPacketType.HELLO, size)
    this.pushU8(game.sprites.length);
    this.pushU8(player.id);
    this.pushVec2U(game.map.width, game.map.height);
    game.sprites.forEach(sprite => this.pushSpriteInit(sprite));
    this.pushBytes(game.map.getCompressedData());
  }
}

export class UpdatePacket extends ServerPacket {
  public constructor(sprite: GameSprite, timestamp: number) {
    super(ServerPacketType.UPDATE, 4 + SIZEOF_STRUCT_SPRITE_UPDATE)
    this.pushF32(timestamp);
    this.pushSpriteUpdate(sprite);
  }
}

export class CreatePacket extends ServerPacket {
  public constructor(sprite: GameSprite) {
      super(ServerPacketType.CREATE, SIZEOF_STRUCT_SPRITE_INIT);
      this.pushSpriteInit(sprite);
  }
}

export class DestroyPacket extends ServerPacket {
  public constructor(sprite: GameSprite, actor: GameSprite | undefined = undefined) {
    super(ServerPacketType.DESTROY, 4);
    this.pushSpriteDescriptor(sprite, actor);
  }
}

export class WaitPacket extends ServerPacket {
  public constructor(game: Game) {
      super(ServerPacketType.WAIT, 4);
      const timeLeft = Math.floor(game.getWaitTime()) & 0x7FFFFFFF;
      const timeAndWaiting = timeLeft | ((game.isWaiting() ? 1 : 0) << 31)
      this.pushU32(timeAndWaiting);
  }
}

export class TerminatePacket extends ServerPacket {
  public constructor() { super(ServerPacketType.TERMINATE); }
}
