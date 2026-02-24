window.$assert = function (condition, message = undefined) {
  if (condition !== true) {
    throw new Error(message ?? "assertion failed!");
  }
};

const $_globalState = {
  routerArgs: new Map(),
  timeouts: new Set(),
  intervals: new Set()
};

function $_getArgsForRoute(location) {
  const args = $_globalState.routerArgs.get(location);
  $_globalState.routerArgs.delete(location);
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
      // Function used as a getter.
      return this.getAttribute(name);
    } else if (value === null) {
      // Function used to remove an attribute.
      this.removeAttribute(name);
    } else {
      // Function used to set an attribute.
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
    $assert(typeof(_for) === "string", "id for attribute 'for' must be a string");
    this.setAttribute("for", _for);
    return this;
  };
  tag.$role = function (role) {
    $assert(typeof(role) === "string", "id for attribute 'role' must be a string");
    this.setAttribute("role", role);
    return this;
  }
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
  tag.$disable = function (yes) {
    $assert(yes === undefined || typeof(yes) === "boolean", "value must be a boolean")
    if (yes === undefined || yes) this.setAttribute("disabled", "");
    else this.$enable();
    return this;
  };
  tag.$enable = function (yes) {
    $assert(yes === undefined || typeof(yes) === "boolean", "value must be a boolean")
    if (yes === undefined || yes) this.removeAttribute("disabled");
    else this.$disable();
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
  };
  tag.$add = function (...children) {
    children.forEach(child => tag.appendChild(typeof(child) === "string" ? document.createTextNode(child) : child));
    return this;
  };
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
  const results = document.querySelectorAll(ident);
  if (results.length === 0) {
    return;
  } else if (results.length === 1) {
    return results[0];
  } else {
    return [...results];
  }
};

function $_removeTimeout(timeoutId) {
  if ($_globalState.timeouts.delete(timeoutId)) {
    clearTimeout(timeoutId);
  }
}

window.$timeout = function (delay, callback, ...args) {
  if (callback == null) {
    $assert(typeof(delay) === "number", "timeoutId must be a number");
    $_removeTimeout(delay);
  } else {
    $assert(typeof(delay) === "number", "delay must be a number");
    const id = setTimeout((...args) => {
      callback(...args);
      $_removeTimeout(id);
    }, delay, ...args);
    $_globalState.timeouts.add(id);
  }
}

function $_removeInterval(intervalId) {
  if ($_globalState.intervals.delete(intervalId)) {
    clearInterval(intervalId);
  }
}

window.$interval = function(period, callback, ...args) {
  if (callback == null) {
    $assert(typeof(period) === "number", "intervalId must be a number");
    $_removeInterval(period);
  } else {
    $assert(typeof(period) === "number", "period must be a number");
    const id = setInterval(callback, period, ...args);
    $_globalState.intervals.add(id);
  }
}

function $_clearAllTimers() {
  $_globalState.timeouts.forEach(timeoutId => clearTimeout(timeoutId));
  $_globalState.intervals.forEach(intervalId => clearInterval(intervalId));
  $_globalState.timeouts.clear();
  $_globalState.intervals.clear();
}

window.$img = function (src, width, height) {
  $assert(typeof(src) === "string", "src must be a string");
  return $tag("img").$attribute("src", src)
                    .$style("width", width ?? "")
                    .$style("height", height ?? "");
};

window.$goto = function (route, ...args) {
  $assert(typeof(route) === "string", "route must be a string");
  if (args.length > 0) {
    $_globalState.routerArgs.set(route, args);
  }
  $_clearAllTimers();
  document.location.hash = route.replace('/', '$').replace(/\//, '-');
};

window.$route = function (hash = document.location.hash) {
  return hash?.replace('#', '')
              .replace('$', '/') // Replace root character.
              .replace(/-/, '/') // Replace separator character.
         ?? "/";
};

[
  "div", "h1", "h2", "h3", "h4", "h5", "h6", "p", "a",
  "br", "hr", "canvas", "button", "input", "label", "ul", "li",
  "strong", "code", "span", "menu", "table", "th", "td", "tr", "thead", "tbody"
].forEach(tag => window[`$${tag}`] = (...children) => $tag(tag, ...children));

window.$root = function (node) {
  if (node === undefined) {
    // Used as a getter.
    $assert(window.$_root != null, "no root node defined");
  } else {
    // Used as a setter.
    $_setFunctions(node);
    window.$_root = node;
  }
  return window.$_root;
}

window.$router = function (routes, callbacks) {
  const root = $root();
  $assert(root != null, "no root node found!");
  const errorHandler = callbacks?.onError ?? console.error;

  function _route(event) {
    const location = $route();
    const prevLocation = $route(event?.oldURL?.split("#").pop());
    if (location.length === 0) {
      $goto(routes.$default ?? "/");
    } else {
      $assert(location.charAt(0) === '/', "route must start with a /");
      $assert(location in routes, `unknown route ${location}`);

      const route = routes[location];

      // Call onLeave callback.
      if (prevLocation in routes) {
        const prevRoute = routes[prevLocation];
        // Per route onLeave callback.
        if (prevRoute.onLeave == null || !prevRoute.onLeave(location, prevLocation)) {
          // Global onLeave callback.
          if (callbacks?.onLeave) callbacks.onLeave(location, prevLocation);
        }
      }

      let routeFn;
      if (typeof(route) === "function") {
        routeFn = route;
      } else if (typeof(route) === "object") {
        $assert(route.onRoute != null, "route.onRoute cannot be null/undefined");
        routeFn = route.onRoute;
      } else {
        $assert(false, `routes must be either functions or objects (got ${typeof(route)})`);
      }

      const args = $_getArgsForRoute(location) ?? [];
      const boundRouteFn = routeFn.bind(undefined, ...args);

      root.$refresh = function () {
        const content = boundRouteFn();
        if (content != null) {
          this.replaceChildren(content);
        }
      }
      root.$refresh();

      // Per route onEnter callback.
      if (route.onEnter == null || !route.onEnter (location, prevLocation)) {
        // Global onEnter callback.
        if (callbacks?.onEnter ) callbacks.onEnter (location, prevLocation);
      }
    }
  }

  function route(evt) {
    try {
      _route(evt);
    } catch (e) {
      errorHandler(e);
    }
  }

  if (routes.$first != null) {
    $goto(routes.$first)
  }
  route();
  window.addEventListener("hashchange", route);

  return root;
};
