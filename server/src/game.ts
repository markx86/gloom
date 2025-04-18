import { BroadcastGroup } from "./broadcast";
import Logger from "./logger";
import { CreatePacket, DestroyPacket } from "./packet";

const MAX_PLAYERS = 4;
const MAX_SPRITES = 256;
const SPRITE_RADIUS = 0.15;
const PLAYER_RUN_SPEED = 3.5;
const PLAYER_HEALTH = 100;
const PLAYER_RELOAD_TIME = 0.25;
const BULLET_INITIAL_SPEED = 10;
const BULLET_DAMAGE = 25;
const COLL_DOF = 8
const POS_DIFF_THRESHOLD = 0.5;

enum GameSpriteType {
  PLAYER,
  BULLET
}

function traceRay(x: number, y: number, dirX: number, dirY: number, map: GameMap) {
  let mapX = Math.floor(x);
  let mapY = Math.floor(y);

  const dposX = x - mapX;
  const dposY = y - mapY;

  const deltaDistY = Math.abs(1.0 / dirX);
  const deltaDistX = Math.abs(1.0 / dirY);

  const distX = dirX > 0 ? (1.0 - dposX) : dposX;
  const distY = dirY > 0 ? (1.0 - dposY) : dposY;

  let intersecDistY = deltaDistY * distX;
  let intersecDistX = deltaDistX * distY;

  const stepDirX = (dirX < 0) ? -1 : +1;
  const stepDirY = (dirY < 0) ? -1 : +1;

  let vertical = true;
  for (let d = 0; d < COLL_DOF; ++d) {
    if (mapX >= map.width || mapX < 0) {
      break;
    }
    if (mapY >= map.height || mapY < 0) {
      break;
    }

    if (map.testBlockAt(mapX, mapY)) {
      break;
    }

    if (intersecDistY < intersecDistX) {
      intersecDistY += deltaDistY;
      mapX += stepDirX;
      vertical = true;
    } else {
      intersecDistX += deltaDistX;
      mapY += stepDirY;
      vertical = false;
    }
  }

  const dist = vertical ? (intersecDistY - deltaDistY) : (intersecDistX - deltaDistX);

  return dist;
}

export abstract class GameSprite {
  readonly id: number;
  readonly type: GameSpriteType;
  readonly game: Game;
  protected x: number;
  protected y: number;
  protected rotation: number;
  protected velocity: number;
  protected dirX: number;
  protected dirY: number;

  public constructor(game: Game, id: number, type: GameSpriteType,
                     x: number, y: number, velocity: number, rotation: number = 0) {
    this.id = id;
    this.type = type;
    this.x = x;
    this.y = y;
    this.rotation = rotation;
    this.velocity = velocity;
    this.dirX = Math.cos(rotation);
    this.dirY = Math.sin(rotation);
    this.game = game;
  }

  protected abstract onWallCollision(): void;

  protected moveAndCollide(delta: number, x: number = this.x, y: number = this.y): readonly [boolean, number, number] {
    const space = this.velocity * delta;

    const signDirX = (this.dirX < 0) ? -1 : +1;
    const signDirY = (this.dirY < 0) ? -1 : +1;

    let vDist = Math.abs(this.dirY) * space;
    let hDist = Math.abs(this.dirX) * space;

    let collided = false;

    const vDistMax = traceRay(
      x, y,
      0, signDirY,
      this.game.map);
    if (vDistMax < vDist + SPRITE_RADIUS) {
      vDist = vDistMax - SPRITE_RADIUS;
      collided = true;
    }

    const hDistMax = traceRay(
      x, y,
      signDirX, 0,
      this.game.map);
    if (hDistMax < hDist + SPRITE_RADIUS) {
      hDist = hDistMax - SPRITE_RADIUS;
      collided = true;
    }

    x += signDirX * hDist;
    y += signDirY * vDist;
    
    return [collided, x, y];
  }

  public tick(delta: number) {
    const [collided, newX, newY] = this.moveAndCollide(delta);
    this.x = newX;
    this.y = newY;
    if (collided === true) {
      this.onWallCollision();
    }
  }

