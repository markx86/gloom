import Logger from "./logger";
import { GameMap } from "./map";
import { BroadcastGroup } from "./broadcast";
import { CreatePacket, DestroyPacket, TerminatePacket, WaitPacket } from "./packet";
import { BulletSprite, GameSprite, PlayerSprite } from "./sprite";
import { genUniqueIntForArray } from "./util";
import { PlayerHandle } from "./player";

export const DT = 1000 / 60;

const MIN_PLAYERS = 2;
const MAX_PLAYERS = 4;
const MAX_SPRITES = 256;
const MAX_GAMES = 256;

const IDLE_TIME = 300;
const WAIT_TIME = 10;
const OVER_TIME = 10;

enum GameState {
  WAITING,
  READY,
  PLAYING,
  OVER
}

function nowTime(): number {
  return Date.now() / 1e3;
}


export class Game {
  readonly id: number;
  readonly creator: string;
  readonly map: GameMap;
  readonly sprites: Array<GameSprite>;
  readonly broadcastGroup: BroadcastGroup;

  private players: Map<number, PlayerHandle>;
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
    this.players = new Map<number, PlayerHandle>();
    this.numOfPlayers = 0;
    this.startTime = 0;
    this.waitTime = IDLE_TIME;
    this.broadcastGroup = BroadcastGroup.get(id);
    this.state = GameState.WAITING;
  }

  public static create(creator: string, map: GameMap): number | undefined {
    if (Game.games.size < MAX_GAMES) {
      const id = genUniqueIntForArray([...Game.games.keys()]);
      Game.games.set(id, new Game(id, creator, map));
      Logger.trace("User '%s' created game with ID %s", creator, id.toString(16));
      return id;
    }
  }

  public static destroy(game: Game) {
    game.broadcastGroup.send(new TerminatePacket());
    Game.games.delete(game.id);
    Logger.trace("Destroyed game with ID %s", game.id.toString(16))
  }

  public static getById(id: number): Game | null {
    const game = Game.games.get(id);
    if (!game) {
      Logger.trace("No game exists with ID %s", id.toString(16));
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
    return this.state === GameState.WAITING;
  }

  public isReady(): boolean {
    return this.state === GameState.READY;
  }

  public isPlaying(): boolean {
    return this.state === GameState.PLAYING;
  }
  
  public static tickAll(delta: number) {
    Game.games.forEach(game => game.tick(delta));
  }

  private readyPlayers(): number {
    let readyPlayers = 0;
    this.sprites.forEach(sprite => {
      if (sprite instanceof PlayerSprite) {
        readyPlayers += sprite.isReady() ? 1 : 0;
      }
    });
    return readyPlayers;
  }

  private tick(delta: number) {
    switch (this.state) {
      case GameState.WAITING: {
        if (this.numOfPlayers >= MIN_PLAYERS && this.readyPlayers() === this.numOfPlayers) {
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
        if (this.numOfPlayers < MIN_PLAYERS || this.readyPlayers() !== this.numOfPlayers) {
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
        // FIXME: Add a maximum game duration.
        if (this.numOfPlayers <= 1) {
          this.waitTime = OVER_TIME;
          this.state = GameState.OVER;
          break;
        }
        // Tick the game world.
        this.sprites.forEach(sprite => sprite.tick(delta));
        break;
      }

      case GameState.OVER: {
        if (this.numOfPlayers == 0 || this.waitTime <= 0) {
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
    // This is decently fast, because this.sprites cannot have more than 255 entries at any
    // given time.
    return this.sprites.filter(sprite => sprite.id === id).pop();
  }

  // NOTE: Entity IDs start from 1 and go up to 255.
  private nextEntityId(): number {
    let id = 0;
    const sortedIds = this.sprites.map(sprite => sprite.id).sort();
    for (let i = 0; i < sortedIds.length; i++) {
      const spriteId = sortedIds[i];
      if (spriteId - id > 1) {
        break;
      }
      id = spriteId;
    }
    return id + 1;
  }

  private trySpawnPlayer(token: number, handle: PlayerHandle): PlayerSprite | undefined {
    const id = this.nextEntityId();
    const pos = this.map.getSpawnPositionForPlayer(id - 1, this.numOfPlayers);
    if (pos != null) {
      Logger.trace("Spawning player %s (ID: %d) @ (x = %f, y = %f, r = %f)", token.toString(16), id, pos.x, pos.y, pos.rot);
      this.numOfPlayers++;
      handle.sprite = new PlayerSprite(token, this, id, pos.x, pos.y, pos.rot);
      return this.addSprite(handle.sprite);
    }
  }

  public newPlayer(token: number): PlayerSprite | undefined {
    const handle = this.players.get(token);
    if (this.numOfPlayers >= MAX_PLAYERS) {
      Logger.warning("Max players reached in game %s", this.id.toString(16));
    } else if (handle == null) {
      Logger.error("No player with that token");
    } else if (handle.sprite != null) {
      return handle.sprite;
    } else {
      // Try to spawn player.
      const playerSprite = this.trySpawnPlayer(token, handle);
      if (playerSprite == null) {
        Logger.warning("No place to spawn player with token %s", token);
      }
      return playerSprite;
    }
  }

  public newBullet(player: PlayerSprite): BulletSprite | undefined {
    return this.addSprite(new BulletSprite(player, this.nextEntityId()));
  }

  private getTokenForUsername(username: string): number | undefined {
    for (const [token, handle] of this.players.entries()) {
      if (handle.username === username) {
        return token;
      }
    }
  }

  public allocatePlayer(username: string): number | string {
    const token = this.getTokenForUsername(username);
    if (token != null && this.state !== GameState.OVER) {
      // The player may be attempting to rejoin the game.
      return token;
    } else if (this.state !== GameState.WAITING && this.state !== GameState.READY) {
      return "That game has already started.";
    } else {
      const token = genUniqueIntForArray([...this.players.keys()]);
      this.players.set(token, new PlayerHandle(username));
      Logger.trace("Allocated player with token %s", token.toString(16));
      return token;
    }
  }

  public deallocatePlayer(token: number) {
    if (this.players.delete(token)) {
      Logger.trace("Deallocated player with token %s", token.toString(16));
    }
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
      this.deallocatePlayer(player.token);
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
