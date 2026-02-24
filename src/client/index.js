import * as api from "./api.js";
import * as gloom from "./gloom.js";
import "./reactive.js";
import {
  showErrorWindow, showInfoWindow, showWarningWindow,
  showMessageWindow, showYesNoWindow,
  createWindow, getWindowControls,
  helpLink, windowIcon, separator,
  MSGWND_HELP, MSGWND_WARN
} from "./windowing.js";

const root = $root($("#root"));
// Disable scroll bars.
document.body.style.overflow = "hidden";

function updateRootNodeSize() {
  root.style.width = `${window.innerWidth}px`;
  root.style.height = `${window.innerHeight}px`;
}

updateRootNodeSize();
window.addEventListener("resize", updateRootNodeSize);

const HOME_TAB_PLAY = 0;
const HOME_TAB_STATS = 1;
const HOME_TAB_LEADERBOARD = 2;
const HOME_TAB_MAPS = 3;

const MAP_SIZE = 32;
const MAP_CANVAS_SCALE = 16;

const MAP_DRAW_CLEAR = 0;
const MAP_DRAW_WALL = 1;
const MAP_DRAW_PLAYER1 = 2;
const MAP_DRAW_PLAYER2 = 3;
const MAP_DRAW_PLAYER3 = 4;
const MAP_DRAW_PLAYER4 = 5;

const MAP_SCROLL_SENSITIVITY = 8;

const MAP_COLORS = [
  "white",
  "black",
  "blue",
  "red",
  "green",
  "magenta"
];

const MAP_DIRECTION_ARROWS = [
  "\ud83e\udc72", //   0 deg
  "\ud83e\udc75", //  45 deg
  "\ud83e\udc71", //  90 deg
  "\ud83e\udc74", // 135 deg
  "\ud83e\udc70", // 180 deg
  "\ud83e\udc77", // 225 deg
  "\ud83e\udc73", // 270 deg
  "\ud83e\udc76", // 315 deg
];

