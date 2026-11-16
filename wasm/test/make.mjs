import { createRequire } from "node:module";
import { writeFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const builder = resolve(process.argv[2] || join(here, "../../../goome-docs/builder"));
const require = createRequire(import.meta.url);
const GM = require(join(builder, "model.js"));
globalThis.GM = GM;
const T = require(join(builder, "templates.js"));
const W = require(join(builder, "wasm.js"));

const L = T.layouts();
const pick = (id) => GM.normProject(L.find((l) => l.id === id).make());
function elements() {
  const p = GM.normProject(GM.starterProject());
  p.root = [];
  p.elements = GM.TEMPLATES();
  p.elements.forEach((d, i) => {
    const n = GM.newNode("el", p);
    Object.assign(n.p, { el: d.id, x: 30 + (i % 3) * 260, y: 30 + Math.floor(i / 3) * 240 });
    p.root.push(n);
  });
  return GM.normProject(p);
}
function styled() {
  const p = GM.normProject(GM.starterProject());
  const w = p.root[0], kids = w.kids[0];
  p.themes = [GM.normTheme(GM.PRESETS.find((t) => t.id === "rose"))];
  kids[1].style = { c: { accent: 0xFFE5534B, text: 0xFFFFE600 }, n: { radius: 2, pad: 14 } };
  kids[2].style = { theme: p.themes[0].id };
  kids[3].p.enableIf = "show_grid && false";
  kids[4].p.showIf = "volume > 2";
  kids[5].style = { radii: [12, 0, 12, 0] };
  kids.push(Object.assign(GM.newNode("label", p), { p: Object.assign(GM.newNode("label", p).p, { label: "vol {volume} grid {show_grid} {name}" }) }));
  return GM.normProject(p);
}
const cases = [
  ["elements", elements(), 1, 6, [[2, 3, 62, 62, 0, 0, 0, 0], [2, 1, 62, 62, 1, 0, 0, 0], [3, 3, 62, 30, 0, 0, 0, 0], [4, 2, 62, 30, 1, 0, 0, 0],
    [4, 3, 590, 46, 0, 0, 0, 0], [4, 1, 590, 46, 1, 0, 0, 0], [5, 2, 590, 46, 1, 0, 0, 0]]],
  ["styled", styled(), 1, 3, []],
  ["starter", GM.normProject(GM.starterProject()), 1, 4, []],
  ["starter_clicks", GM.normProject(GM.starterProject()), 1, 9, [
    [1, 3, 60, 167, 0, 0, 0, 0], [1, 1, 60, 167, 1, 0, 0, 0], [2, 2, 60, 167, 1, 0, 0, 0],
    [3, 3, 95, 132, 0, 0, 0, 0], [3, 1, 95, 132, 1, 0, 0, 0], [4, 2, 95, 132, 1, 0, 0, 0],
    [5, 3, 200, 308, 0, 0, 0, 0], [5, 1, 200, 308, 1, 0, 0, 0], [5, 2, 200, 308, 1, 0, 0, 0],
    [6, 5, 0, 0, 0, 0, 101, 0, "e"], [6, 5, 0, 0, 0, 0, 120, 0, "x"], [7, 3, 330, 253, 0, 0, 0, 0],
  ]],
  ["hud", pick("hud"), 1, 3, []],
  ["notes", pick("notes"), 1, 3, []],
  ["inventory", pick("inventory"), 2, 3, []],
  ["dashboard", pick("dashboard"), 1, 3, []],
  ["mod_side", pick("mod_side"), 1, 3, []],
];
for (const [name, project, scale, frames, ev] of cases) {
  project.config = Object.assign({}, project.config, { blink_ms: 0 });
  const { prog, w, h } = W.lower(project, { noAnim: true });
  writeFileSync(join(here, name + ".json"), JSON.stringify(prog) + "\n");
  const lines = [w + " " + h + " " + scale + " " + frames].concat(ev.map((e) => e.join(" ")));
  writeFileSync(join(here, name + ".ev"), lines.join("\n") + "\n");
  console.log(name, w + "x" + h, "@" + scale);
}
