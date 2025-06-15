import Logger from "./logger";
import { BroadcastGroup } from "./broadcast";
import { CreatePacket, DestroyPacket, TerminatePacket, WaitPacket } from "./packet";
import { randomInt } from "node:crypto";

const MIN_PLAYERS = 2;
const MAX_PLAYERS = 4;
const MAX_SPRITES = 256;
const MAX_GAMES = 256;

const PLAYER_HEALTH = 100;
const PLAYER_RUN_SPEED = 3.5;
const PLAYER_RELOAD_TIME = 0.5;

const BULLET_DAMAGE = 25;
const BULLET_INITIAL_SPEED = 10;

const COLL_DOF = 8

const IDLE_TIME = 300;
const WAIT_TIME = 10;
const OVER_TIME = 10;

const POS_DIFF_THRESHOLD = 0.5;
const TIME_SKEW_THRESHOLD = -0.1;

enum GameSpriteType {
  PLAYER,
  BULLET
}

enum GameState {
  WAITING,
  READY,
  PLAYING,
  OVER
}

function nowTime(): number {
  return Date.now() / 1e3;
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
  readonly radius: number;
  protected x: number;
  protected y: number;
  protected rotation: number;
  protected velocity: number;
  protected dirX: number;
  protected dirY: number;

  public constructor(game: Game, id: number, type: GameSpriteType, radius: number,
                     x: number, y: number, velocity: number, rotation: number = 0) {
    this.id = id;
    this.type = type;
    this.x = x;
    this.y = y;
    this.radius = radius;
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
    if (vDistMax < vDist + this.radius) {
      vDist = vDistMax - this.radius;
      collided = true;
    }

    const hDistMax = traceRay(
      x, y,
      signDirX, 0,
      this.game.map);
    if (hDistMax < hDist + this.radius) {
      hDist = hDistMax - this.radius;
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
  getToken(): number;
  unsetPlayer(): void;
}

export class PlayerSprite extends GameSprite {
  readonly holder: PlayerHolder;

  private health: number;
  private reloadTime: number;

  public constructor(holder: PlayerHolder, game: Game, id: number,
                     x: number, y: number, r: number = 0) {
    super(game, id, GameSpriteType.PLAYER, 0.15, x, y, 0, r);
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
        Logger.trace("Player %d killed by player %d", this.id, other.owner.id);
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
      const minDist = this.radius + other.radius;
      if (this.distanceFrom(other) < minDist) {
        this.onSpriteCollision(other);
      }
    })
  }

  public acknowledgeUpdatePacket(ts: number, x: number, y: number, rotation: number, keys: number): [boolean, number] {
    Logger.trace("now: %f - ts: %f", this.game.getTime(), ts);
    const delta = this.game.getTime() - ts;

    const [_, predictedX, predictedY] = this.moveAndCollide(delta, x, y);
    const dx = this.x - predictedX;
    const dy = this.y - predictedY;
    const dist = Math.sqrt(dx * dx + dy * dy);
    Logger.trace("Distance from prediction: %d", dist)
    const ack = delta > TIME_SKEW_THRESHOLD && dist <= POS_DIFF_THRESHOLD;

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
      player.game, id, GameSpriteType.BULLET, 0.01,
      player.getX(), player.getY(),
      BULLET_INITIAL_SPEED, player.getR(),
    );
    this.owner = player;
  }

  protected onWallCollision() {
    this.game.removeSprite(this);
  }
}

export class SpawnPosition {
  readonly x: number;
  readonly y: number;
  readonly rot: number;

  constructor(x: number, y: number, rot: number) {
    this.x = x + 0.5;
    this.y = y + 0.5;
    this.rot = -rot * Math.PI / 180.0;
  }
}

export class GameMap {
  private spawnPositions: Array<SpawnPosition>;
  private compressed: Uint8Array | undefined;
  private tiles: Uint8Array;
  readonly width: number;
  readonly height: number;

  public constructor(width: number, height: number, tiles: ArrayLike<number>, spawns: Array<SpawnPosition>) {
    if (width * height != tiles.length) {
      throw new Error(`Trying to create map of size ${width}x${height} with only ${tiles.length} tiles!`);
    }
    this.width = width;
    this.height = height;
    this.tiles = new Uint8Array(tiles);
    this.spawnPositions = spawns;
  }

  public getCompressedData(): Uint8Array {
    if (!this.compressed) {
      const compressed = this.tiles.reduce((running, tileValue, tileIndex) => {
        tileValue &= 1;
        const bitPos = tileIndex & 7;
        if (bitPos === 0) {
          running.push(tileValue);
        } else {
          running[tileIndex >> 3] |= tileValue << bitPos;
        }
        return running;
      }, new Array<number>())
      this.compressed = new Uint8Array(compressed);
    }
    return this.compressed;
  }

  public getSizeInBytes(): number {
    return ((this.tiles.length + 7) & ~7) >> 3;
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
    return (this.tiles[x + y * this.width] & 1) !== 0;
  }

  public getSpawnPositionForPlayer(id: number, numPlayers: number): SpawnPosition | undefined {
    if (numPlayers < this.spawnPositions.length) {
      const index = id % this.spawnPositions.length;
      return this.spawnPositions[index];
    }
  }
}

export class Game {
  readonly id: number;
  readonly creator: string;
  readonly map: GameMap;
  readonly sprites: Array<GameSprite>;
  readonly broadcastGroup: BroadcastGroup;

  private playerTokens: Map<number, string>;
  private numOfPlayers: number;
  private startTime: number;
  private waitTime: number;
  private deadSprites: Set<number>;
  private state: GameState;