gloom.loadGloom().then(([gloomLaunch, gloomExit]) => {
  const Globals = {
    gameWs: null,
    myGameId: null,
    myUsername: null,
    myStats: null,
    myMaps: null,
    leaderboard: null,
    homePageTab: HOME_TAB_PLAY
  };

  function validateInput(event) {
    const inputBox = event.target;
    const data = event.data;
    const value = inputBox.value;
    if (data != null) {
      if (!(/^[a-fA-F0-9]+$/.test(data))) {
        const caret = inputBox.selectionStart;
        inputBox.value = value.substring(0, caret - data.length) + value.substring(caret);
      } else {
        inputBox.value = value.substring(0, 8);
      }
    }
  }
  
  const gotoHelp = () => $goto("/login/help");
  const gotoHome = () => $goto("/");
  
  async function doLogin(event) {
    const [wndEnable, wndDisable] = getWindowControls(event.target);
  
    const username_field = $("#field-username");
    const password_field = $("#field-password");
    $assert(username_field != null && password_field != null);
  
    const username = username_field.value;
    const password = password_field.value;
  
    wndDisable();
    if (username.length === 0) {
      showErrorWindow("Please insert a username!", wndEnable);
      return;
    }
    if (password.length === 0) {
      showErrorWindow("Please insert a password!", wndEnable);
      return;
    }
  
    try {
      const response = await api.post("/login", {
        username: username,
        password: password
      });
      if (response.status === 200) {
        $goto("/");
      } else {
        showErrorWindow("Invalid username or password. Please check your credentials and try again.", wndEnable);
      }
    } catch {
      showWarningWindow("An unknown error occurred while trying to process your registration request. Please try again later.", wndEnable);
    }
  }
  
  async function doRegister(event) {
    const [wndEnable, wndDisable] = getWindowControls(event.target);
  
    const username_field = $("#field-username");
    const password_field = $("#field-password");
    const password_check_field = $("#field-password-check");
    $assert(username_field != null && password_field != null && password_check_field != null);
  
    const username = username_field.value;
    const password = password_field.value;
  
    wndDisable();
    if (username.length === 0) {
      showErrorWindow("Please insert a username!", wndEnable);
      return;
    }
    if (password.length === 0) {
      showErrorWindow("Please insert a password!", wndEnable);
      return;
    }
    if (password !== password_check_field.value) {
      showErrorWindow("Passwords do not match!", wndEnable);
      return;
    }
  
    try {
      const response = await api.post("/register", {
        username: username,
        password: password
      });
  
      switch (response.status) {
        case 200: {
          showInfoWindow(
            "Registration successful! Use your crendentials to log-in.",
            () => $goto("/login")
          );
          break;
        }
        case 400: { showErrorWindow((await response.json()).message, wndEnable); break; }
        default:  { showWarningWindow("Something went wrong! Please try again later.", wndEnable); break; }
      }
    } catch {
      showWarningWindow("An unknown error occurred while trying to log you in. Please try again later.", wndEnable);
    }
  }
  
  function doLogout(event) {
    const [_, wndDisable] = getWindowControls(event.target);
    wndDisable();
    api.get("/logout").finally(_ => {
      // On success we redirect to the login page.
      // An error (403) means the user does not have a valid session anyway, so redirect to login again.
      $goto("/login");
    });
  }

  function serializeMap(canvas) {
    const spawnPoints = [0, 0, 0, 0];
    const mapTiles = [];
    canvas._map.forEach((cell, index) => {
      const x = index % MAP_SIZE;
      const y = Math.floor(index / MAP_SIZE);
      // Skip corner blocks
      if (x === 0 || x === MAP_SIZE-1 || y === 0 || y === MAP_SIZE-1) {
        return;
      }

      const playerIndex = getPlayerIndex(cell);
      if (playerIndex != null) {
        const u16 = (x & 0b11111) | ((y & 0b11111) << 5) | ((canvas._getPlayerRotation(playerIndex) & 0b111111) << 10);
        spawnPoints[playerIndex] = u16;
      }

      mapTiles.push(cell === MAP_DRAW_WALL ? 1 : 0);
    });

    // There's an invalid player position in the array
    if (spawnPoints.find(u16 => u16 === 0) != null) {
      return;
    }

    const serialized = new Uint8Array(2*spawnPoints.length + Math.ceil(((MAP_SIZE-2)**2) / 8));
    const view = new DataView(serialized.buffer, serialized.byteOffset, 2*spawnPoints.length);
    spawnPoints.forEach((u16, index) => view.setUint16(index << 1, u16, true));

    let j = view.byteLength * 8;
    for (let i = 0; i < mapTiles.length; i++) {
      const bit = j & 7;
      const byte = j >> 3;
      serialized[byte] |= mapTiles[i] << bit;
      ++j;
    }

    return serialized;
  }

  async function doSaveMap(event) {
    const [wndEnable, wndDisable] = getWindowControls(event.target);
    const canvas = $("#map-editor");

    wndDisable();

    const mapData = serializeMap(canvas);
    if (mapData == null) {
      showErrorWindow("A map must support up to four players!", wndEnable);
      return;
    }

    const res = await api.post("/map/update", {
      mapId: canvas._mapId,
      mapData: mapData.toBase64()
    });

    switch (res.status) {
      case 200: { $goto("/"); break; }
      case 400: { showErrorWindow((await res.json())?.message ?? "Invalid map!", wndEnable); break; }
      default:  { showWarningWindow("An error occurred while trying to save the map! Try again later.", wndEnable); break; }
    }
  }

  async function doCreateMap(event) {
    const mapName = $("#field-map-name")?.value;
    if (mapName == null) {
      return;
    }

    const [wndEnable, wndDisable] = getWindowControls(event.target);
    wndDisable();

    const res = await api.post("/map/create", { mapName });
    switch (res.status) {
      case 200: { $goto("/map", (await res.json()).mapId); break; }
      case 400: { showErrorWindow((await res.json()).message, wndEnable); break; }
      default:  { showWarningWindow("An error occurred while trying to create the map! Try again later.", wndEnable); break; }
    }
  }

  async function doDeleteMap(event, mapId) {
    const [wndEnable, wndDisable] = getWindowControls(event.target);
    wndDisable();

    const res = await api.post("/map/delete", { mapId });
    switch (res.status) {
      case 200: {
        if (Globals.myMaps != null) {
          // Do not refetch the maps list, just remove entry from the existing list.
          Globals.myMaps = Globals.myMaps.filter(map => map.mapId !== mapId);
          root.$refresh();
        }
        wndEnable();
        break;
      }

      case 400:
      case 404: {
        showErrorWindow((await res.json()).message, wndEnable);
        break;
      }

      default: {
        showWarningWindow("An error occurred while trying to deleting the map! Try again later.", wndEnable);
        break;
      }
    }
  }

  function refreshMyGameId() {
    api.get("/game/id")
      .then(res => {
        if (res.status === 200) {
          return res.json();
        } else {
          return { gameId: null };
        }
      })
      .then(data => {
        if (data.gameId !== Globals.myGameId) {
          Globals.myGameId = data.gameId;
          root.$refresh();
        }
      })
      .catch(e => {
        // Should've used $refresh()
        const gameIdLabel = $("#game-id");
        if (gameIdLabel != null) {
          gameIdLabel.textContent = "Could not fetch game ID";
        }
        console.error(e);
      });
  }

  async function refreshStats() {
    const res = await api.get("/stats");
    switch (res.status) {
      case 200: {
        const data = await res.json();

        Globals.myStats = data.userStats;
        Globals.leaderboard = data.leaderboard;

        // Maybe refresh scoreboard screen.
        if (
          $route() === "/"
          && (Globals.homePageTab === HOME_TAB_LEADERBOARD || Globals.homePageTab === HOME_TAB_STATS)
        ) {
          root.$refresh();
        }

        break;
      }
      case 404: { return; }
      default:  { console.error("Could not refresh scoreboard: %s", res.statusText); break; }
    }
  }

  async function fetchMaps() {
    const res = await api.get("/map/list");
    switch (res.status) {
      case 200: {
        Globals.myMaps = await res.json();
        if ($route() === "/" && Globals.homePageTab === HOME_TAB_MAPS) {
          root.$refresh();
        }
        break;
      }
      default:  { console.error("Could not fetch maps: %s", res.statusText); break; }
    }
  }

  function zeroPadL(s, w) {
    if (s.length >= w) {
      return s;
    }
    while (s.length < w) {
      s = "0" + s;
    }
    return s;
  }

  function parseGameIdString(gameIdString) {
    if (gameIdString != null) {
      const gameId = parseInt(gameIdString, 16);
      if (isFinite(gameId) && !isNaN(gameId)) {
        return gameId;
      }
    }
  }

  function gameIdToString(gameId) {
    if (gameId != null) {
      return zeroPadL(gameId.toString(16), 8).toUpperCase();
    }
  }

  function getGameId() {
    const gameIdString = $("#field-game-id")?.value;
    if (gameIdString?.length === 0) {
      return Globals.myGameId;
    } else if (gameIdString?.length === 8) {
      return parseGameIdString(gameIdString);
    }
  }

  async function doValidateSession() {
    try {
      const res = await api.get("/session/validate")
      if (res.status !== 200) throw Error("Not authenticated")
      const data = await res.json();
      Globals.myUsername = data?.username ?? "player";
      return true;
    } catch(e) {
      return false;
    }
  }
  
  async function doCreateGame(event) {
    const button = event.target;
    button.$disable();
    try {
    const response = await api.get("/game/create");
      if (response.status === 500) {
        showWarningWindow("Something went wrong while creating your game. Please try again later", button.$enable);
        return;
      }
      const data = await response.json();
      if (response.status !== 200) {
        const showWindow = response.status > 500 ? showWarningWindow : showErrorWindow;
        showWindow(data.message, button.$enable);
      } else {
        Globals.myGameId = data.gameId;
        root.$refresh();
      }
    } catch {
      showWarningWindow("An unknown error occurred while trying to create your game. Please try again later", button.$enable);
    }
  }
  
  async function doJoinGame(event) {
    const createButton = $("#btn-create");
    const createButtonDisabled = createButton.$attribute("disabled") != null;
    const [_wndEnable, wndDisable] = getWindowControls(event.target);
    const wndEnable = () => {
      _wndEnable();
      if (createButtonDisabled) { createButton.$disable(); }
    };
    wndDisable();
    const gameId = getGameId();
    if (gameId == null) {
      showErrorWindow("Please insert a valid game ID first.", wndEnable);
      return;
    }
  
    try {
      const response = await api.post("/game/join", { gameId });
      const data = await response.json();
      if (response.status === 200) {
        $goto("/game", gameId, data.playerToken, Globals.myUsername);
      } else {
        showErrorWindow(data.message, wndEnable);
      }
    } catch {
      showWarningWindow("An unknown error occurred while trying to join the game. Please try again later.", wndEnable);
    }
  }
  
  function listEntry(entry, desc) {
    return $p($strong(entry), " ► ", desc);
  }

  function showMapEditorHelp(event) {
    const [wndEnable, wndDisable] = getWindowControls(event.target);
    wndDisable();
    showMessageWindow(
      $div(
        $h5("TODO")
      ),
      MSGWND_HELP, wndEnable
    );
  };
  
  function showGameHelp(event) {
    const [wndEnable, wndDisable] = getWindowControls(event.target);
    wndDisable();
    showMessageWindow(
      $div(
        $h5("Controls").$style("margin", "12px 0px")
                       .$style("font-size", "20px"),
        $ul(
          $li(listEntry("W", "Move forwards")),
          $li(listEntry("S", "Move backwards")),
          $li(listEntry("A", "Strage left")),
          $li(listEntry("D", "Strage right")),
          $li(listEntry("P", "Pauses the game")),
          $li(listEntry("Mouse movement", "Move the camera left and right")),
          $li(listEntry("Mouse buttons", "Shoot"))
        ).$style("padding-left", "12px")
      ).$style("padding-left", "4px"),
      MSGWND_HELP, wndEnable
    );
  }
  
  function showSignUpHelp(event) {
    const [wndEnable, wndDisable] = getWindowControls(event.target);
    wndDisable();
    showMessageWindow(
      $div(
        $h5("Creating an account").$style("margin", "12px 0px")
                                  .$style("font-size", "20px"),
        $p("Here are the things to keep in mind when creating an account."),
        $ul(
          $li("Your username must be at least ", $strong("3"), " characters long"),
          $li("Your username must be no more than ", $strong("24"), " characters long"),
          $li("Your password must be at least ", $strong("8"), " characters long"),
          $li("Your password must be no more than ", $strong("64"), " characters long"),
          $li("Your password must contain at least a number and one of the following symbols", $code("!\"£$%&/()=?^-'.+#@\|~"))
        ).$style("padding-left", "12px")
      ).$style("padding-left", "4px"),
      MSGWND_HELP, wndEnable
    );
  }
  
  const login = () => {
    return createWindow(
      {
        title: "Welcome to Gloom",
        buttons: { help: gotoHelp, close: gotoHelp }
      },
      $div(
        windowIcon("/static/img/login.png", "48px", "48px"),
        $div(
          $p("Type a username and password to log on to Gloom."),
          $br(),
          $div(
            $label("Username:").$for("field-username"),
            $input().$type("text").$id("field-username")
          ).$class("field-row"),
          $div(
            $label("Password:").$for("field-password"),
            // Bump the field by one pixel, to align it with the username field
            // (very hacky, but it works also I hate css :D).
            $input().$type("password").$id("field-password").$style("margin-left", "7px")
          ).$class("field-row")
        ).$style("padding", "0px 8px"),
        $ul(
          $li($button("OK").$class("default").$onclick(doLogin)).$style("margin", "4px"),
          $li($button("Cancel").$onclick(gotoHelp)).$style("margin", "4px"),
        ).$style("list-style-type", "none")
         .$style("padding-left", "8px")
         .$style("margin-top", "6px")
      ).$style("padding", "8px 12px 20px 12px")
       .$style("display", "flex")
    );
  };
  
  const loginHelp = () => {
    return createWindow(
      {
        title: "Login help",
        buttons: { close: () => $goto("/login") }
      },
      $div(
        windowIcon("/static/img/help.png", "32px", "32px"),
        $div(
          helpLink("I want to create an account", "/signup"),
          $br(), $br(),
          helpLink("I already have an account, let me in!", "/login"),
        ).$style("padding", "0px 8px")
      ).$style("display", "flex")
       .$style("padding", "8px 12px 20px 12px")
    );
  };
  
  const signup = () => {
    return createWindow(
      {
        title: "Create a Gloom account",
        width: "400px",
        buttons: {
          help: showSignUpHelp,
          close: () => $goto("/login/help"),
        }
      },
      $div(
        $div(
          windowIcon("/static/img/signup.png", "48px", "48px"),
          $div(
            $h4("Account creation").$style("margin", "12px 0px"),
            $p(
              "Welcome to the Gloom account creating page.",
              $br(),
              "Please fill out the following form with your information."
            ).$style("line-height", "1.3em"),
            $div(
              $label("Username").$for("field-username"),
              $input().$type("text").$id("field-username")
            ).$class("field-row-stacked"),
            $div(
              $label("Password").$for("field-password"),
              $input().$type("password").$id("field-password")
            ).$class("field-row-stacked"),
            $div(
              $label("Retype your password").$for("field-password-check"),
              $input().$type("password").$id("field-password-check")
            ).$class("field-row-stacked")
          ).$style("padding", "0px 8px")
           .$style("margin-left", "4px")
        ).$style("display", "flex")
         .$style("padding", "8px 12px 12px 12px"),
        separator().$style("width", "95%"),
        $div(
          $button("Cancel").$onclick(gotoHelp),
          $button("Register").$onclick(doRegister)
                             .$class("default")
                             .$style("margin-left", "8px")
        ).$style("padding", "12px")
         .$style("float", "right")
      )
    );
  };

  const homeSeparator = () => separator().$style("margin", "12px 0px").$style("width", "100%");
  const homeTitle = title => $h4(title).$style("margin", "4px 0px 0px 0px");

  const homePlay = () => {
    const myGameIdString = gameIdToString(Globals.myGameId) ?? "No game running.";
    const currentGameId = $("#field-game-id")?.value ?? null;
    const placeholderString = Globals.myGameId == null
                              ? "Enter a game ID"
                              : `Enter a game ID (default: ${myGameIdString})`;

    return $div(
      $div(
        $label("Your game").$for("game-id")
                           .$style("font-weight", "bold")
                           .$style("margin-bottom", "8px"),
        $div(
          $p(myGameIdString).$id("game-id")
                            .$style("margin", "0px")
                            .$style("flex-grow", "1"),
          $button("Create").$id("btn-create")
                           .$style("margin-left", "12px")
                           .$onclick(doCreateGame)
                           .$enable(Globals.myGameId == null)
        ).$style("display", "flex")
         .$style("align-items", "center")
      ),
      homeSeparator(),
      $div(
        $label("Join a game").$for("field-game-id").$style("font-weight", "bold"),
        $div(
          $input().$id("field-game-id")
                  .$type("text")
                  .$attribute("placeholder", placeholderString)
                  .$attribute("value", currentGameId)
                  .$style("flex-grow", "1")
                  .$on("input", validateInput),
          $button("Join").$style("margin", "0px 0px 0px 8px")
                         .$onclick(doJoinGame)
        ).$style("display", "flex")
         .$style("align-items", "center")
      ).$class("field-row-stacked")
    );
  };

  const homeStats = () => {
    if (Globals.myStats == null) {
      return $div(
        $p($strong("Error fetching statistics data.")).$style("color", "red")
      );
    }

    const myStats = Globals.myStats;

    return $div(
      homeTitle(`${myStats.username} stats:`),
      homeSeparator(),
      listEntry("Total score", myStats.score.toString()),
      listEntry("Wins", myStats.wins.toString()),
      listEntry("Kills", myStats.kills.toString()),
      listEntry("Deaths", myStats.deaths.toString()),
      listEntry("Games played", myStats.games.toString()),
      listEntry("Avg. kills per game", (myStats.kills / myStats.games).toPrecision(2)),
      listEntry("K/D", (myStats.kills / myStats.deaths).toPrecision(2)),
    );
  };

  const homeLeaderboard = () => {
    if (Globals.leaderboard == null) {
      return $div(
        $p($strong("Error fetching leaderboard data.")).$style("color", "red")
      );
    }

    const rows = Globals.leaderboard.map(stats => {
      const row = $tr(
        $td(stats.username),
        $td(stats.score.toString()),
        $td(stats.wins.toString()),
        $td(stats.kills.toString()),
        $td(stats.deaths.toString()),
        $td(stats.games.toString())
      );
      if (stats.username === Globals.myUsername) {
        row.$class("highlighted");
      }
      return row;
    });

    return $div(
      homeTitle("Leaderboard"),
      homeSeparator(),
      $div(
        $table(
          $thead(
            $tr(
              $th("Username"),
              $th("Total score"),
              $th("Wins"),
              $th("Kills"),
              $th("Deaths"),
              $th("Games played"),
            )
          ),
          $tbody(...rows)
        ),
      ).$class("sunken-panel")
    );
  };

  function generateMapsTableRows() {
    const actionButton = (icon, onclick) => {
      return $img(icon, "16px", "16px")
        .$style("cursor", "pointer")
        .$style("margin", "1px 4px 1px 0px")
        .$onclick(onclick);
    };

    return $div(
      $label("Your maps"),
      $div(
        $table(
          $thead(
            $tr(
              $th("ID"),
              $th("Name"),
              $th("Actions")
            )
          ),
          $tbody(
            ...Globals.myMaps.map(map => {
              return $tr(
                $td(map.mapId.toString()),
                $td(map.mapName),
                $td(
                  actionButton("/static/img/edit.png", () => $goto("/map", map.mapId, map.mapData)),
                  actionButton("/static/img/delete.png", event => doDeleteMap(event, map.mapId))
                )
              );
            })
          )
        ).$style("width", "100%")
      ).$class("sunken-panel")
       .$style("max-height", "200px"),
    ).$class("field-row-stacked");
  }

  const homeMaps = () => {
    const extraComponents = [];

    if (Globals.myMaps == null) {
      extraComponents.push(homeSeparator());
      extraComponents.push(
        $div(
          $p($strong("Error fetching maps.")).$style("color", "red")
        )
      );
    } else if (Globals.myMaps.length > 0) {
      extraComponents.push(homeSeparator());
      extraComponents.push(generateMapsTableRows());
    }

    return $div(
      $div(
        $label("Map name").$for("field-map-name"),
        $div(
          $input().$type("text")
                  .$id("field-map-name")
                  .$attribute("placeholder", "Enter a map name")
                  .$style("flex-grow", "2"),
          $button("Create").$onclick(doCreateMap)
                           .$style("margin", "0px 0px 0px 12px")
        ).$style("display", "flex")
         .$style("align-items", "center")
      ).$class("field-row-stacked"),
      ...extraComponents
    );
  };

  function showHomeTab(homeTab) {
    if (Globals.homePageTab !== homeTab) {
      Globals.homePageTab = homeTab;
      root.$refresh();
    }
  }

  const home = () => {
    const usernameString = Globals.myUsername ?? "player";
    const homeTab = (id, name) => {
      return $li(
        $a(name)
      ).$role("tab")
       .$attribute("aria-selected", (Globals.homePageTab === id).toString())
       .$style("cursor", "pointer")
       .$onclick(() => showHomeTab(id));
    }
    const homeTabs = [
      homePlay,
      homeStats,
      homeLeaderboard,
      homeMaps
    ];

    return createWindow(
      {
        title: $span(`Welcome back ${usernameString}`).$id("home-title"),
        width: "300px",
        buttons: {
          close: doLogout,
        }
      },
      $div(
        $menu(
          homeTab(HOME_TAB_PLAY, "Play"),
          homeTab(HOME_TAB_MAPS, "Maps"),
          homeTab(HOME_TAB_STATS, "Statistics"),
          homeTab(HOME_TAB_LEADERBOARD, "Leaderboard")
        ).$role("tablist"),
        $div(
          homeTabs[Globals.homePageTab]()
        ).$role("tabpanel")
         .$class("window")
         .$style("padding", "12px")
      ).$class("window-body")
    );
  };
  
  const game = (gameId, playerToken, currentUser) => {
    $assert(
      typeof(gameId) === "number" &&
      typeof(playerToken) === "number" &&
      typeof(currentUser) == "string"
    );
  
    queueMicrotask(() => {
      Globals.gameWs = gloomLaunch(currentUser, gameId, playerToken, gotoHome);
    });
  
    return createWindow(
      {
        title: "GLOOM.EXE",
        buttons: {
          help: showGameHelp,
          close: gloomExit
        }
      },
      $div(
        $canvas().$id("viewport")
      ).$class("sunken")
       .$style("padding", "2px")
       .$style("margin", "2px")
    );
  };

  function getMapIndex(event) {
    const x = Math.floor(event.offsetX / MAP_CANVAS_SCALE);
    const y = Math.floor(event.offsetY / MAP_CANVAS_SCALE);
    // NOTE: This check makes sure that border pixels cannot be modified
    if (x < MAP_SIZE-1 && x > 0 && y < MAP_SIZE-1 && y > 0) {
      return x + y * MAP_SIZE;
    }
  }

  function getPlayerIndex(cellValue) {
    if (cellValue >= MAP_DRAW_PLAYER1 && cellValue <= MAP_DRAW_PLAYER4) {
      return cellValue - MAP_DRAW_PLAYER1;
    }
  }

  function renderMapCanvas() {
    const ctx = this._ctx;

    ctx.fillStyle = "white";
    ctx.fillRect(0, 0, this.width, this.height);

    ctx.strokeStyle = "gray";
    ctx.lineWidth = 0.5;
    ctx.textBaseline = "middle";
    ctx.textAlign = "center";
    this._map.forEach((cell, index) => {
      const x = (index % MAP_SIZE) * MAP_CANVAS_SCALE;
      const y = Math.floor(index / MAP_SIZE) * MAP_CANVAS_SCALE;
      if (cell !== MAP_DRAW_CLEAR) {
        ctx.fillStyle = MAP_COLORS[cell];
        ctx.fillRect(x, y, MAP_CANVAS_SCALE, MAP_CANVAS_SCALE);
      }
      ctx.strokeRect(x, y, MAP_CANVAS_SCALE, MAP_CANVAS_SCALE);

      const playerIndex = getPlayerIndex(cell);
      if (playerIndex != null) {
        const arrowIndex = this._getPlayerRotation(playerIndex);
        const arrow = MAP_DIRECTION_ARROWS[arrowIndex];
        ctx.fillStyle = "white";
        ctx.fillText(arrow, x + MAP_CANVAS_SCALE / 2, y + MAP_CANVAS_SCALE / 2);
      }
    });
  }

  function mapEditorSwitchMode(mode) {
    if (mode === this._drawMode || mode == null) {
      return;
    }
    if (this._drawMode != null) {
      this._drawButtons[this._drawMode].$attribute("class", null);
    }
    this._drawButtons[mode].$attribute("class", "active");
    this._prevDrawMode = this._drawMode;
    this._drawMode = mode;
  }

  function mapEditorOnScroll(event) {
    const index = getMapIndex(event);
    if (index == null) {
      return;
    }
    const playerIndex = getPlayerIndex(this._map[index]);
    if (playerIndex != null) {
      const delta = event.deltaY / MAP_SCROLL_SENSITIVITY;
      this._addPlayerRotation(playerIndex, delta);
      this._redraw();
    }
  }

  function mapEditorOnMouseEvent(event) {
    if (event.buttons > 0) {
      if (event.buttons === 2) {
        this._switchMode(MAP_DRAW_CLEAR);
      }
      const index = getMapIndex(event);
      if (this._map[index] !== this._drawMode) {
        if (this._drawMode >= MAP_DRAW_PLAYER1 && this._map.find(cell => cell === this._drawMode) != null) {
          return;
        }
        this._dirty = true;
        this._map[index] = this._drawMode;
        // Only redraw if we change the map
        this._redraw();
      }
    }
  }

  function mapEditorCancelClear(event) {
    if (event.button === 2 || event.buttons === 2) {
      this._switchMode(this._prevDrawMode);
    }
  }

  function mapEditorOnClose(event) {
    if ($("#map-editor")?._dirty === true) {
      const [wndEnable, wndDisable] = getWindowControls(event.target);
      wndDisable();
      showYesNoWindow(
        "The map was changed. Are you sure you want to quit without saving?",
        MSGWND_WARN, wndEnable,
        () => $goto("/"), () => {}
      );
    } else {
      $goto("/");
    }
  }

  function preventContextMenu(event) {
    event.preventDefault();
    event.stopPropagation();
  }

  function getPlayerRotation(index) {
    return Math.round(this._playerRotations[index]) % MAP_DIRECTION_ARROWS.length;
  }

  function addPlayerRotation(index, delta) {
    this._playerRotations[index] = (this._playerRotations[index] + delta) % MAP_DIRECTION_ARROWS.length;
    while (this._playerRotations[index] < 0) {
      this._playerRotations[index] += MAP_DIRECTION_ARROWS.length;
    }
  }

  const map = (mapId, mapData) => {
    $assert(typeof(mapId) === "number");
    $assert(mapData == null || typeof(mapData) === "string");

    const canvasSize = MAP_SIZE * MAP_CANVAS_SCALE;
    const canvas = $canvas().$id("map-editor")
                            .$attribute("width", canvasSize.toString())
                            .$attribute("height", canvasSize.toString())
                            .$on("mousemove", mapEditorOnMouseEvent)
                            .$on("mousedown", mapEditorOnMouseEvent)
                            .$on("mouseup", mapEditorCancelClear)
                            .$on("mouseleave", mapEditorCancelClear)
                            .$on("wheel", mapEditorOnScroll)
                            .$on("contextmenu", preventContextMenu);

    const map = new Uint8Array(MAP_SIZE * MAP_SIZE);
    const playerRotations = [0, 0, 0, 0];

    if (mapData != null) {
      // Deserialize map data
      const serialized = Uint8Array.fromBase64(mapData);
      const view = new DataView(serialized.buffer, serialized.byteOffset, 8);

      let bitmap_pos = view.byteLength * 8;
      for (let i = 0; i < map.length; i++) {
        const x = i % MAP_SIZE;
        const y = Math.floor(i / MAP_SIZE);
        if (x === MAP_SIZE-1 || x === 0 || y === MAP_SIZE-1 || y === 0) {
          map[i] = MAP_DRAW_WALL;
          continue;
        }
        const bit = bitmap_pos & 7;
        const byte = bitmap_pos >> 3;
        map[i] = (serialized[byte] & (1 << bit)) !== 0 ? MAP_DRAW_WALL : MAP_DRAW_CLEAR;
        ++bitmap_pos;
      }

      for (let i = 0; i < playerRotations.length; i++) {
        const u16 = view.getUint16(i << 1, true);
        const x = u16 & 0b11111;
        const y = (u16 >> 5) & 0b11111;
        const r = u16 >> 10;

        $assert(x > 0 && x < MAP_SIZE-1);
        $assert(y > 0 && y < MAP_SIZE-1);
        map[x + y * MAP_SIZE] = i + MAP_DRAW_PLAYER1;
        playerRotations[i] = r;
      }
    } else {
      // Initialize empty map
      for (let i = 0; i < map.length; i++) {
        const x = i % MAP_SIZE;
        const y = Math.floor(i / MAP_SIZE);
        map[i] = (x === MAP_SIZE-1 || x === 0 || y === MAP_SIZE-1 || y === 0) ? MAP_DRAW_WALL : MAP_DRAW_CLEAR;
      }
    }

    const drawButton = (id, text) => {
      return $button(text).$style("color", MAP_COLORS[id])
                          .$style("font-weight", "bold")
                          .$style("font-size", "10px")
                          .$style("flex", "1 1 0px")
                          .$style("margin", "0px 4px 2px 4px")
                          .$onclick(() => canvas._switchMode(id));
    };

    const buttons = [
      drawButton(MAP_DRAW_CLEAR,     "CLEAR").$style("text-shadow", "1px 0 black, -1px 0 black, 0 1px black, 0 -1px black"),
      drawButton(MAP_DRAW_WALL,      "WALL"),
      drawButton(MAP_DRAW_PLAYER1,   "PLAYER 1"),
      drawButton(MAP_DRAW_PLAYER2,   "PLAYER 2"),
      drawButton(MAP_DRAW_PLAYER3,   "PLAYER 3"),
      drawButton(MAP_DRAW_PLAYER4,   "PLAYER 4"),
    ];

    canvas._ctx = canvas.getContext("2d");
    canvas._dirty = false;
    canvas._drawMode = null;
    canvas._prevDrawMode = null;
    canvas._drawButtons = buttons;
    canvas._playerRotations = playerRotations;
    canvas._map = map;
    canvas._mapId = mapId;
    canvas._redraw = renderMapCanvas;
    canvas._switchMode = mapEditorSwitchMode;
    canvas._getPlayerRotation = getPlayerRotation;
    canvas._addPlayerRotation = addPlayerRotation;

    canvas._redraw();
    canvas._switchMode(MAP_DRAW_WALL);

    return createWindow(
      {
        title: "Map Editor",
        buttons: {
          help: showMapEditorHelp,
          close: mapEditorOnClose
        }
      },
      $div(
        $div(
          ...buttons
        ).$style("display", "flex"),
        $div(
          canvas
        ).$class("sunken")
         .$style("padding", "2px 2px 0px 2px")
         .$style("margin", "2px"),
        $div(
          $button("Cancel").$onclick(mapEditorOnClose),
          $button("Save").$class("default")
                         .$style("margin-left", "8px")
                         .$onclick(doSaveMap)
        ).$style("padding", "8px")
         .$style("float", "right")
      ).$style("padding", "4px")
    );
  };
  
  api.get("/session/refresh").then(res => {
    const initialRoute = res.status === 200 ? $route() : "/login";
    $router(
      {
        $first: initialRoute,
        $default: "/login",
        "/": {
          onRoute: home,
          onEnter: () => {
            doValidateSession()
              .then(success => {
                if (success) {
                  refreshStats();
                  refreshMyGameId();
                  fetchMaps();
                  $interval(300000, refreshStats); // Refresh scoreboard every 5 minutes.
                  $interval(5000, refreshMyGameId); // Refresh game id every 5 seconds.
                  root.$refresh();
                } else {
                  $goto("/login");
                }
              });
          }
        },
        "/login": {
          onRoute: login,
          onEnter: () => {
            doValidateSession()
              .then(success => {
                if (success) $goto("/");
              });
          }
        },
        "/login/help": loginHelp,
        "/signup": signup,
        "/map": map,
        "/game": {
          onRoute: game,
          onLeave: () => {
            if (Globals.gameWs != null && Globals.gameWs.readyState !== WebSocket.CLOSED) {
              Globals.gameWs.close();
            }
            Globals.gameWs = undefined;
          }
        }
      },
      {
        onError: (exc) => {
          console.error(exc);
          $goto("/");
        },
      }
    );
  });
});
