<h1 align="center">🛠 Contributing to c-goome 🛠</h1>

<p align="center">
Thanks for wanting to help! Small PRs that fix one thing are the easiest for me to check and merge, so keep them small and they'll get in way faster.<br>
A desktop or compositor it doesnt work on yet and bugs you hit on your own setup are the most useful things to fix.
</p>

<details>
<summary align="center"><b>📋 The short version 📋</b></summary>

<p align="center">
• One thing per PR. One fix or one feature, if it does two things it gets closed<br>
• Open your PR against the <b>dev</b> branch, not main<br>
• Change <code>src/</code> and <code>include/</code>, never <code>goome.h</code> in the root<br>
• Dont touch the protected files, a bot closes the PR if you do<br>
• Test it and say how you tested it, and on which OS<br>
• CI has to pass<br>
• AI is fine, but say you used it, and you need to know what every line does
</p>
</details>

<details>
<summary align="center"><b>💬 Before you start 💬</b></summary>

<p align="center">
Small fixes can go straight to a PR.<br>
For a new feature or anything big, <a href="https://github.com/goome-ui/c-goome/issues">open an issue</a> first so we can talk about it. Nobody wants to spend a week on something that doesnt fit.<br> <br>
New public functions in <code>include/goome.h</code> always need an issue first. Once people use them they cant really change anymore.
</p>
</details>

<details>
<summary align="center"><b>🌿 Branches 🌿</b></summary>

<p align="center">
Everything lands on <b>dev</b> first. Once its been around for a bit it moves to <b>beta</b>, then <b>main</b>. I do the moving, you only ever PR into dev.
</p>
</details>

<details>
<summary align="center"><b>🚫 What gets closed automatically 🚫</b></summary>

<p align="center">
A bot checks every PR and closes it with a comment if it hits any of these. Dont take it personally, fix it and open a new one.<br> <br>
• <b>Not against dev</b> - main and beta only get stuff that already sat on dev for a while<br>
• <b>Anything in <code>.github/</code></b> - thats CI and these rules, a PR could turn them off for itself<br>
• <b><code>goome.h</code> in the root</b> - it gets made from <code>src/</code> and <code>include/</code> by <code>tools/single.py</code>, so a hand edit gets wiped next build anyway<br>
• <b><code>get.sh</code> or <code>get.ps1</code></b> - they run on peoples computers when they install goome<br>
• <b><code>LICENSE</code></b> - thats not something a PR changes<br>
• <b><code>docs/logo.png</code>, <code>docs/logo.svg</code>, <code>docs/demo.gif</code>, <code>docs/screenshot.png</code></b> - those are how goome looks to everyone, I make them<br>
• <b>More than 3000 changed files</b> - thats way too big to check<br> <br>
Renaming or moving any of those files counts too. Think one of them is wrong? <a href="https://github.com/goome-ui/c-goome/issues">Open an issue</a> and Ill fix it.
</p>
</details>

<details>
<summary align="center"><b>📄 goome.h 📄</b></summary>

<p align="center">
<code>goome.h</code> in the root is the one file version people drop into their project. Its made from <code>include/</code> and <code>src/</code> by <code>tools/single.py</code>, so change those instead.<br> <br>
<code>./build.sh</code> remakes it every time you build, so run <code>git checkout goome.h</code> before you commit and leave it out of the PR. I remake it after merging.
</p>
</details>

<details>
<summary align="center"><b>🤖 Using AI 🤖</b></summary>

<p align="center">
Using AI to help write your PR is allowed. Just:<br> <br>
• Say in the PR that AI helped<br>
• Know what your code does. If you get asked about something you should know how to answer and not need to ask an AI.<br>
• Test it yourself. Untested AI dumps get closed
</p>
</details>

<details>
<summary align="center"><b>🧪 Testing 🧪</b></summary>

<p align="center">
Build it and run the unit tests first. On linux:
</p>

```sh
./build.sh test

# same but warnings count as errors, like CI
WERROR=1 ./build.sh test
```

