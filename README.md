# gloom
#### a 2.5D FPS game written in WASM and TypeScript

---

## Credits and sources

- **Player textures**: _Marine from DOOM & DOOM II_ by id Software
- **Bullet texture**: _Enemy Bullets_ by Wahib Yousaf ([link](https://gameguru101.artstation.com/projects/Lev02P))
- **In game font**: _ZAP_ by John Zaitseff ([link](https://www.zap.org.au/projects/console-fonts-zap))
- **UI style**: _98.css_ by Jordan Scales ([**@jdan**](https://github.com/jdan)) ([link](https://jdan.github.io/98.css)) 
- **UI font**: _Microsoft Sans-Serif_ by Microsoft

And last, but not least, _grecha.js_ by Alexey Kutepov ([**@rexim**](https://github.com/rexim)) ([link](https://github.com/tsoding/grecha.js)), which
was the inspiration for `reactive.js` after React let me down :(


## Project structure

The project has the following structure:

- `Dockerfile` This file describes the docker environment necessary to build the client and the server.
- `docker-compose.yml` This file is a working (and production ready) docker compose file to quickly setup and host the server.
- `package.json` NPM project file.
- `scripts` All the scripts needed to build the project.
  - `tools` Extra scripts that are used to generate source files at build-time.
- `static` Contains the files that the HTTP server will serve to the client.
  - `css` Stylesheet and font files.
  - `html` HTML files.
  - `js` JavaScript and WASM files.
  - `img` Images used by the UI.
- `res` Resources for the WASM game, such as textures and map data.
- `src` All the source files.
  - `client` Source code for the frontend (JavaScript).
    - `wasm` Source code for the game (C)
  - `server` Source code for the HTTP and game server (TypeScript)


## Setting it up

To host the server you will need the [`docker`](https://www.docker.com/get-started) installed and configured on your system.
The first step is to clone this repository:

```sh
git clone https://github.com/markx86/gloom.git
```

If you have setup SSH access to GitHub, it is recommended you use that:

```sh
git clone git@github.com:markx86/gloom.git
```

After cloning the repository, enter the project root with `cd gloom` and run:

> [!NOTE]
> Before running this command you might want to check out the [configuration](#configuration) section.

```sh
docker compose up --build -d
```

> [!IMPORTANT]
> If you do not wish to run the server as a daemon, use
> ```sh
> docker compose up --build
> ```
> instead. Do note that pressing `Ctrl+C` or closing the terminal
> will also close the server.

**This will take a while** (up to 5 minutes or more, depending on your internet connection).

After the server is up and running you can connect to it locally, by going to `http://localhost:8080`,
assuming you are using the same machine the server is being hosted on.

If you have any problems, you can check the server logs by using the command:

```sh
docker compose logs -f
```


## Configuration

You can configure four parameters.
- `LOG_VERBOSE`: Enables/Disables verbose logging. Set to `1` to enable. By default it's disabled.
- `DATABASE`: Path to the database file. This a SQLite3 database path, therefore things like `:memory:` will also work. By default it's memory, but the provided `docker-compose` file will create a volume and store the database in there.
- `COOKIE_SECRET`: The secret key used to sign the cookies. If it's not set, the server will generate a random one each time it is restarted.
- HTTP server port: To change this port it is necessary to edit either the `docker-compose` or the source code directly (`src/server/http-server.ts`). By default the HTTP server is hosted on port `8080`.

> [!NOTE]
> The WebSocket server port is `8492` and while it _can_ be changed, it is not recommended.
> However if you still wish to change it, you need to edit the `WSS_PORT` variable in `src/server/game-server.ts` and `src/client/gloom.js`.

> [!IMPORTANT]
> If you're using `docker-compose`, you can change `COOKIE_SECRET` and `LOG_VERBOSE`, by creating a `.env` file in the project root,
> and writing your values there, in the form `PARAM_NAME=param_value`. For example `COOKIE_SECRET=mySuperSecret1234`.
