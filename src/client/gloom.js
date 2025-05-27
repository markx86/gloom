export async function loadGloom() {
  const textDecoder = new TextDecoder("utf-8");
  const canvasDefaultWidth = 640;
  const canvasDefaultHeight = 480;
  const containerPadding = 3;
  const url = `ws://${window.location.hostname}:8492`;

  let canvasContainer = null, canvas = null, ctx = null;
  let fbArray = null, fb = null;
  let originTime = 0;
  
  function updateViewportSize() {
    const aspectRatio = fb == null ? (canvasDefaultWidth / canvasDefaultHeight) : (fb.width / fb.height);
    const parentWidth = window.innerWidth;
    const parentHeight = window.innerHeight;
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
    // shamelessly stolen from
    // https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API/Tutorial/Optimizing_canvas#scaling_canvas_using_css_transforms
    const xScale = containerWidth / canvas.width;
    const yScale = containerHeight / canvas.height;
    const scaleToFit = Math.min(xScale, yScale);
    canvas.style.transform = `scale(${scaleToFit})`;
  }
  
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
    const pages = (sz + (1 << 16) - 1) >> 16;
    const fbAddress = fbArray.byteOffset;
    const fbSize = fbArray.byteLength;
    const fbWidth = fb.width;
    const fbHeight = fb.height;
    memory.grow(pages);
    // re-register framebuffer, because it gets detached every time we
    // call memory.grow(..)
    register_fb(fbAddress, fbWidth, fbHeight, fbSize);
    return pages << 16;
  }
  
  // void register_fb(void* addr, u32 width, u32 height, u32 size);
  function register_fb(addr, width, height, size) {
    fbArray = new Uint8ClampedArray(memory.buffer, addr, size);
    fb = new ImageData(fbArray, width, height);
    canvas.width = width;
    canvas.height = height;
    updateViewportSize();
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
  
  // i32 send_packet(void* pkt, u32 len);
  function send_packet(bufptr, len) {
    try {
      ws.send(memory.buffer.slice(bufptr, bufptr + len));
      return len;
    } catch {
      return -1;
    }
  }
  
  // f32 time(void);
  function time() {
    return (Date.now() - originTime) / 1e3;
  }
  
  const importObject = {
    env: {
      write,
      pointer_lock,
      pointer_release,
      request_mem,
      register_fb,
      send_packet,
      time
    },
  };
  
  function processKeyEvent(event) {
    if (instance == null) {
      return;
    }
    instance.exports.key_event(event.keyCode, event.key.charCodeAt(0), event.type === "keydown");
  }
  
  let mouseButtons = 0;
  
  function processMouseEvent(e) {
    if (instance == null) {
      return;
    }
    switch (e.type) {
      case "mousedown": {
        instance.exports.mouse_down(e.offsetX, e.offsetY, e.button);
        break;
      }
      case "mouseup": {
        instance.exports.mouse_up(e.offsetX, e.offsetY, e.button);
        break;
      }
      case "mousemove": {
        instance.exports.mouse_move(e.offsetX, e.offsetY, e.movementX, e.movementY);
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
    instance.exports.set_pointer_locked(pointerIsLocked());
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

  function setupGame() {
    originTime = Date.now();
    
    canvas = $("#viewport");
    $assert(canvas != null && canvas instanceof HTMLCanvasElement, "no canvas with id 'viewport'");
    
    canvasContainer = canvas.parentElement;
    $assert(canvasContainer != null, "no canvas container");
    
    ctx = canvas.getContext("2d");
    
    canvasContainer.style.padding = `${containerPadding}px`;
    
    canvas.width = canvasDefaultWidth;
    canvas.height = canvasDefaultHeight;
    canvas.style.transformOrigin = "top left";
    // TODO maybe use 'crisp-edges' instead of 'pixelated' on Firefox
    canvas.style.imageRendering = "pixelated";

    toggleListeners(true);
  }

  const obj = await WebAssembly.instantiateStreaming(fetch("/static/js/gloom.wasm"), importObject);
  const instance = obj.instance;
  const memory = instance.exports.memory;
    

  window.launchGloom = token => {
    setupGame();
    const ws = new WebSocket(url);

    function exitGame() {
      toggleListeners(false);
      fb = ctx = canvas = canvasContainer = null;
      ws.close();
    }

    let prevTimestamp;  

    function tick(timestamp) {
      const delta = (timestamp - prevTimestamp) / 1000;
      if (instance.exports.tick(delta) === true) {
        ctx.putImageData(fb, 0, 0);
        prevTimestamp = timestamp;
        window.requestAnimationFrame(tick);
      } else {
        exitGame();
      }
    }

    // init game
    function startGame(online, token) {
      ws.removeEventListener("error", wsErrorHandler);
      ws.removeEventListener("open", wsOpenHandler);
      if (online) {
        // send handshake
        {
          data = new ArrayBuffer(8);
          view = new DataView(data);
          view.setUint32(0, token, true);
          view.setUint32(4, 0xBADC0FFE, true); // Handshake magic
          ws.send(data);
        }
      }
      instance.exports.init(online, token);
      window.requestAnimationFrame((timestamp) => {
        updateViewportSize();
        prevTimestamp = timestamp;
        window.requestAnimationFrame(tick);
      });
    }
  
    const wsErrorHandler = () => startGame(false, token);
    const wsOpenHandler = () => startGame(true, token);
  
    ws.binaryType = "arraybuffer";
    ws.addEventListener("message", e => {
      if (e.data instanceof ArrayBuffer) {
        const pkt = new Uint8Array(e.data);
        new Uint8Array(memory.buffer, instance.exports.__pkt_buf, 0x1000).set(pkt);
        instance.exports.multiplayer_on_recv(pkt.byteLength);
      }
    });
    ws.addEventListener("error", wsErrorHandler);
    ws.addEventListener("open", wsOpenHandler);
    ws.addEventListener("error", console.log);
    ws.addEventListener("open", console.log);
  }
}
