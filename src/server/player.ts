import { PlayerSprite } from "./sprite";
import { updateUserStats } from "./database";
import Logger from "./logger";

export class Player {
  readonly username: string;
  readonly token: number;
  readonly stats: PlayerStats;

  public sprite: PlayerSprite | undefined;

  constructor(username: string, token: number) {
    this.username = username;
    this.token = token;
    this.stats = new PlayerStats();
  }

  public saveStats() {
    updateUserStats(this.username, this.stats?.getKills(), this.stats.isDead())
  }
}

export class PlayerStats {
  private dead: boolean;
  private kills: number;

  constructor() {
    this.dead = false;
    this.kills = 0;
  }

  public addKill() {
    ++this.kills;
  }

  public getKills(): number {
    return this.kills;
  }

  public isDead(value: boolean | undefined = undefined): boolean {
    const prevValue = this.dead;
    if (value != null) {
      this.dead = value;
    }
    return prevValue;
  }
};
