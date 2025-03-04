(async () => {
  // remove margin from body to make canvas fullscreen
  document.body.style.margin = "0";

  const textDecoder = new TextDecoder("utf-8");

  const canvas = document.getElementById("viewport");
  const ctx = canvas.getContext("2d");
  // set transform origin to top-left corner
  canvas.style.transformOrigin = "0 0";
  canvas.style.imageRendering = "crisp-edges";

  let memory = null;
  let fbView = null;
  let fb = null;

  // void write(i32 fd, const char* str, u32 len);
  function write(fd, p, l) {
    const s = textDecoder.decode(memory.buffer.slice(p, p + l));
    switch (fd) {
      case 1:
        console.log(s);
        break;
      case 2:
        console.error(s);
        break;
      default:
        break;
    }
  }

  // u32 request_mem(u32 size);
  function request_mem(sz) {
    if (!memory) {
      return 0;
    }
    const pages = (sz + (1 << 16) - 1) >> 16;
    memory.grow(pages);
    return pages << 16;
  }

  function register_fb(addr, width, height, size) {
    if (!memory) {
      return;
    }
    fbView = new Uint8ClampedArray(memory.buffer, addr, size);
    fb = new ImageData(fbView, width, height);
    canvas.width = width;
    canvas.height = height;
  }

  function pointerIsLocked() {
    return document.pointerLockElement === canvas;
  }

  // void pointer_lock(void);
  async function pointer_lock() {
    if (pointerIsLocked()) {
      return;
    }

    try {
      await canvas.requestPointerLock({
        unadjustedMovement: true
      });
    } catch (error) {
      if (error.name === "NotSupportedError") {
        console.warn("disabling mouse acceleration is not supported: %s", error);
        await canvas.requestPointerLock();
      } else {
        console.error("could not lock pointer: %s", error);
      }
    }
  }

  // void pointer_release(void);
  function pointer_release() {
    document.exitPointerLock();
  }

  const importObject = {
    env: {
      write,
      pointer_lock,
      pointer_release,
      request_mem,
      register_fb,
    },
  };

  const wasmBase64 = "@@WASMB64@@";
  const wasmBytes = Uint8Array.from(atob(wasmBase64), c => c.charCodeAt(0)).buffer;
  const obj = await WebAssembly.instantiate(wasmBytes, importObject);
  const instance = obj.instance;

  memory = instance.exports.memory;

  // init game
  instance.exports.init();

  function updateViewportSize() {
    // shamelessly stolen from
    // https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API/Tutorial/Optimizing_canvas#scaling_canvas_using_css_transforms
    const xScale = window.innerWidth / canvas.width;
    const yScale = window.innerHeight / canvas.height;
    const scaleToFit = Math.min(xScale, yScale);
    canvas.style.transform = `scale(${scaleToFit})`;
  }

  function processKeyEvent(event) {
    instance.exports.key_event(event.keyCode, event.key.charCodeAt(0), event.type === "keydown");
  }

  window.addEventListener("resize", updateViewportSize);
  window.addEventListener("keydown", processKeyEvent);
  window.addEventListener("keyup", processKeyEvent);
  document.addEventListener("pointerlockchange", () => instance.exports.set_pointer_locked(pointerIsLocked()));
  canvas.addEventListener("click", () => instance.exports.mouse_click());
  canvas.addEventListener("mousemove", e => instance.exports.mouse_moved(e.layerX, e.layerY, e.movementX, e.movementY));
  updateViewportSize();

  let prevTimestamp;

  function tick(timestamp) {
    const delta = (timestamp - prevTimestamp) / 1000;
    instance.exports.tick(delta);
    ctx.putImageData(fb, 0, 0);
    prevTimestamp = timestamp;
    window.requestAnimationFrame(tick);
  }

  window.requestAnimationFrame((timestamp) => {
    prevTimestamp = timestamp;
    window.requestAnimationFrame(tick);
  });
})();
