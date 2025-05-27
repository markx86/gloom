import * as api from "./api.js";
import * as gloom from "./gloom.js";
import { showErrorWindow, showInfoWindow, showWarningWindow, createWindow, getWindow, helpLink, windowIcon } from "./windowing.js";
import "./reactive.js";

$root($("#root"));

function updateRootNodeSize() {
  $root().style.width = `${window.innerWidth}px`;
  $root().style.height = `${window.innerHeight}px`;
}
updateRootNodeSize();
window.addEventListener("resize", updateRootNodeSize);

gloom.loadGloom();

const gotoHelp = () => $goto("/login/help");

async function doLogin(event) {
  const wnd = getWindow(event.target);
  $assert(wnd != null, "button is not attached to a window");
  const wndDisable = () => wnd.setDisabled(true);
  const wndEnable = () => wnd.setDisabled(false);

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

  const response = await api.post("/login", {
    username: username,
    password: password
  });
  if (response.status === 200) {
    $goto("/");
  } else {
    showErrorWindow("Invalid username or password. Please check your credentials and try again.", wndEnable);
  }
}

async function doRegister(event) {
  const wnd = getWindow(event.target);
  $assert(wnd != null, "button is not attached to a window");
  const wndDisable = () => wnd.setDisabled(true);
  const wndEnable = () => wnd.setDisabled(false);

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
}

async function doLogout(event) {
  const button = event.target;
  const wnd = getWindow(button);
  $assert(wnd != null, "button is not attached to a window");
  button.$disable();
  wnd.setDisabled(true);
  api.get("/logout").then(_ => {
    // on success we redirect to the login page
    // an error (403) means the user does not have a valid session anyway, so redirect to login again
    $goto("/login");
  });
}

function refreshSession(onSuccessRoute, onFailureRoute) {
  api.get("/refresh-session").then(res => {
    if (res.status === 200) {
      onSuccessRoute != null ? $goto(onSuccessRoute) : null;
    } else {
      onFailureRoute != null ? $goto(onFailureRoute) : null;
    }
  });
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

const loginHelp = () => createWindow(
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

const signup = () => {
  return createWindow(
    {
      title: "Create a Gloom account",
      width: "400px",
      buttons: {
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
      $hr().$style("width", "95%")
           .$style("margin", "0% 2.5%")
           .$style("border", "none")
           .$style("height", "1px")
           .$style("opacity", "0.5")
           .$style("background", "linear-gradient( to right, red 20%, yellow 20%, yellow 36%, green 36%, green 60%, blue 60%, blue 100% )"),
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
  return createWindow(
    {
      title: "Home of Gloom",
      buttons: {
        close: doLogout
      }
    },
    $div(
      "todo"
    )
  );
};

$router({
  "/": home,
  "/login": login,
  "/login/help": loginHelp,
  "/signup": signup
});

refreshSession("/", "/login");
