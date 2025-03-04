export default class Logger {
  public static error(message: any, ...optargs: any[]) {
    console.log("[!] " + message, optargs);
  }

  public static warning(message: any, ...optargs: any[]) {
    console.log("[#] " + message, optargs);
  }

  public static info(message: any, ...optargs: any[]) {
    console.log("[*] " + message, optargs);
  }

  public static success(message: any, ...optargs: any[]) {
    console.log("[+] " + message, optargs);
  }
}

