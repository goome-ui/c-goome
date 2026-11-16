import { readFileSync, mkdirSync, rmSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { createRequire } from "node:module";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const out = join(here, "..", "out", "web");
const GoomeWeb = createRequire(import.meta.url)(join(root, "wasm", "web", "goome.js"));
let fails = 0;
const realLog = console.log;
const check = (name, ok, extra) => {
  if (!ok) fails++;
  realLog((ok ? "ok   " : "FAIL ") + name + (extra ? "  " + extra : ""));
};

rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });
const build = (name) => {
  execFileSync(join(root, "wasm", "web-build.sh"), [join(root, "examples", name + ".c"), join(out, name)], { stdio: "pipe" });
  return readFileSync(join(out, name, "goome.wasm"));
};

const refuse = (file) => {
  try {
    execFileSync(join(root, "wasm", "web-build.sh"), [join(root, "examples", file), join(out, "no")], { stdio: "pipe" });
    return "";
  } catch (e) {
    return String(e.stderr);
  }
};
check("overlay is refused", /isnt a goome app/.test(refuse("hello.c")));
check("blocking loop is refused", /goome_web_frame/.test(refuse("app_ui.c")));

const fake = () => {
  const listeners = {};
  const el = (extra) => ({
    style: {},
    listeners: {},
    attrs: {},
    setAttribute(k, v) { this.attrs[k] = v; },
    addEventListener(t, f) { (this.listeners[t] ||= []).push(f); },
    focus() { this.focused = true; (this.listeners.focus || []).forEach((f) => f({})); },
    fire(t, e) { (this.listeners[t] || []).forEach((f) => f(Object.assign({ preventDefault() {} }, e))); },
    ...extra,
  });
  const canvas = el({
    width: 0, height: 0,
    getContext: () => ({ putImageData(img) { canvas.last = img; canvas.puts++; } }),
    getBoundingClientRect: () => ({ left: 0, top: 0 }),
    setPointerCapture() {},
    puts: 0,
  });
  const input = el({ value: "" });
  const doc = { title: "", body: { appendChild() {} }, createElement: () => input };
  const queue = [];
  const win = { innerWidth: 640, innerHeight: 420, devicePixelRatio: 2, addEventListener: (t, f) => (listeners[t] ||= []).push(f), listeners };
  const clip = [];
  const env = {
    window: win, document: doc, navigator: { platform: "Linux", clipboard: { writeText: (s) => (clip.push(s), Promise.resolve()) } },
    raf: (f) => (queue.push(f), queue.length),
    setTimeout: (f) => (queue.push(f), queue.length),
    now: () => 0,
    ImageData: class { constructor(data, w, h) { this.data = data, this.width = w, this.height = h; } },
    onquit: () => (env.quit = true),
  };
  return { canvas, input, doc, win, env, queue, clip };
};

const hash = (ex) => {
  const w = ex.gmw_width(), h = ex.gmw_height(), p = ex.gmw_pixels();
  const m = new Uint8Array(ex.memory.buffer, p, w * h * 4);
  let x = 2166136261;
  for (let i = 0; i < m.length; i++) x = Math.imul(x ^ m[i], 16777619);
  return (x >>> 0).toString(16).padStart(8, "0");
};
const px = (ex, x, y) => {
  const w = ex.gmw_width(), p = ex.gmw_pixels();
  const m = new Uint8Array(ex.memory.buffer, p, w * ex.gmw_height() * 4);
  const i = (y * w + x) * 4;
  return Array.from(m.subarray(i, i + 4)).join(",");
};
let ms = 0;
const step = (ex, n = 2) => {
  let r = 0;
  for (let i = 0; i < n; i++) r = ex.gmw_frame((ms += 16));
  return r;
};
const click = (ex, x, y) => {
  ex.gmw_push(3, x, y, 0, 0, 0, 0, 0);
  step(ex, 1);
  ex.gmw_push(1, x, y, 1, 0, 0, 0, 0);
  ex.gmw_push(2, x, y, 1, 0, 0, 0, 0);
  return step(ex, 3);
};

