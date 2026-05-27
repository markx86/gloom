import Logger from "./logger";
import { Game } from "./game";
import { GameMap } from "./map";
import { Player } from "./player";

const PLAYER_HEALTH = 100;
const PLAYER_RUN_SPEED = 3.5;
const PLAYER_RELOAD_TIME = 0.5;
const PLAYER_RADIUS = 0.15;

const BULLET_DAMAGE = 25;
const BULLET_INITIAL_SPEED = 10;

const COLL_DOF = 8

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
  readonly radius: number;
  protected x: number;
  protected y: number;
  protected rotation: number;
  protected velocity: number;
  protected dirX: number;
  protected dirY: number;

  public constructor(
    game: Game, id: number, type: GameSpriteType, radius: number,
    x: number, y: number, velocity: number, rotation: number = 0
  ) {
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
      this.game.map
    );

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

export class PlayerSprite extends GameSprite {
  readonly player: Player;

  private health: number;
  private reloadTime: number;
  private ready: boolean;

  public constructor(
    player: Player, game: Game,
    id: number, x: number, y: number, r: number = 0
  ) {
    super(game, id, GameSpriteType.PLAYER, PLAYER_RADIUS, x, y, 0, r);
    this.player = player;
    this.health = PLAYER_HEALTH;
    this.reloadTime = 0;
    this.ready = false;
  }

  public setReady(yes: boolean) {
    this.ready = yes;
  }

  public isReady(): boolean {
    return this.ready;
  }

  protected onWallCollision() {}

  private onSpriteCollision(other: GameSprite) {
    if (other instanceof BulletSprite && other.owner !== this) {
      this.game.removeSprite(other, this);
      this.health -= BULLET_DAMAGE;
      if (this.health <= 0) {
        // Update stats.
        this.player.stats?.setDead(true);
        other.owner.player.stats?.addKill();
        // Remove player from game.
        Logger.trace("Player %d killed by player %d", this.id, other.owner.id);
        this.game.removePlayer(this, other.owner);
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

  public processUpdatePacket(inputX: number, inputY: number, rotation: number) {
    this.rotation = rotation;

    const longDirX = Math.cos(rotation);
    const longDirY = Math.sin(rotation);

    const sideDirX = -longDirY;
    const sideDirY = +longDirX;

    // Ensure inputX is between -1.0 and +1.0
    if /**/ (inputX < -1.0) inputX = -1.0;
    else if (inputX > +1.0) inputX = +1.0;

    // Ensure inputY is between -1.0 and +1.0
    if /**/ (inputY < -1.0) inputY = -1.0;
    else if (inputY > +1.0) inputY = +1.0;

    const inputStrength = Math.sqrt(inputX * inputX + inputY * inputY);
    if (inputStrength > 0) {
      const dirX = longDirX * inputY + sideDirX * inputX;
      const dirY = longDirY * inputY + sideDirY * inputX;
      const dirLength = Math.sqrt(dirX * dirX + dirY * dirY);

      this.dirX = dirX / dirLength;
      this.dirY = dirY / dirLength;

      // Since both inputX and inputY are between -1.0 and +1.0, inputStrength is between
      // 0 and sqrt(2), so to express the current inputStrength as a percentage of the maximum
      // inputStrength, we can just divide it by sqrt(2).
      this.velocity = (inputStrength * Math.SQRT1_2) * PLAYER_RUN_SPEED;
    } else {
      this.velocity = this.dirX = this.dirY = 0.0;
    }

    Logger.trace("Player %d is @ (x = %f, y = %f)", this.id, this.x, this.y);
  }

  public fireBullet(): BulletSprite | undefined {
    if (this.reloadTime <= 0 && this.health > 0) {
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

