(function (root) {
  const KEYS = { Backspace: 8, Tab: 9, Enter: 13, Escape: 27, " ": 32, Delete: 127, ArrowLeft: 0x90, ArrowRight: 0x91, ArrowUp: 0x92, ArrowDown: 0x93,
    Home: 0x94, End: 0x95, PageUp: 0x96, PageDown: 0x97, Insert: 0x98 };
  const CURSORS = ["default", "text", "pointer", "move", "ew-resize", "ns-resize", "nwse-resize", "nesw-resize", "wait", "not-allowed", "none"];
  const T = { down: 1, up: 2, motion: 3, scroll: 4, key: 5, focus: 11, text: 13, preedit: 14 };
  const enc = new TextEncoder(), dec = new TextDecoder();

  const imports = (mem) => ({
    env: {
      gmw_console: (p, n) => console.log(dec.decode(new Uint8Array(mem().buffer, p, n)).replace(/\n$/, "")),
      sin: Math.sin, cos: Math.cos, tan: Math.tan, atan: Math.atan, atan2: Math.atan2, asin: Math.asin, acos: Math.acos,
      exp: Math.exp, log: Math.log, log2: Math.log2, log10: Math.log10, pow: Math.pow, sinh: Math.sinh, cosh: Math.cosh, tanh: Math.tanh,
    },
  });

  async function load(source) {
    let mem = null;
    const bytes = source instanceof ArrayBuffer || ArrayBuffer.isView(source) ? source : await (await fetch(source)).arrayBuffer();
    const { instance } = await WebAssembly.instantiate(bytes, imports(() => mem));
    mem = instance.exports.memory;
    return instance.exports;
  }

  function attach(ex, canvas, env) {
    const win = env.window, doc = env.document, nav = env.navigator;
    const raf = env.raf || ((f) => win.requestAnimationFrame(f));
    const later = env.setTimeout || ((f, ms) => setTimeout(f, ms));
    const now = env.now || (() => win.performance.now());
    const Img = env.ImageData || win.ImageData;
    const ctx = canvas.getContext("2d");
    const input = doc.createElement("textarea");
    Object.assign(input.style, { position: "fixed", left: "0", top: "0", width: "1px", height: "1px", opacity: "0", padding: "0", border: "0", resize: "none" });
    input.setAttribute("autocapitalize", "off");
    input.setAttribute("autocomplete", "off");
    input.setAttribute("spellcheck", "false");
    doc.body.appendChild(input);

    const cstr = (s) => {
      const b = enc.encode(String(s) + "\0");
      const p = ex.gmw_alloc(b.length);
      new Uint8Array(ex.memory.buffer, p, b.length).set(b);
      return p;
    };
    const rstr = (p) => {
      if (!p) return "";
      const m = new Uint8Array(ex.memory.buffer);
      let e = p;
      while (m[e]) e++;
      return dec.decode(m.subarray(p, e));
    };
    const push = (type, x, y, button, dy, key, mods, text) => {
      const tp = text ? cstr(text) : 0;
      ex.gmw_push(type, x, y, button, dy, key, mods, tp);
      if (tp) ex.gmw_free(tp);
      wake();
    };
    const modsOf = (e) => (e.shiftKey ? 0x100 : 0) | (e.ctrlKey ? 0x200 : 0) | (e.altKey ? 0x400 : 0) | (e.metaKey ? 0x800 : 0);
    const spot = (e) => {
      const r = canvas.getBoundingClientRect();
      return [e.clientX - r.left, e.clientY - r.top];
    };
    const button = (e) => (e.button === 2 ? 3 : e.button === 1 ? 2 : 1);

    const state = { alive: true, timer: 0, frame: 0, lastInput: 0, lastClip: "", resize: false, composing: false, wheel: 0 };

    function sizes() {
      return [Math.max(1, win.innerWidth | 0), Math.max(1, win.innerHeight | 0), Math.min(4, Math.max(1, win.devicePixelRatio || 1))];
    }

    function fit() {
      const [w, h] = sizes();
      canvas.style.width = w + "px";
      canvas.style.height = h + "px";
    }

    function wake() {
      state.lastInput = now();
      if (!state.alive) return;
      if (state.timer) (win.clearTimeout || clearTimeout)(state.timer), (state.timer = 0);
      if (!state.frame) state.frame = raf(tick);
    }

    function draw() {
      const w = ex.gmw_width(), h = ex.gmw_height(), p = ex.gmw_pixels();
      if (!p || !w || !h) return;
      if (canvas.width !== w || canvas.height !== h) (canvas.width = w), (canvas.height = h);
      ctx.putImageData(new Img(new Uint8ClampedArray(ex.memory.buffer, p, w * h * 4).slice(), w, h), 0, 0);
    }

    function syncClip() {
      const clip = rstr(ex.gmw_clip_get());
      if (clip === state.lastClip) return;
      state.lastClip = clip;
      if (nav && nav.clipboard && clip) nav.clipboard.writeText(clip).catch(() => {});
    }

    function tick(ms) {
      state.frame = 0;
      if (!state.alive) return;
      if (state.resize) {
        state.resize = false;
        const [w, h, s] = sizes();
        ex.gmw_resize(w, h, s);
        fit();
      }
      const r = ex.gmw_frame(ms);
      if (r & 1) draw();
      canvas.style.cursor = CURSORS[ex.gmw_cursor()] || "default";
      syncClip();
      if (!(r & 2)) {
        state.alive = false;
        if (env.onquit) env.onquit();
        return;
      }
      if (r & 1 || now() - state.lastInput < 1000) state.frame = raf(tick);
      else state.timer = later(() => ((state.timer = 0), (state.frame = raf(tick))), 100);
    }

    canvas.tabIndex = 0;
    canvas.style.outline = "none";
    canvas.style.touchAction = "none";
    canvas.style.display = "block";
    const focusInput = () => input.focus({ preventScroll: true });
    canvas.addEventListener("pointerdown", (e) => {
      focusInput();
      try {
        canvas.setPointerCapture(e.pointerId);
      } catch (er) {}
      const [x, y] = spot(e);
      push(T.motion, x, y, 0, 0, 0, 0, 0);
      push(T.down, x, y, button(e), 0, 0, modsOf(e), 0);
      e.preventDefault();
    });
    canvas.addEventListener("pointermove", (e) => {
      const [x, y] = spot(e);
      push(T.motion, x, y, 0, 0, 0, 0, 0);
    });
    canvas.addEventListener("pointerup", (e) => {
      const [x, y] = spot(e);
      push(T.up, x, y, button(e), 0, 0, modsOf(e), 0);
    });
    canvas.addEventListener("pointerleave", () => push(T.motion, -100000, -100000, 0, 0, 0, 0, 0));
    canvas.addEventListener("contextmenu", (e) => e.preventDefault());
    canvas.addEventListener("wheel", (e) => {
      state.wheel += e.deltaMode === 1 ? e.deltaY : e.deltaMode === 2 ? e.deltaY * 3 : e.deltaY / 40;
      const k = state.wheel > 0 ? Math.floor(state.wheel) : Math.ceil(state.wheel);
      if (k) {
        const [x, y] = spot(e);
        state.wheel -= k;
        push(T.scroll, x, y, 0, k, 0, modsOf(e), 0);
      }
      e.preventDefault();
    }, { passive: false });

    input.addEventListener("keydown", (e) => {
      if (state.composing || e.isComposing || e.key === "Process") return;
      const mods = modsOf(e);
      const paste = (e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "v";
      if (paste || e.key === "F5" || e.key === "F12") return;
      let key = KEYS[e.key] || 0;
      const f = /^F(\d{1,2})$/.exec(e.key);
      if (f && +f[1] >= 1 && +f[1] <= 12) key = 0x80 + +f[1];
      const cp = Array.from(e.key);
      let text = "";
      if (!key && cp.length === 1) {
        const c = cp[0].toLowerCase();
        key = c.charCodeAt(0) < 128 ? c.charCodeAt(0) : 0;
        if (!e.ctrlKey && !e.metaKey) text = cp[0];
      }
      if (key === 32 && !e.ctrlKey && !e.metaKey) text = " ";
      e.preventDefault();
      if (!key && !text) return;
      push(T.key, 0, 0, 0, 0, key, mods, text);
    });
    input.addEventListener("paste", (e) => {
      const s = (e.clipboardData && e.clipboardData.getData("text/plain")) || "";
      const p = cstr(s);
      ex.gmw_clip_set(p);
      ex.gmw_free(p);
      state.lastClip = s;
      const mac = nav && /Mac/.test(nav.platform || "");
      push(T.key, 0, 0, 0, 0, 118, mac ? 0x800 : 0x200, 0);
      e.preventDefault();
    });
    input.addEventListener("compositionstart", () => (state.composing = true));
    input.addEventListener("compositionupdate", (e) => {
      const t = e.data || "";
      push(T.preedit, 0, 0, enc.encode(t).length, 0, 0, 0, t);
    });
    input.addEventListener("compositionend", (e) => {
      state.composing = false;
      push(T.preedit, 0, 0, 0, 0, 0, 0, "");
      const t = e.data || "";
      for (const piece of t.match(/[\s\S]{1,16}/g) || []) push(T.text, 0, 0, 0, 0, 0, 0, piece);
      input.value = "";
    });
    input.addEventListener("input", () => {
      if (!state.composing) input.value = "";
    });
    input.addEventListener("focus", () => push(T.focus, 0, 0, 1, 0, 0, 0, 0));
    input.addEventListener("blur", () => push(T.focus, 0, 0, 0, 0, 0, 0, 0));
    win.addEventListener("resize", () => ((state.resize = true), wake()));

    const [w, h, s] = sizes();
    const rc = ex.gmw_init(w, h, s, env.chrome ? 1 : 0);
    if (rc) throw new Error(rc === -2 ? "the app never opened a goome window" : "goome_web_init failed");
    fit();
    const title = rstr(ex.gmw_title());
    if (title && doc) doc.title = title;
    focusInput();
    wake();
    return { state, ex, tick, wake, input, push, title };
  }

  async function boot(canvas, wasm, opts) {
    const ex = await load(wasm);
    const env = Object.assign({ window, document, navigator }, opts || {});
    return attach(ex, canvas, env);
  }

  const GoomeWeb = { boot, attach, load, T };
  if (typeof module !== "undefined" && module.exports) module.exports = GoomeWeb;
  else root.GoomeWeb = GoomeWeb;
})(typeof window !== "undefined" ? window : globalThis);