{
  const wasm = build("web_counter");
  const html = readFileSync(join(out, "web_counter", "index.html"), "utf8");
  const a = await GoomeWeb.load(wasm);
  check("counter inits without a title bar", a.gmw_init(480, 360, 1, 0) === 0);
  const b = await GoomeWeb.load(wasm);
  check("counter inits with the bar", b.gmw_init(480, 360, 1, 1) === 0);
  step(a), step(b);
  const text = (ex) => {
    const m = new Uint8Array(ex.memory.buffer);
    let p = ex.gmw_title(), e = p;
    while (m[e]) e++;
    return new TextDecoder().decode(m.subarray(p, e));
  };
  check("title comes from goome_open", text(a) === "Counter", text(a));
  check("title in the page", /<title>Counter<\/title>/.test(html));
  const top = [px(a, 440, 0), px(a, 440, 3), px(a, 440, 8)];
  const bar = [px(b, 440, 0), px(b, 440, 3), px(b, 440, 8)];
  check("no bar: top rows are plain background", top[0] === top[1] && top[1] === top[2], top[0]);
  check("with bar: top rows are the bar color", bar[0] === bar[1] && bar[0] !== top[0], bar[0] + " vs " + top[0]);
  check("frames differ with and without the bar", hash(a) !== hash(b), hash(a) + " " + hash(b));
  check("click where the close button would be keeps the web app open", (click(a, 470, 12) & 2) === 2);
  check("same click quits when the bar is on", (click(b, 470, 12) & 2) === 0);
  const same = hash(a);
  step(a);
  check("frame is stable", hash(a) === same);
}

{
  const wasm = build("web_draw");
  const d = fake();
  const app = GoomeWeb.attach(await GoomeWeb.load(wasm), d.canvas, d.env);
  const ex = app.ex;
  check("draw app title reaches document.title", d.doc.title === "Scribble", d.doc.title);
  check("canvas css size is logical", d.canvas.style.width === "640px" && d.canvas.style.height === "420px");
  const pump = () => {
    for (let i = 0; i < 4 && d.queue.length; i++) d.queue.shift()(ms += 16);
  };
  pump();
  check("first frame drawn", d.canvas.puts >= 1);
  check("canvas is sized in device pixels", d.canvas.width === 1280 && d.canvas.height === 840, d.canvas.width + "x" + d.canvas.height);
  const blank = hash(ex);
  d.canvas.fire("pointerdown", { clientX: 100, clientY: 100, button: 0, pointerId: 1 });
  for (let i = 0; i < 10; i++) d.canvas.fire("pointermove", { clientX: 100 + i * 8, clientY: 100 + i * 3 });
  d.canvas.fire("pointerup", { clientX: 180, clientY: 130, button: 0 });
  pump(), pump();
  const drawn = hash(ex);
  check("mouse draws", drawn !== blank);
  check("cursor follows the app", d.canvas.style.cursor === "default");
  d.canvas.fire("pointerdown", { clientX: 10, clientY: 200, button: 0, pointerId: 1 });
  pump();
  check("hand cursor while pressed", d.canvas.style.cursor === "pointer", d.canvas.style.cursor);
  d.canvas.fire("pointerup", { clientX: 10, clientY: 200, button: 0 });
  pump();
  d.canvas.fire("wheel", { clientX: 50, clientY: 50, deltaY: 80, deltaMode: 0 });
  d.canvas.fire("pointerdown", { clientX: 300, clientY: 300, button: 0, pointerId: 1 });
  d.canvas.fire("pointerup", { clientX: 300, clientY: 300, button: 0 });
  pump(), pump();
  check("wheel reaches the app", hash(ex) !== drawn);
  d.input.fire("keydown", { key: "c", ctrlKey: true });
  pump(), pump();
  check("copy lands on the real clipboard", d.clip.length === 1 && /^\d+ dots$/.test(d.clip[0]), d.clip[0]);
  const beforePaste = hash(ex);
  d.input.fire("paste", { clipboardData: { getData: () => "from outside" } });
  pump(), pump();
  check("paste reaches the app", hash(ex) !== beforePaste);
  const t0 = hash(ex);
  d.input.fire("compositionstart", {});
  d.input.fire("compositionupdate", { data: "ni" });
  pump(), pump();
  const comp = hash(ex);
  check("ime preedit shows", comp !== t0);
  d.input.fire("compositionend", { data: "你好" });
  pump(), pump();
  check("ime text arrives", hash(ex) !== comp);
  d.win.innerWidth = 900;
  d.win.innerHeight = 500;
  d.win.listeners.resize.forEach((f) => f());
  pump(), pump();
  check("window resize reaches goome", d.canvas.width === 1800 && d.canvas.height === 1000, d.canvas.width + "x" + d.canvas.height);
  check("canvas css follows the window", d.canvas.style.width === "900px");
  check("focus is wired", d.input.focused === true);
  check("loop keeps scheduling", d.queue.length > 0);
}


