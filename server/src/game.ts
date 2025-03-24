import Logger from "./logger";

const MAX_PLAYERS = 16;
const SPRITE_RADIUS = 0.15;
const PLAYER_RUN_SPEED = 3.5;
const COLL_DOF = 8

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

export class GameSprite {
  readonly id: number;
  protected x: number;
  protected y: number;
  protected rotation: number;
  protected velocity: number;
  protected dirX: number;
  protected dirY: number;
  protected map: GameMap;

  public constructor(id: number, map: GameMap, x: number, y: number, velocity: number, rotation: number = 0) {
    this.id = id;
    this.x = x;
    this.y = y;
    this.rotation = rotation;
    this.velocity = velocity;
    this.dirX = Math.cos(rotation);
    this.dirY = Math.sin(rotation);
    this.map = map;
  }

  protected onCollision() {
    // TODO: remove this in prod
    console.log("sprite collided");
  }

  public tick(delta: number) {
    const space = this.velocity * delta;

    const signDirX = (this.dirX < 0) ? -1 : +1;
    const signDirY = (this.dirY < 0) ? -1 : +1;

    let vDist = Math.abs(this.dirY) * space;
    let hDist = Math.abs(this.dirX) * space;

    let collided = false;

    const vDistMax = traceRay(
      this.x, this.y,
      0, signDirY,
      this.map);
    if (vDistMax < vDist + SPRITE_RADIUS) {
      vDist = vDistMax - SPRITE_RADIUS;
      collided = true;
    }

    const hDistMax = traceRay(
      this.x, this.y,
      signDirX, 0,
      this.map);
    if (hDistMax < hDist + SPRITE_RADIUS) {
      hDist = hDistMax - SPRITE_RADIUS;
      collided = true;
    }

    this.x += signDirX * hDist;
    this.y += signDirY * vDist;

    if (collided === true) {
      this.onCollision();
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

export class PlayerSprite extends GameSprite {
  readonly game: Game;

  public constructor(id: number, game: Game, x: number, y: number, r: number = 0) {
    super(id, game.map, x, y, 0, r);
    this.game = game;
  }
  
  protected onCollision() {
    // TODO: remove this in prod
    console.log("player collided");
  }

  public acknowledgeUpdatePacket(x: number, y: number, rotation: number, keys: number): boolean {
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
    this.rotation = rotation;

    this.x = x;
    this.y = y;

    const longDir = ((keys & 0x00ff) !== 0 ? 1 : 0) - ((keys & 0xff00) !== 0 ? 1 : 0);
    keys >>= 16;
    const sideDir = ((keys & 0x00ff) !== 0 ? 1 : 0) - ((keys & 0xff00) !== 0 ? 1 : 0);

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

    return true;
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
  private numOfPlayers: number;

  private static games = new Map<number, Game>();

  public constructor(id: number, map: GameMap) {
    this.id = id;
    this.map = map;
    this.sprites = new Array<GameSprite>();
    this.numOfPlayers = 0;
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
    this.sprites.forEach(sprite => sprite.tick(delta)); // tick the game world
  }

  public getSpriteByID(id: number): GameSprite | undefined {
    return this.sprites.filter(sprite => sprite.id === id).pop();
  }

  private nextEntityID(): number {
    return this.sprites.reduce((max, sprite) => sprite.id > max ? sprite.id : max, -1) + 1;
  }

  public newPlayer(): PlayerSprite | undefined {
    if (this.numOfPlayers++ >= MAX_PLAYERS) {
      return undefined;
    }
    const player = new PlayerSprite(this.nextEntityID(), this, 1.5, 1.5);
    this.sprites.push(player);
    return player;
  }

  public removePlayer(player: PlayerSprite) {
    const index = this.sprites.indexOf(player);
    if (index >= 0) {
      this.sprites.splice(index, 1);
      --this.numOfPlayers;
    }
  }
}
