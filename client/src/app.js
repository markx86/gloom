(async () => {

  // remove margin from body to make canvas fullscreen
  document.body.style.margin = "0";

  const textDecoder = new TextDecoder("utf-8");

  const canvas = document.getElementById("viewport");
  const ctx = canvas.getContext("2d");

  canvas.style.transformOrigin = "top left";
  canvas.style.position = "absolute";
  canvas.style.top = "50%";
  canvas.style.left = "50%";
  // TODO maybe use 'crisp-edges' instead of 'pixelated' on Firefox
  canvas.style.imageRendering = "pixelated";

  let fbView = null;
  let fb = null;

  function getPlayerToken() {
    // TODO: fetch this from params or cookie or whatever
    return Math.floor(Math.random() * 0xffffffff);
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
    const fbAddress = fbView.byteOffset;
    const fbSize = fbView.byteLength;
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

  // i32 send_packet(void* pkt, u32 len);
  function send_packet(bufptr, len) {
    try {
      ws.send(memory.buffer.slice(bufptr, bufptr + len));
      return len;
    } catch {
      return -1;
    }
  }

  const originTime = Date.now();

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

  const wasmBase64 = "@@WASMB64@@";
  const wasmBytes = Uint8Array.from(atob(wasmBase64), c => c.charCodeAt(0)).buffer;
  const obj = await WebAssembly.instantiate(wasmBytes, importObject);
  const instance = obj.instance;

  const memory = instance.exports.memory;

  function updateViewportSize() {
    // shamelessly stolen from
    // https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API/Tutorial/Optimizing_canvas#scaling_canvas_using_css_transforms
    const xScale = window.innerWidth / canvas.width;
    const yScale = window.innerHeight / canvas.height;
    const scaleToFit = Math.min(xScale, yScale);
    canvas.style.transform = `scale(${scaleToFit}) translate(-50%, -50%)`;
  }

  function processKeyEvent(event) {
    instance.exports.key_event(event.keyCode, event.key.charCodeAt(0), event.type === "keydown");
  }

  document.addEventListener("pointerlockchange", () => instance.exports.set_pointer_locked(pointerIsLocked()));
  window.addEventListener("load", updateViewportSize);
  window.addEventListener("resize", updateViewportSize);
  window.addEventListener("keydown", processKeyEvent);
  window.addEventListener("keyup", processKeyEvent);
  canvas.addEventListener("mousedown", e => instance.exports.mouse_down(e.offsetX, e.offsetY, e.button));
  canvas.addEventListener("mouseup", e => instance.exports.mouse_up(e.offsetX, e.offsetY, e.button));
  canvas.addEventListener("mousemove", e => instance.exports.mouse_moved(e.offsetX, e.offsetY, e.movementX, e.movementY));

  let prevTimestamp;

  function tick(timestamp) {
    const delta = (timestamp - prevTimestamp) / 1000;
    instance.exports.tick(delta);
    ctx.putImageData(fb, 0, 0);
    prevTimestamp = timestamp;
    window.requestAnimationFrame(tick);
  }

  function sendHandshake(token) {
    data = new ArrayBuffer(8);
    view = new DataView(data);
    view.setUint32(0, token, true);
    view.setUint32(4, 0xBADC0FFE, true); // Handshake magic
    ws.send(data);
  }

  // init game
  function launchGame(online) {
    ws.removeEventListener("error", launchGameOffline);
    ws.removeEventListener("open", launchGameOnline);
    const token = getPlayerToken()
    if (online) {
      sendHandshake(token);
    }
    instance.exports.init(online, token);
    window.requestAnimationFrame((timestamp) => {
      updateViewportSize();
      prevTimestamp = timestamp;
      window.requestAnimationFrame(tick);
    });
  }

  const launchGameOffline = () => launchGame(false);
  const launchGameOnline = () => launchGame(true);

  const url = `ws://${window.location.hostname}:8492`;

  const ws = new WebSocket(url);
  ws.binaryType = "arraybuffer";
  ws.addEventListener("message", e => {
    if (e.data instanceof ArrayBuffer) {
      const pkt = new Uint8Array(e.data);
      new Uint8Array(memory.buffer, instance.exports.__pkt_buf, 0x1000).set(pkt);
      instance.exports.multiplayer_on_recv(pkt.byteLength);
    }
  });
  ws.addEventListener("error", launchGameOffline);
  ws.addEventListener("open", launchGameOnline);
})();