  private static games = new Map<number, Game>();

  public constructor(id: number, creator: string, map: GameMap) {
    this.id = id;
    this.creator = creator;
    this.map = map;

    this.sprites = new Array<GameSprite>();
    this.deadSprites = new Set<number>();
    this.playerTokens = new Map<number, string>();
    this.numOfPlayers = 0;
    this.startTime = 0;
    this.waitTime = IDLE_TIME;
    this.broadcastGroup = BroadcastGroup.get(id);
    this.state = GameState.WAITING;
  }

  public static create(creator: string, map: GameMap): number | undefined {
    if (Game.games.size < MAX_GAMES) {
      let id: number | undefined;
      while (id == null || id in Game.games) {
        id = randomInt(2 ** 32);
      }
      Game.games.set(id, new Game(id, creator, map));
      return id;
    }
  }

  public static destroy(game: Game) {
    game.broadcastGroup.send(new TerminatePacket());
    Game.games.delete(game.id);
  }

  public static destroyAll() {
    Game.games.forEach((game, _) => Game.destroy(game));
  }

  public static getById(id: number): Game | null {
    const game = Game.games.get(id);
    if (!game) {
      Logger.error("No game exists with ID %s", id.toString(16));
      return null;
    }
    return game;
  }

  public static getByCreator(creator: string): Game | null {
    for (const game of Game.games.values()) {
      if (game.creator === creator) {
        return game;
      }
    }
    return null;
  }

  public isWaiting(): boolean {
    return this.state == GameState.WAITING;
  }
  
  public static tickAll(delta: number) {
    Game.games.forEach(game => game.tick(delta));
  }

  private tick(delta: number) {
    switch (this.state) {
      case GameState.WAITING: {
        if (this.numOfPlayers >= MIN_PLAYERS) {
          this.waitTime = WAIT_TIME;
          this.state = GameState.READY;
          this.broadcastGroup.send(new WaitPacket(this));
        } else if (this.waitTime <= 0) {
          this.state = GameState.OVER;
        } else {
          this.waitTime -= delta;
        }
        break;
      }

      case GameState.READY: {
        if (this.numOfPlayers < MIN_PLAYERS) {
          this.waitTime = IDLE_TIME;
          this.state = GameState.WAITING;
        } else if (this.waitTime <= 0) {
          this.startTime = nowTime();
          this.waitTime = 0;
          this.state = GameState.PLAYING;
        } else {
          this.waitTime -= delta;
          break;
        }
        this.broadcastGroup.send(new WaitPacket(this));
        break;
      }

      case GameState.PLAYING: {
        if (this.numOfPlayers <= 1) {
          this.waitTime = OVER_TIME;
          this.state = GameState.OVER;
          break;
        }
        this.sprites.forEach(sprite => sprite.tick(delta)); // tick the game world
        break;
      }

      case GameState.OVER: {
        if (this.numOfPlayers == 0 || this.waitTime <= 0) {
          Logger.trace("Destroying game with ID: %s", this.id.toString(16));
          Game.destroy(this);
        } else {
          this.waitTime -= delta;
        }
        break;
      }
    }
    this.cleanupSprites();
  }

  public getSpriteById(id: number): GameSprite | undefined {
    return this.sprites.filter(sprite => sprite.id === id).pop();
  }

  // NOTE: entity IDs start from 1 and go up to 255
  private nextEntityId(): number {
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
    } else if (this.playerTokens.has(holder.getToken())) {
      const id = this.nextEntityId();
      const pos = this.map.getSpawnPositionForPlayer(id - 1, this.numOfPlayers);
      if (pos != null) {
        Logger.trace("Spawining player %s (ID: %d) @ (x = %f, y = %f, r = %f)", holder.getToken(), id, pos.x, pos.y, pos.rot);
        return this.addSprite(new PlayerSprite(holder, this, id, pos.x, pos.y, pos.rot));
      }
      Logger.warning("No place to spawn player with token %s", holder.getToken());
    } else {
      Logger.error("No player with that token");
    }
  }

  public newBullet(player: PlayerSprite): BulletSprite | undefined {
    return this.addSprite(new BulletSprite(player, this.nextEntityId()));
  }

  public allocatePlayer(username: string): number | undefined {
    if (this.state !== GameState.WAITING && this.state !== GameState.READY) {
      return;
    }
    for (const uname of this.playerTokens.values()) {
      if (uname === username) {
        return;
      }
    }
    let token: number | undefined;
    while (token == null || token in this.playerTokens) {
      token = randomInt(2 ** 32);
    }
    this.playerTokens.set(token, username);
    Logger.trace("Allocated player with token %s", token.toString(16));
    return token;
  }

  private addSprite<T extends GameSprite>(sprite: T): T | undefined {
    if (this.sprites.length < MAX_SPRITES) {
      this.sprites.push(sprite);
      this.broadcastGroup.send(new CreatePacket(sprite));
      return sprite;
    }
  }

  private cleanupSprites() {
    this.deadSprites.forEach(spriteIndex => this.sprites.splice(spriteIndex, 1));
    this.deadSprites.clear();
  }

  public removePlayer(player: PlayerSprite, actor: PlayerSprite | undefined = undefined) {
    if (this.removeSprite(player, actor)) {
      --this.numOfPlayers;
      this.playerTokens.delete(player.holder.getToken());
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
    return nowTime() - this.startTime;
  }

  public getWaitTime(): number {
    return this.waitTime;
  }
}