const area = (ex, x0, y0, x1, y1) => {
  const w = ex.gmw_width(), p = ex.gmw_pixels();
  const m = new Uint8Array(ex.memory.buffer, p, w * ex.gmw_height() * 4);
  let x = 2166136261;
  for (let y = y0; y < y1; y++)
    for (let c = x0 * 4; c < x1 * 4; c++) x = Math.imul(x ^ m[y * w * 4 + c], 16777619);
  return (x >>> 0).toString(16);
};
const press = (ex, k, mods = 0) => {
  ex.gmw_push(5, 0, 0, 0, 0, k, mods, 0);
  return step(ex, 2);
};
const typed = (ex, s) => {
  for (const ch of s) {
    const b = new TextEncoder().encode(ch + "\0");
    const p = ex.gmw_alloc(b.length);
    new Uint8Array(ex.memory.buffer, p, b.length).set(b);
    ex.gmw_push(5, 0, 0, 0, 0, ch.charCodeAt(0), 0, p);
    ex.gmw_free(p);
    step(ex, 1);
  }
};
const lines = [];
const spy = (fn) => {
  lines.length = 0;
  const err = [];
  const e = console.error, w = console.warn;
  console.log = (...a) => lines.push(a.join(" "));
  console.error = (...a) => err.push(a.join(" "));
  console.warn = (...a) => err.push(a.join(" "));
  return fn().finally(() => {
    console.log = realLog;
    console.error = e;
    console.warn = w;
    return err;
  }).then(() => err);
};

const ballX = (ex, w, h) => {
  const p = ex.gmw_pixels();
  const m = new Uint8Array(ex.memory.buffer, p, w * h * 4);
  let sx = 0, n = 0;
  for (let y = 120; y < h; y += 2)
    for (let x = 0; x < w; x += 2) {
      const i = (y * w + x) * 4;
      if (m[i] === 255 && m[i + 1] === 255 && m[i + 2] === 255) sx += x, n++;
    }
  return n > 1 ? sx / n : -1;
};

{
  const wasm = build("web_game");
  check("game wasm stays small", wasm.length < 614400, wasm.length + " bytes");
  const html = readFileSync(join(out, "web_game", "index.html"), "utf8");
  check("game title in the page", /<title>Brickout<\/title>/.test(html));
  const W = 360, H = 640;
  const errors = await spy(async () => {
    const ex = await GoomeWeb.load(wasm);
    check("game inits", ex.gmw_init(W, H, 1, 0) === 0);
    step(ex, 30);
    const top = [px(ex, 300, 0), px(ex, 300, 3), px(ex, 300, 8)];
    check("game has no title bar", top[0] === top[1] && top[1] === top[2], top[0]);
    const menu = hash(ex);
    press(ex, 27);
    step(ex, 20);
    check("esc closes the start menu", hash(ex) !== menu);
    const hud = () => area(ex, 10, 10, 120, 34);
    const hud0 = hud();
    const idle = hash(ex);
    step(ex, 10);
    check("ball waits on the paddle", hash(ex) === idle);
    ex.gmw_push(3, 120, 520, 0, 0, 0, 0, 0);
    step(ex, 30);
    check("mouse moves the paddle", hash(ex) !== idle);
    const moved = hash(ex);
    press(ex, 0x90);
    step(ex, 30);
    check("arrow keys move the paddle", hash(ex) !== moved);
    press(ex, 32);
    step(ex, 5);
    const flying = hash(ex);
    step(ex, 5);
    check("space launches the ball", hash(ex) !== flying);
    press(ex, 112);
    step(ex, 30);
    const paused = hash(ex);
    step(ex, 30);
    check("p pauses", hash(ex) === paused);
    press(ex, 27);
    step(ex, 30);
    check("esc resumes", hash(ex) !== paused);
    let played = 0;
    for (let f = 0; f < 14000 && !lines.some((l) => /^level 2/.test(l)); f++) {
      step(ex, 1);
      const bx = ballX(ex, W, H);
      if (bx >= 0) ex.gmw_push(3, bx, H - 80, 0, 0, 0, 0, 0);
      played = f;
    }
    check("score goes up while playing", hud() !== hud0);
    check("clearing the bricks starts level 2", lines.some((l) => /^level 2/.test(l)), played + " frames");
    press(ex, 114);
    step(ex, 20);
    check("r starts over with a zero score", hud() === hud0);
  });
  check("game logs no errors", errors.length === 0, errors.join("|"));
}

