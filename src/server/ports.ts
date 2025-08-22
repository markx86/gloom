import Logger from "./logger";
import { getEnvIntOrDefault } from "./util";

export const HTTP_PORT = getEnvIntOrDefault("HTTP_PORT", 8080, 0, 0xFFFF);
export const HTTPS_PORT = getEnvIntOrDefault("HTTP_PORT", 8443, 0, 0xFFFF);

if (HTTP_PORT === HTTPS_PORT) {
  Logger.error("HTTP_PORT cannot be the same as HTTPS_PORT");
  process.exit(-1);
}
