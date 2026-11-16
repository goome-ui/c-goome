import { readFileSync, readdirSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const out = join(here, "..", "out");
const bytes = readFileSync(join(out, "goome.wasm"));
const enc = new TextEncoder(), dec = new TextDecoder();
let fails = 0;

for (const f of readdirSync(here).filter((x) => x.endsWith(".json")).sort()) {
  const name = f.slice(0, -5);
  const json = readFileSync(join(here, f), "utf8");
  const lines = readFileSync(join(here, name + ".ev"), "utf8").split("\n").filter(Boolean);
  const [w, h, scale, frames] = lines[0].split(" ").map(Number);
  const ev = lines.slice(1).map((l) => {
    const p = l.split(" ");
    return { f: +p[0], a: p.slice(1, 8).map(Number), text: p.slice(8).join(" ") };
  });
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const x = instance.exports;
  const str = (s) => {
    const b = enc.encode(s + "\0");
    const p = x.gw_alloc(b.length);
    new Uint8Array(x.memory.buffer, p, b.length).set(b);
    return p;
  };
  const read = (p) => {
    const m = new Uint8Array(x.memory.buffer);
    let e = p;
    while (m[e]) e++;
    return dec.decode(m.subarray(p, e));
  };
  if (x.gw_init(w, h, scale)) throw new Error(name + ": gw_init failed");
  const jp = str(json);
  if (x.gw_load(jp, enc.encode(json).length)) throw new Error(name + ": gw_load failed: " + read(x.gw_error()));
  x.gw_free(jp);
  for (let k = 0; k < frames; k++) {
    for (const e of ev.filter((e) => e.f === k)) {
      const tp = e.text ? str(e.text) : 0;
      x.gw_push(...e.a, tp);
      if (tp) x.gw_free(tp);
    }
    x.gw_frame(k * 16);
  }
  const got = (x.gw_hash() >>> 0).toString(16).padStart(8, "0") + " " + x.gw_width() + "x" + x.gw_height();
  const log = read(x.gw_log_take());
  const native = execFileSync(join(out, "gw-native"), [join(here, f), join(here, name + ".ev")], { encoding: "utf8" });
  const want = native.split("\n")[0];
  const ok = got === want && log === native.split("\n").slice(1).join("\n");
  if (!ok) fails++;
  console.log((ok ? "ok   " : "FAIL ") + name.padEnd(16) + " wasm " + got + "  native " + want + (log ? "  log: " + log.trim().replace(/\n/g, " | ") : ""));
}
console.log(fails ? fails + " failed" : "all frames match native");
process.exit(fails ? 1 : 0);