  public getR(): number {
    return this.rotation;
  }

  public getX(): number {
    return this.x;
  }

  public getY(): number {
    return this.y;
  }

  public getVX(): number {
    return this.dirX * this.velocity;
  }

  public getVY(): number {
    return this.dirY * this.velocity;
  }
}

export interface PlayerHolder {
  unsetPlayer(): void;
}

export class PlayerSprite extends GameSprite {
  private health: number;
  private reloadTime: number;
  private holder: PlayerHolder;

  public constructor(holder: PlayerHolder, game: Game, id: number,
                     x: number, y: number, r: number = 0) {
    super(game, id, GameSpriteType.PLAYER, x, y, 0, r);
    this.holder = holder;
    this.health = PLAYER_HEALTH;
    this.reloadTime = 0;
  }

  protected onWallCollision() {}

  private onSpriteCollision(other: GameSprite) {
    if (other instanceof BulletSprite && other.owner !== this) {
      this.game.removeSprite(other, this);
      this.health -= BULLET_DAMAGE;
      if (this.health <= 0) {
        Logger.info("Player %d killed by player %d", this.id, other.owner.id);
        this.game.removePlayer(this, other.owner);
        this.holder.unsetPlayer();
      }
    }
  }

  private distanceFrom(sprite: GameSprite): number {
    const dx = sprite.getX() - this.x;
    const dy = sprite.getY() - this.y;
    return Math.sqrt(dx * dx + dy * dy);
  }

  public override tick(delta: number) {
    super.tick(delta);
    if (this.reloadTime > 0) {
      this.reloadTime -= delta;
    }
    this.game.sprites.forEach(other => {
      if (this === other) {
        return;
      }
      if (this.distanceFrom(other) < SPRITE_RADIUS) {
        this.onSpriteCollision(other);
      }
    })
  }

  public acknowledgeUpdatePacket(ts: number, x: number, y: number, rotation: number, keys: number): [boolean, number] {
    const delta = this.game.getTime() - ts;

    const [_, predictedX, predictedY] = this.moveAndCollide(-delta);
    const dist = Math.sqrt(Math.pow(predictedX - x, 2) + Math.pow(predictedY - y, 2));
    Logger.info("Distance from prediction: %d", dist)
    const ack = dist <= POS_DIFF_THRESHOLD;

    // FIXME: limit angle between updates
  
    const longDir = ((keys & 0x00FF) !== 0 ? 1 : 0) - ((keys & 0xFF00) !== 0 ? 1 : 0);
    keys >>= 16;
    const sideDir = ((keys & 0x00FF) !== 0 ? 1 : 0) - ((keys & 0xFF00) !== 0 ? 1 : 0);

    const longDirX = Math.cos(rotation);
    const longDirY = Math.sin(rotation);

    const sideDirX = -longDirY;
    const sideDirY = +longDirX;

    this.dirX = longDirX * longDir + sideDirX * sideDir;
    this.dirY = longDirY * longDir + sideDirY * sideDir;

    if (longDir !== 0 && sideDir !== 0) {
      this.dirX *= Math.SQRT1_2;
      this.dirY *= Math.SQRT1_2;
    }
    this.velocity = (longDir !== 0 || sideDir !== 0) ? PLAYER_RUN_SPEED : 0;

    if (ack) {
      this.x = x;
      this.y = y;
      this.rotation = rotation;
    }

    return [ack, delta];
  }

  public fireBullet(): BulletSprite | undefined {
    if (this.reloadTime <= 0) {
      this.reloadTime = PLAYER_RELOAD_TIME;
      return this.game.newBullet(this);
    }
  }

  public getHealth(): number {
    return this.health;
  }
}

export class BulletSprite extends GameSprite {
  readonly owner: PlayerSprite;

  public constructor(player: PlayerSprite, id: number) {
    super(
      player.game, id, GameSpriteType.BULLET,
      player.getX(), player.getY(),
      BULLET_INITIAL_SPEED, player.getR()
    );
    this.owner = player;
  }

