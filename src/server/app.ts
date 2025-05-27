#!/usr/bin/env node

process.on("SIGINT", () => process.exit(0));

import "./http-server.ts";
import "./game-server.ts";
