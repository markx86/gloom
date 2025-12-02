import { PlayerSprite } from "./sprite";

export class PlayerHandle {
  public username: string;
  public sprite: PlayerSprite | undefined;

  constructor(username: string) {
    this.username = username;
  }
}