  protected onWallCollision() {
    console.log("[#] Bullet hit wall, destroying");
    this.game.removeSprite(this);
  }
}

export class GameMap {
  private compressed: Uint8Array | undefined;
  private tiles: Uint8Array;
  readonly width: number;
  readonly height: number;

  public constructor(width: number, height: number, tiles: ArrayLike<number>) {
    if (width * height != tiles.length) {
      throw new Error(`Trying to create map of size ${width}x${height} with only ${tiles.length} tiles!`);
    }
    this.width = width;
    this.height = height;
    this.tiles = new Uint8Array(tiles);
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

  public getSizeInBytes(): number {
    return ((this.tiles.length + 3) & ~3) >> 2;
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

export class Game {
  readonly id: number;
  readonly map: GameMap;
  readonly sprites: Array<GameSprite>;
  readonly broadcastGroup: BroadcastGroup;
  private numOfPlayers: number;
  private time: number;
  private deadSprites: Set<number>;

  private static games = new Map<number, Game>();

  public constructor(id: number, map: GameMap) {
    this.id = id;
    this.map = map;
    this.sprites = new Array<GameSprite>();
    this.deadSprites = new Set<number>();
    this.numOfPlayers = 0;
    this.time = 0;
    this.broadcastGroup = BroadcastGroup.get(id);
  }

  public static create(id: number, map: GameMap) {
    Game.games.set(id, new Game(id, map));
  }

  public static getByID(id: number): Game | null {
    const game = Game.games.get(id);
    if (!game) {
      Logger.error("No game exists with ID %s", id.toString(16));
      return null;
    }
    return game;
  }

  public static tickAll(delta: number) {
    Game.games.forEach(game => game.tick(delta));
  }

  private tick(delta: number) {
    this.time += delta;
    // Logger.info("Game time: %d (delta %d)", this.time, delta);
    this.sprites.forEach(sprite => sprite.tick(delta)); // tick the game world
    this.cleanupSprites();
  }

  public getSpriteByID(id: number): GameSprite | undefined {
    return this.sprites.filter(sprite => sprite.id === id).pop();
  }

  // NOTE: entity IDs start from 1 and go up to 255
  private nextEntityID(): number {
    let id = 0;
    const sortedIds = this.sprites
      .flatMap(sprite => sprite.id)
      .sort();
    for (let i = 0; i < sortedIds.length; i++) {
      const spriteId = sortedIds[i];
      if (spriteId - id > 1) {
        break;
      }
      id = spriteId;
    }
    return id + 1;
  }

  public newPlayer(holder: PlayerHolder): PlayerSprite | undefined {
    if (this.numOfPlayers++ >= MAX_PLAYERS) {
      Logger.warning("Max players reached in game %s", this.id.toString(16));
      return undefined;
    }
    return this.addSprite(new PlayerSprite(holder, this, this.nextEntityID(), 1.5, 1.5));
  }

  public newBullet(player: PlayerSprite): BulletSprite | undefined {
    return this.addSprite(new BulletSprite(player, this.nextEntityID()));
  }

  private addSprite<T extends GameSprite>(sprite: T): T | undefined {
    if (this.sprites.length >= MAX_SPRITES) {
      return undefined;
    }
    this.sprites.push(sprite);
    this.broadcastGroup.send(new CreatePacket(sprite));
    return sprite;
  }

  private cleanupSprites() {
    this.deadSprites.forEach(spriteIndex => this.sprites.splice(spriteIndex, 1));
    this.deadSprites.clear();
  }

  public removePlayer(player: PlayerSprite, actor: PlayerSprite | undefined = undefined) {
    if (this.removeSprite(player, actor)) {
      --this.numOfPlayers;
    }
  }

  public removeSprite(sprite: GameSprite, actor: GameSprite | undefined = undefined): boolean {
    const index = this.sprites.indexOf(sprite);
    if (index < 0) {
      return false;
    }
    this.deadSprites.add(index);
    this.broadcastGroup.send(new DestroyPacket(sprite, actor));
    return true;
  }

  public getTime(): number {
    return this.time;
  }
}
