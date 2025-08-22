import * as api from "./api.js";
import * as gloom from "./gloom.js";
import "./reactive.js";
import {
  showErrorWindow, showInfoWindow, showWarningWindow,
  createWindow, getWindowControls,
  helpLink, windowIcon, separator,
  MSGWND_HELP, showMessageWindow
} from "./windowing.js";

let currentUser;
let currentGameId;
let intervalId;
let gameWs;

$root($("#root"));
// disable scroll bars
document.body.style.overflow = "hidden";

function updateRootNodeSize() {
  $root().style.width = `${window.innerWidth}px`;
  $root().style.height = `${window.innerHeight}px`;
}
updateRootNodeSize();
window.addEventListener("resize", updateRootNodeSize);

let gloomLauncher, gloomExit;

gloom.loadGloom().then(([launcher, exit]) => {
  gloomLauncher = launcher;
  gloomExit = exit;
});

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
const quitGame = () => (gloomExit == null ? gotoHome() : gloomExit());

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
    // on success we redirect to the login page
    // an error (403) means the user does not have a valid session anyway, so redirect to login again
    $goto("/login");
  });
}

function refreshGameId() {
  api.get("/game/id")
    .then(res => {
      if (res.status === 200) {
        return res.json();
      } else if (res.status === 404) {
        $("#game-id").textContent = "No running game";
        $("#btn-create").$enable();
      } else {
        $("#game-id").textContent = "Could not fetch game ID";
      }
      $("#field-game-id").placeholder = "Enter game ID";
      return null;
    })
    .then(data => {
      if (typeof(data?.gameId) === "number") {
        setGameId(data.gameId);
      }
    })
    .catch(_ => $("#game-id").textContent = "Could not fetch game ID");
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

function setGameId(gameId) {
  if (gameId !== currentGameId) {
    const gameIdString = zeroPadL(gameId.toString(16), 8).toUpperCase();
    $("#game-id").textContent = gameIdString;
    $("#field-game-id").placeholder = `Enter game ID (default: ${gameIdString})`;
    $("#btn-create").$disable();
    currentGameId = gameId;
  }
}

function getGameId() {
  const gameIdString = $("#field-game-id")?.value;
  if (gameIdString == null || gameIdString.length === 0) {
    return currentGameId;
  } else if (gameIdString.length === 8) {
    const gameId = parseInt(gameIdString, 16);
    if (isFinite(gameId) && !isNaN(gameId)) {
      return gameId;
    }
  }
  return undefined;
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
      setGameId(data.gameId);
      // keep the create button disabled
      button.$disable();
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
      $goto("/game", gameId, data.wssPort, data.playerToken);
    } else {
      showErrorWindow(data.message, wndEnable);
    }
  } catch {
    showWarningWindow("An unknown error occurred while trying to join the game. Please try again later.", wndEnable);
  }
}

function controlHelp(key, help) {
  return $p($strong(key), " ► ", help);
}

function showGameHelp(event) {
  const [wndEnable, wndDisable] = getWindowControls(event.target);
  wndDisable();
  showMessageWindow(
    $div(
      $h5("Controls").$style("margin", "12px 0px")
                     .$style("font-size", "20px"),
      $ul(
        $li(controlHelp("W", "Move forwards")),
        $li(controlHelp("S", "Move backwards")),
        $li(controlHelp("A", "Strage left")),
        $li(controlHelp("D", "Strage right")),
        $li(controlHelp("P", "Pauses the game")),
        $li(controlHelp("Mouse movement", "Move the camera left and right")),
        $li(controlHelp("Mouse buttons", "Shoot"))
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
  api.get("/session/validate")
    .then(res => {
      if (res.status === 200) {
        $goto("/");
      }
    });
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
          // bump the filed by one pixel, to align it with the username field
          // (very hacky, but it works also I hate css :D)
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
     .$style("align-items", "center top")
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

const home = () => {
  currentGameId = undefined;
  api.get("/session/validate")
    .then(res => {
      if (res.status !== 200) {
        throw Error("Not authenticated")
      } else {
        return res.json();
      }
    })
    .then(data => {
      currentUser = data?.username ?? "player";
      $("#home-title").textContent = `Welcome back ${currentUser}`;
    })
    .catch(() => $goto("/login"));
  // do not use a parameter to avoid headaches
  refreshGameId();
  intervalId = setInterval(refreshGameId, 5000); // refresh game id every 5 seconds
  return createWindow(
    {
      title: $span("Welcome back player").$id("home-title"),
      width: "300px",
      buttons: {
        close: doLogout,
      }
    },
    $div(
      $div(
        $label("Your game").$for("game-id")
                           .$style("font-weight", "bold")
                           .$style("margin-bottom", "8px"),
        $div(
          $p("Fetching game ID...").$id("game-id")
                                   .$style("margin", "0px")
                                   .$style("flex-grow", "1"),
          $button("Create").$id("btn-create")
                           .$style("margin-left", "12px")
                           .$onclick(doCreateGame)
                           .$disable()
        ).$style("display", "flex")
         .$style("align-items", "center")
      ),
      separator().$style("margin", "12px 0px")
                 .$style("width", "100%"),
      $div(
        $label("Join a game").$for("field-game-id").$style("font-weight", "bold"),
        $div(
          $input().$id("field-game-id")
                  .$type("text")
                  .$attribute("placeholder", "Enter a game ID")
                  .$style("flex-grow", "1")
                  .$on("input", validateInput),
          $button("Join").$style("margin", "0px 0px 0px 8px")
                         .$onclick(doJoinGame)
        ).$style("display", "flex")
         .$style("align-items", "center")
      ).$class("field-row-stacked")
    ).$style("padding", "12px")
  );
};

const game = (gameId, wssPort, playerToken) => {
  $assert(
    typeof(gameId) === "number" &&
    typeof(wssPort) === "number" &&
    typeof(playerToken) === "number" &&
    currentUser != null
  );

  (new Promise(resolve => resolve()))
    .then(() => gameWs = gloomLauncher(currentUser, wssPort, gameId, playerToken, gotoHome));

  return createWindow(
    {
      title: "GLOOM.EXE",
      buttons: {
        help: showGameHelp,
        close: quitGame
      }
    },
    $div(
      $canvas().$id("viewport")
    ).$style("box-shadow", "inset -1px -1px #fff, inset 1px 1px grey, inset -2px -2px #dfdfdf, inset 2px 2px #0a0a0a")
     .$style("padding", "2px")
     .$style("margin", "2px")
  );
};

api.get("/session/refresh").then(res => {
  const initialRoute = res.status === 200 ? $route() : "/login";
  $router(
    {
      $first: initialRoute,
      $default: "/login",
      "/": home,
      "/login": login,
      "/login/help": loginHelp,
      "/signup": signup,
      "/game": game
    },
    {
      onError: () => $goto("/"),
      onBeforeRoute: () => {
        // clear any interval that's running
        if (intervalId != null) {
          clearInterval(intervalId);
          intervalId = undefined;
        }
        // close dangling WebSocket
        if (gameWs != null) {
          if (gameWs.readyState !== WebSocket.CLOSED) {
            gameWs.close();
          }
          gameWs = undefined;
        }
      }
    }
  );
});