{
  const wasm = build("web_game");
  const W = 360, H = 640;
  const errors = await spy(async () => {
    const ex = await GoomeWeb.load(wasm);
    ex.gmw_init(W, H, 1, 0);
    step(ex, 30);
    press(ex, 27);
    step(ex, 20);
    const over = (n) => lines.filter((l) => /^game over/.test(l)).length >= n;
    for (let round = 1; round <= 2; round++) {
      for (let f = 0; f < 4000 && !over(round); f++) {
        if (f % 150 === 0) press(ex, 32);
        else step(ex, 1);
      }
      check("losing every ball ends the game " + round, over(round), lines[lines.length - 1]);
      step(ex, 40);
      const modal = hash(ex);
      press(ex, 27);
      step(ex, 40);
      check("game over menu closes and a new game starts " + round, hash(ex) !== modal);
    }
    const score = lines.filter((l) => /^game over/.test(l)).map((l) => +l.split(" ").pop());
    check("both games report a score", score.length === 2 && score.every((n) => n >= 0), score.join(","));
  });
  check("game over run logs no errors", errors.length === 0, errors.join("|"));
}

{
  const wasm = build("web_app");
  check("app wasm stays small", wasm.length < 614400, wasm.length + " bytes");
  const html = readFileSync(join(out, "web_app", "index.html"), "utf8");
  check("app title in the page", /<title>Tidy<\/title>/.test(html));
  const W = 390, H = 700;
  const errors = await spy(async () => {
    const ex = await GoomeWeb.load(wasm);
    check("app inits", ex.gmw_init(W, H, 1, 0) === 0);
    step(ex, 30);
    const top = [px(ex, 300, 0), px(ex, 300, 3), px(ex, 300, 8)];
    check("app has no title bar", top[0] === top[1] && top[1] === top[2], top[0]);
    const status = () => area(ex, 0, 676, 200, 700);
    const s0 = status();
    const first = hash(ex);
    click(ex, 100, 123);
    typed(ex, "walk dog");
    press(ex, 13);
    step(ex, 90);
    check("typing a task and enter adds it", status() !== s0);
    check("the list changed", hash(ex) !== first);
    click(ex, 355, 574);
    step(ex, 90);
    check("the x button deletes it again", status() === s0);
    const darkBg = px(ex, 5, 300);
    click(ex, 313, 62);
    step(ex, 20);
    click(ex, 357, 198);
    step(ex, 60);
    const lightBg = px(ex, 5, 300);
    const brightness = (c) => c.split(",").slice(0, 3).reduce((a, b) => a + +b, 0);
    check("dark mode toggle switches the theme", lightBg !== darkBg, darkBg + " vs " + lightBg);
    check("it went light", brightness(lightBg) > brightness(darkBg) + 300);
    click(ex, 357, 198);
    step(ex, 60);
    check("and back to dark", px(ex, 5, 300) === darkBg);
    const tab = hash(ex);
    const lower = area(ex, 0, 540, W, 660);
    click(ex, 100, 447);
    step(ex, 60);
    const modal = hash(ex);
    check("clear opens a modal", modal !== tab);
    press(ex, 27);
    step(ex, 60);
    check("esc closes it", area(ex, 0, 540, W, 660) === lower);
    click(ex, 195, 62);
    step(ex, 30);
    const stats = hash(ex);
    click(ex, 313, 62);
    step(ex, 30);
    check("tabs switch pages", hash(ex) === tab && stats !== tab);
    click(ex, 100, 62);
    step(ex, 30);
    ex.gmw_push(3, 357, 334, 0, 0, 0, 0, 0);
    step(ex, 4);
    const quiet = hash(ex);
    step(ex, 60, 16);
    check("hovering shows a tooltip", hash(ex) !== quiet);
  });
  check("app logs no errors", errors.length === 0, errors.join("|"));
}

realLog(fails ? fails + " failed" : "web build ok");
process.exit(fails ? 1 : 0);
