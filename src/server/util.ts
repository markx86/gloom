export function getEnvStringOrDefault(name: string, def: string): string {
  const value = process.env[name];
  return value != null && value.length > 0 ? value : def;
}

export function getEnvIntOrDefault(name: string, def: number, min: number, max: number): number {
  const value = process.env[name];
  const int = parseInt(value ?? "");
  return int >= min && int <= max ? int : def;
}
