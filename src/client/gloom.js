// NOTE: this scripts requires reactive.js

export async function loadGloom() {
  const textDecoder = new TextDecoder("utf-8");
  const canvasDefaultWidth = 640;
  const canvasDefaultHeight = 480;

  let canvasContainer = null, canvas = null, ctx = null, fb = null;
  let originTime = 0;
  let ws = null;
  let settingsKey = null;
  
  function updateViewportSize() {
    const aspectRatio = fb == null ? (canvasDefaultWidth / canvasDefaultHeight) : (fb.width / fb.height);
    const parentWidth = window.innerWidth;
    const parentHeight = window.innerHeight;
    // Resize the canvas container.
    let containerWidth, containerHeight;
    if (parentWidth < parentHeight) {
      containerWidth = parentWidth * 0.75;
      containerHeight = containerWidth / aspectRatio;
    } else {
      containerHeight = parentHeight * 0.75;
      containerWidth = containerHeight * aspectRatio;
    }
    canvasContainer.style.width = `${containerWidth}px`;
    canvasContainer.style.height = `${containerHeight}px`;
    // Shamelessly stolen from
    // https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API/Tutorial/Optimizing_canvas#scaling_canvas_using_css_transforms
    const xScale = containerWidth / canvas.width;
    const yScale = containerHeight / canvas.height;
    const scaleToFit = Math.min(xScale, yScale);
    canvas.style.transform = `scale(${scaleToFit})`;
  }
  
  // void platform_write(i32 fd, const char* str, u32 len);
  function platform_write(fd, p, l) {
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

  function getPages(bytes) {
    return (bytes + ((1 << 16) - 1)) >> 16;
  }

  function getFramebufferSizes() {
    return [instance.exports.gloom_framebuffer_width(), instance.exports.gloom_framebuffer_height()];
  }

  function allocateFrameBuffer() {
    const [width, height] = getFramebufferSizes();
    const fbStride = width;
    const fbSize = 4 * fbStride * height;
    const zbSize = 4 * width;
    const fbAddress = instance.exports.__heap_base;
    memory.grow(getPages(fbSize + zbSize));
    instance.exports.gloom_framebuffer_set(fbAddress, fbAddress + fbSize, fbStride);
    return [fbAddress, width, height, fbSize];
  }
  
  function createFramebuffer() {
    const [fbAddress, fbWidth, fbHeight, fbSize] = allocateFrameBuffer();
    const fbArray = new Uint8ClampedArray(memory.buffer, fbAddress, fbSize);
    fb = new ImageData(fbArray, fbWidth, fbHeight);
    canvas.width = fbWidth;
    canvas.height = fbHeight;
    updateViewportSize();
  }
  
  function pointerIsLocked() {
    return document.pointerLockElement === canvas;
  }
  
  // void platform_pointer_lock(void);
  async function platform_pointer_lock() {
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
  
  // void platform_pointer_release(void);
  function platform_pointer_release() {
    document.exitPointerLock();
  }
  
  // i32 platform_send_packet(void* pkt, u32 len);
  function platform_send_packet(bufptr, len) {
    try {
      ws.send(memory.buffer.slice(bufptr, bufptr + len));
      return len;
    } catch {
      return -1;
    }
  }

  // void platform_settings_store(f32 drawdist, f32 fov, f32 mousesens, b8 camsmooth);
  function platform_settings_store(drawdist, fov, mousesens, camsmooth) {
    camsmooth = camsmooth !== 0;
    const settings = { mousesens, drawdist, fov, camsmooth };
    localStorage.setItem(settingsKey, btoa(JSON.stringify(settings)));
  }
  
  // f32 platform_get_time(void);
  function platform_get_time() {
    return (Date.now() - originTime) / 1e3;
  }
  
  const importObject = {
    env: {
      platform_write,
      platform_pointer_lock,
      platform_pointer_release,
      platform_send_packet,
      platform_settings_store,
      platform_get_time,
      // FIXME: implement acos(..) in WASM
      platform_acos: Math.acos
    },
  };
  
  function processKeyEvent(event) {
    if (instance == null) {
      return;
    }
    instance.exports.gloom_on_key_event(event.keyCode, event.key.charCodeAt(0), event.type === "keydown");
  }
  
  let mouseButtons = 0;
  
  function processMouseEvent(e) {
    if (instance == null) {
      return;
    }
    switch (e.type) {
      case "mousedown": {
        instance.exports.gloom_on_mouse_down(e.offsetX, e.offsetY, e.button);
        break;
      }
      case "mouseup": {
        instance.exports.gloom_on_mouse_up(e.offsetX, e.offsetY, e.button);
        break;
      }
      case "mousemove": {
        instance.exports.gloom_on_mouse_moved(e.offsetX, e.offsetY, e.movementX, e.movementY);
        break;
      }
      case "mouseleave": {
        mouseButtons = e.buttons;
        break;
      }
      case "mouseenter": {
        if (e.buttons !== mouseButtons) {
          canvas.dispatchEvent(new MouseEvent(e.buttons === 0 ? "mouseup" : "mousedown"));
        }
        break;
      }
    }
  }
  
  function processPointerLockChange() {
    instance.exports.gloom_set_pointer_locked(pointerIsLocked());
  }

  function toggleListeners(on) {
    const f = on ? "addEventListener" : "removeEventListener";
    document[f]("pointerlockchange", processPointerLockChange);
    window[f]("load", updateViewportSize);
    window[f]("resize", updateViewportSize);
    window[f]("keydown", processKeyEvent);
    window[f]("keyup", processKeyEvent);
    canvas[f]("mousedown", processMouseEvent);
    canvas[f]("mouseup", processMouseEvent);
    canvas[f]("mousemove", processMouseEvent);
    canvas[f]("mouseleave", processMouseEvent);
    canvas[f]("mouseenter", processMouseEvent);
  }

  function loadSettings(username) {
    settingsKey = `settings-${username}`;
    const b64Settings = localStorage.getItem(settingsKey);
    if (b64Settings != null) {
      const settings = JSON.parse(atob(b64Settings));
      instance.exports.gloom_settings_load(
        settings.drawdist, settings.fov,
        settings.mousesens, settings.camsmooth
      );
    } else {
      instance.exports.gloom_settings_defaults();
    }
  }

  function setupGame(username) {
    originTime = Date.now();
    
    canvas = $("#viewport");
    $assert(canvas != null && canvas instanceof HTMLCanvasElement, "no canvas with id 'viewport'");
    
    canvasContainer = canvas.parentElement;
    $assert(canvasContainer != null, "no canvas container");
    
    ctx = canvas.getContext("2d");
    
    canvas.width = canvasDefaultWidth;
    canvas.height = canvasDefaultHeight;
    canvas.style.transformOrigin = "top left";
    // TODO: Maybe use 'crisp-edges' instead of 'pixelated' on Firefox.
    canvas.style.imageRendering = "pixelated";
    canvas.style.background = "black";

    loadSettings(username);
    toggleListeners(true);
  }

  const obj = await WebAssembly.instantiateStreaming(fetch("/static/js/gloom.wasm"), importObject);
  const instance = obj.instance;
  const memory = instance.exports.memory;
    

  const launchGloom = (username, gameId, playerToken, onCloseHandler) => {
    setupGame(username);

    const proto = window.location.protocol.replace("http", "ws");
    const url = `${proto}//${window.location.host}/game`;
    ws = new WebSocket(url);

    function exitGame() {
      toggleListeners(false);
      fb = ctx = canvas = canvasContainer = null;
      ws.close();
    }

    let prevTimestamp;

    function tick(timestamp) {
      const delta = (timestamp - prevTimestamp) / 1000;
      if (instance.exports.gloom_tick(delta) !== 0) {
        ctx.putImageData(fb, 0, 0);
        prevTimestamp = timestamp;
        window.requestAnimationFrame(tick);
      } else {
        exitGame();
        if (onCloseHandler != null) {
          onCloseHandler();
        }
      }
    }

    // Init game.
    function startGame(online) {
      ws.removeEventListener("error", wsErrorHandler);
      ws.removeEventListener("open", wsOpenHandler);
      if (online) {
        // Send handshake.
        {
          data = new ArrayBuffer(12);
          view = new DataView(data);
          view.setUint32(0, playerToken, true);
          view.setUint32(4, gameId, true);
          view.setUint32(8, 0xBADC0FFE, true); // Handshake magic.
          ws.send(data);
        }
      }

      createFramebuffer();
      instance.exports.gloom_init(online, gameId, playerToken);

      window.requestAnimationFrame((timestamp) => {
        updateViewportSize();
        prevTimestamp = timestamp;
        window.requestAnimationFrame(tick);
      });
    }
  
    const wsErrorHandler = () => startGame(false);
    const wsOpenHandler = () => startGame(true);

    const pktBuffer = instance.exports.gloom_packet_buffer();
    const pktBufferSize = instance.exports.gloom_packet_buffer_size();
  
    ws.binaryType = "arraybuffer";
    ws.addEventListener("message", e => {
      if (e.data instanceof ArrayBuffer) {
        const pkt = new Uint8Array(e.data);
        new Uint8Array(memory.buffer, pktBuffer, pktBufferSize).set(pkt);
        instance.exports.gloom_on_recv_packet(pkt.byteLength);
      }
    });
    ws.addEventListener("close", instance.exports.gloom_on_ws_close);
    ws.addEventListener("error", wsErrorHandler);
    ws.addEventListener("open", wsOpenHandler);

    return ws;
  }

  return [launchGloom, instance.exports.gloom_exit];
}
