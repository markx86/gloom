import "./reactive.js";

const MSGWND_ERROR = "error";
const MSGWND_WARN  = "warning";
const MSGWND_INFO  = "info";

function capitalize(str) {
  $assert(typeof(str) === "string", "can only capitalize strings");
  return str.charAt(0).toUpperCase() + str.substring(1).toLowerCase();
}

function setDisabledRecursive(obj, yes) {
  for (const child of obj.children) {
    $assert(child.$attribute != null, "trying to disable a non prev.js component");
    setDisabledRecursive(child, yes);
  }
  obj.$attribute("disabled", yes ? "" : null);
}

export function getWindow(component) {
  $assert(component != null, "component cannot be null or undefined");
  while (component != null) {
    if (component.getAttribute("class") === "window") {
      return component;
    } else {
      component = component.parentElement;
    }
  }
  return undefined;
}

export function icon(path, width, height) {
  return $img(path, width, height)
    .$style("image-rendering", "pixelated");
}

export function windowIcon(path, width, height) {
  return icon(path, width, height)
    .$style("margin", "8px 4px 8px 4px");
}

export function helpLink(message, link) {
  $assert(typeof(message) === "string", "message must be a string");
  $assert(typeof(link) === "string", "link must be a string");
  $assert(link.charAt(0) === '/', "link must start with a /");
  const href = link.substring(1).replace(/\//, '-');
  return $a(
    icon("/static/img/arrow.png", "16px", "16px").$style("margin", "0px 4px"),
    message
  ).$attribute("href", `#${href}`);
}

export function closeWindow(event) {
  const window = getWindow(event.target);
  $assert(window != null, "child is not attached to window");
  window.$destroy();
}

export function createWindow(options, ...children) {
  const title = options.title ?? "Untitled window";
  const width = options.width ?? "fit-content";
  const height = options.height ?? "fit-content";
  const buttons = options.buttons ?? { close: closeWindow };
  $assert(buttons instanceof Object, "options.buttons must be an object");
  const wnd =
    $div(
      $div(
        $div(title).$class("title-bar-text"),
        $div(
          ...Object.entries(buttons)
                   .map(values => $button().$aria(capitalize(values[0])).$onclick(values[1]))
        ).$class("title-bar-controls")
      ).$class("title-bar"),
      ...children
    ).$class("window")
     .$style("position", "absolute")
     .$style("width", width)
     .$style("height", height)
     // always make windows centered
     .$style("left", "50%")
     .$style("top", "50%")
     .$style("transform-origin", "center")
     .$style("transform", "translate(-50%, -50%)");
  wnd.setDisabled = function (yes) {
    setDisabledRecursive(this, yes);
    const titleBars = this.getElementsByClassName("title-bar");
    $assert(titleBars.length === 1, "No title-bar or multiple title-bars in window");
    titleBars[0].$class(yes ? "title-bar inactive" : "title-bar");
  }
  return wnd;
}

function createMessageWindow(content, type, onclose) {
  return createWindow(
    {
      title: capitalize(type),
    },
    $div(
      windowIcon(`/static/img/${type}.png`, "32px", "32px"),
      $div(
        $p(content),
      ).$style("padding", "0px 8px")
    ).$style("padding", "8px 12px 0px 12px")
     .$style("display", "flex"),
    $div(
      $button("OK").$class("default").$style("width", "75px").$onclick(closeWindow)
    ).$style("padding", "8px 8px 12px 8px")
     .$style("display", "flex")
     .$style("flex-direction", "column")
     .$style("align-items", "center")
  ).$style("max-width", "400px")
   .$ondestroy(onclose);
}

function showMessageWindow(message, type, onclose) {
  $root().$add(createMessageWindow(message, type, onclose));
}

export function showErrorWindow(message, onclose) {
  showMessageWindow(message, MSGWND_ERROR, onclose);
}

export function showWarningWindow(message, onclose) {
  showMessageWindow(message, MSGWND_WARN, onclose);
}

export function showInfoWindow(message, onclose) {
  showMessageWindow(message, MSGWND_INFO, onclose);
}

