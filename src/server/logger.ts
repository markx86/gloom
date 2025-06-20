import { getEnvStringOrDefault } from "./util";

const LOG_TRACE = getEnvStringOrDefault("LOG_VERBOSE", "0") === "1";

export default class Logger {
  public static error(message: any, ...optargs: any[]) {
    console.log("[!] " + message, ...optargs);
  }

  public static warning(message: any, ...optargs: any[]) {
    console.log("[#] " + message, ...optargs);
  }

  public static trace(message: any, ...optargs: any[]) {
    if (LOG_TRACE) {
      console.log("[*] " + message, ...optargs);
    }
  }

  public static success(message: any, ...optargs: any[]) {
    console.log("[+] " + message, ...optargs);
  }
}

