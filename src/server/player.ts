import { PlayerSprite } from "./sprite";
import * as db from "./database";

export class Player {
  readonly username: string;
  readonly token: number;
  readonly stats: PlayerStats;

  public sprite?: PlayerSprite;

  constructor(username: string, token: number) {
    this.username = username;
    this.token = token;
    this.stats = new PlayerStats(username);
  }
}

export class PlayerStats {
  private dead: boolean;
  private kills: number;
  private username: string;

  constructor(username: string) {
    this.dead = false;
    this.kills = 0;
    this.username = username;
  }

  public addKill() {
    ++this.kills;
  }

  public setDead(value: boolean) {
    this.dead = value;
  }

  public save() {
    db.updateUserStats(this.username, this.kills, this.dead);
  }
};
