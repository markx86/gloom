window.$assert = function (condition, message = undefined) {
  if (condition !== true) {
    throw new Error(message ?? "assertion failed!");
  }
};

const $_routerArgs = new Map();

function $_getArgsForRoute(location) {
  const args = $_routerArgs.get(location);
  $_routerArgs.delete(location);
  return args;
}

function $_callOnDestroy(tag) {
  for (const child of tag.children) {
    $_callOnDestroy(child);
  }
  if (tag.ondestroy != null) {
    tag.ondestroy();
  }
}

function $_setFunctions(tag) {
  tag.$attribute = function (name, value) {
    $assert(typeof(name) === "string", "attribute name must be a string");
    if (value === undefined) {
      // function used as a getter
      return this.getAttribute(name);
    } else if (value === null) {
      // function used to remove an attribute
      this.removeAttribute(name);
    } else {
      // function used to set an attribute
      $assert(typeof(value) === "string", "attribute value must be string");
      this.setAttribute(name, value);
    }
    return this;
  };
  tag.$aria = function (label) {
    $assert(typeof(label) === "string", "aria label must be a string");
    this.setAttribute("aria-label", label);
    return this;
  };
  tag.$for = function (_for) {
    $assert(typeof(_for) === "string", "for's id must be a string");
    this.setAttribute("for", _for);
    return this;
  };
  tag.$type = function (type) {
    $assert(typeof(type) === "string", "type must be a string");
    this.setAttribute("type", type);
    return this;
  };
  tag.$id = function (id) {
    $assert(typeof(id) === "string", "id must be a string");
    this.setAttribute("id", id);
    return this;
  };
  tag.$class = function (name) {
    $assert(typeof(name) === "string", "class name must be a string");
    this.setAttribute("class", name);
    return this;
  };
  tag.$disable = function () {
    this.setAttribute("disabled", "");
    return this;
  };
  tag.$enable = function () {
    this.removeAttribute("disabled");
    return this;
  };
  tag.$onclick = function (callback) {
    $assert(callback == null || callback instanceof Function, "callback must be a function");
    this.onclick = callback;
    return this;
  };
  tag.$ondestroy = function (callback) {
    $assert(callback == null || callback instanceof Function, "callback must be a function");
    this.ondestroy = callback;
    return this;
  };
  tag.$style = function (key, value) {
    $assert(typeof(key) === "string", "style attribute must be a string");
    this.style[key] = value;
    return this;
  };
  tag.$on = function (event, callback, options) {
    $assert(typeof(event) === "string", "event name must be a string");
    $assert(callback instanceof Function, "callback must be a function");
    $assert(options === undefined || typeof(options) === "object" || typeof(options) === "boolean", "options must be either a boolean or an object");
    this.addEventListener(event, callback, options);
    return this;
  }
  tag.$add = function (...children) {
    children.forEach(child => tag.appendChild(typeof(child) === "string" ? document.createTextNode(child) : child));
    return this;
  }
  tag.$destroy = function () {
    $_callOnDestroy(this);
    if (this.isConnected) {
      this.remove();
    }
  };
}

window.$tag = function (name, ...children) {
  const tag = document.createElement(name);
  $_setFunctions(tag);
  tag.$add(...children);
  return tag;
};

window.$ = function (ident) {
  $assert(typeof(ident) === "string", "identifier must be a string");
  const type = ident.charAt(0);
  if (type === '#') {
    return document.getElementById(ident.substring(1));
  } else if (type === '.') {
    return document.getElementsByClassName(ident.substring(1));
  } else {
    return document.getElementsByTagName(ident);
  }
};

window.$img = function (src, width, height) {
  $assert(typeof(src) === "string", "src must be a string");
  return $tag("img").$attribute("src", src)
                    .$style("width", width ?? "")
                    .$style("height", height ?? "");
};

window.$goto = function (route, ...args) {
  $assert(typeof(route) === "string", "route must be a string");
  if (args.length > 0) {
    $_routerArgs.set(route, args);
  }
  document.location.hash = route.replace('/', '$').replace(/\//, '-');
};

window.$route = function () {
  return document.location.hash
      .replace('#', '')
      .replace('$', '/') // replace root character
      .replace(/-/, '/'); // replace separator character
};

[
  "div", "h1", "h2", "h3", "h4", "h5", "h6", "p", "a",
  "br", "hr", "canvas", "button", "input", "label", "ul", "li",
  "strong", "code"
].forEach(tag => window[`$${tag}`] = (...children) => $tag(tag, ...children));

window.$root = function (node) {
  if (node === undefined) {
    // used as a getter
    $assert(window.$_root != null, "no root node defined");
    return window.$_root;
  } else {
    // used as a setter
    $_setFunctions(node);
    window.$_root = node;
  }
}

window.$router = function (routes, errorCallback) {
  const result = $root();
  $assert(result != null, "no root node found!");
  const errorHandler = errorCallback ?? console.error;

  function _route() {
    const location = $route();
    if (location.length === 0) {
      $goto(routes.$default ?? "/");
    } else {
      $assert(location.charAt(0) === '/', "route must start with a /");
      $assert(location in routes, `unknown route ${location}`);
      $assert(routes[location] instanceof Function, "routes must be functions");

      const args = $_getArgsForRoute(location);
      const routeFn = routes[location];
      $assert(
        (routeFn.length === 0 && args == null) ||
        (args != null && routeFn.length === args.length),
        `invalid parameters passed to route ${location}`
      );

      const content = args == null ? routeFn() : routeFn(...args);
      if (content != null) {
        result.replaceChildren(content);
      }
    }
  }

  function route() {
    try {
      _route();
    } catch (e) {
      errorHandler(e);
    }
  }

  if (routes.$first != null) {
    $goto(routes.$first)
  }
  route();
  window.addEventListener("hashchange", route);
  result.$refresh = route;

  return result;
};