<p align="center">
<code>./build.sh win</code> cross builds the windows one with mingw into <code>build-win/</code> (<code>CC</code> picks another compiler). Then run the backend test that fits your change. They each run somewhere that cant mess with your real desktop:
</p>

```sh
# x11 on Xvfb, with picom and without a compositor
# X11_SCRATCH = where it puts its stuff, X11_DISPLAY = which display (default :91)
# X11_TOOLS = a folder with picom in usr/bin if you dont have it installed
tests/x11.sh
tests/x11.sh asan

# wayland on headless sway, checked with grim
# SCRATCH = where it puts its stuff, WL_INCLUDE = where wayland-client.h is
# WTYPE = path to wtype if its not on PATH, SAN=1 for asan+ubsan
tests/wayland.sh

# gnome capture through mutter, plus the portal code on a fake portal
# GN_BIN = a sanitizer build of tests/gnome_cap.c if you want one
tests/gnome.sh

# windows build under wine on its own Xvfb, builds build-win first
# W32_SCRATCH = where it puts its stuff, PY = which python, CC = which mingw
tests/win32.sh

# real windows, after ./build.sh win
pwsh tests/win32.ps1

# the mac stuff
tests/macos.sh
```

<p align="center">
They're heavy (Xvfb, sway, wine), dont run them all at once. Without the env vars they put everything in <code>test-out/</code>.<br> <br>
The unit tests check pixel hashes of the drawing code, and those have to come out the same on every OS. If your change moves a hash on purpose, say why in the PR and update it in <code>tests/unit.c</code> in the same PR.<br> <br>
In the PR write a few lines on what you ran and on which OS. Backend changes also need the desktop or compositor and its version you tried it on (like "sway 1.10 on Arch" or "KDE Plasma 6.2, Fedora 41").<br> <br>
Drawing, the ui widgets and the examples all count as something you can see, so they need a before and after screenshot.<br> <br>
CI runs on linux, windows and both macs when you open the PR. It has to be green before I merge.
</p>
</details>

<details>
<summary align="center"><b>✏️ Code style ✏️</b></summary>

<p align="center">
• Plain C99, no new libraries or dependencies<br>
• Nothing gets linked and nothing is required. Anything optional gets dlopened on linux and mac, LoadLibrary'd on windows, mac stuff goes through the objc runtime. No libm either<br>
• Everything ends up in one translation unit, so every <code>static</code> in <code>src/</code> needs its file's prefix or it clashes with another file's<br>
• Linux builds have to keep working on glibc 2.34. That means no <code>atoi</code>, <code>sscanf</code>, <code>fscanf</code>, <code>strtol</code> and friends, parse numbers by hand with <code>gm_num</code> like the rest of the code does. CI checks this with objdump<br>
• Match the code around it. Same naming, same formatting, short names are fine inside functions<br>
• Comments only where something isnt obvious. Short and casual, like the ones already in there<br>
• Dont reformat whole files, it makes the diff impossible to read
</p>
</details>

<details>
<summary align="center"><b>✅ PR checklist ✅</b></summary>

<p align="center">
☐ Its against dev<br>
☐ It does one thing<br>
☐ I changed src/ or include/ and left goome.h out of the PR<br>
☐ I didnt touch any protected files<br>
☐ No new dependencies<br>
☐ I tested it and wrote how, on which OS and desktop<br>
☐ Pixel hashes didnt change, or I said why they did<br>
☐ Screenshots if you can see it<br>
☐ CI is green<br>
☐ I said if AI helped, and I know what every line does
</p>
</details>

<details>
<summary align="center"><b>🛡️ Security 🛡️</b></summary>

<p align="center">
Found a security hole? Dont open a public issue or PR for it, read <a href="https://github.com/goome-ui/.github/blob/main/SECURITY.md">SECURITY.md</a> and tell me privately first.
</p>
</details>

<p align="center">
By sending a PR you agree your code goes out under the same <a href="LICENSE">GPL-3.0 license</a> as the rest of c-goome.
</p>
