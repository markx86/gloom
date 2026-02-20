import { randomInt } from "node:crypto";

export function getEnvStringOrDefault(name: string, def: string): string {
  const value = process.env[name];
  return value != null && value.length > 0 ? value : def;
}

export function getEnvIntOrDefault(name: string, def: number): number {
  const value = process.env[name];
  const int = parseInt(value ?? "");
  if (isNaN(int) || !isFinite(int)) {
    return def;
  } else {
    return int;
  }
}

export function genUniqueIntForArray(arr: Array<number>): number {
  let x: number | undefined;
  while (x == null || arr.includes(x)) {
    x = randomInt(2 ** 32);
  }
  return x;
}
