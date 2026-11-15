# win32 backend on a real windows desktop (github windows-latest has one)
# pwsh tests/win32.ps1 [-bin build-win] [-out w32-out]   exes come from ./build.sh win, run from the repo root
param([string]$bin = "build-win", [string]$out = "w32-out")
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force $out | Out-Null
$bin = (Resolve-Path $bin).Path
$out = (Resolve-Path $out).Path

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint f, IntPtr e);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    public static int Diff(byte[] ppm, int off, int pw, int px0, int py0, byte[] bgra, int stride, int bx, int by, int w, int h, int step, int tol) {
        int bad = 0;
        for (int y = 0; y < h; y += step)
            for (int x = 0; x < w; x += step) {
                int p = off + ((py0 + y) * pw + px0 + x) * 3, q = (by + y) * stride + (bx + x) * 4;
                if (Math.Abs(ppm[p] - bgra[q + 2]) > tol || Math.Abs(ppm[p + 1] - bgra[q + 1]) > tol || Math.Abs(ppm[p + 2] - bgra[q]) > tol) bad++;
            }
        return bad;
    }
}
"@
[void][W]::SetProcessDPIAware() # real px for CopyFromScreen and SetCursorPos, like goome itself
Add-Type -AssemblyName System.Windows.Forms, System.Drawing

$script:fails = 0
function Pass($m) { Write-Host "PASS $m" }
function Fail($m) { Write-Host "FAIL $m"; $script:fails++ }
function Info($m) { Write-Host "info $m" }
function Pump($ms) {
    $end = (Get-Date).AddMilliseconds($ms)
    while ((Get-Date) -lt $end) { [Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 15 }
}
$scr = [Windows.Forms.Screen]::PrimaryScreen.Bounds
function Shot($name) {
    $b = New-Object Drawing.Bitmap $scr.Width, $scr.Height
    $gr = [Drawing.Graphics]::FromImage($b)
    $gr.CopyFromScreen($scr.X, $scr.Y, 0, 0, $b.Size)
    $gr.Dispose()
    if ($name) { $b.Save("$out\$name.png") }
    $b
}
function Bits($bmp) {
    $r = New-Object Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
    $d = $bmp.LockBits($r, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $a = New-Object byte[] ($d.Stride * $bmp.Height)
    [Runtime.InteropServices.Marshal]::Copy($d.Scan0, $a, 0, $a.Length)
    $bmp.UnlockBits($d)
    @{ a = $a; s = $d.Stride; w = $bmp.Width; h = $bmp.Height }
}
function Ppm($path) {
    $a = [IO.File]::ReadAllBytes($path)
    $nl = 0; $i = 0
    while ($nl -lt 3) { if ($a[$i] -eq 10) { $nl++ }; $i++ }
    $head = [Text.Encoding]::ASCII.GetString($a, 0, $i).Split([char[]]" `n", [StringSplitOptions]::RemoveEmptyEntries)
    @{ a = $a; off = $i; w = [int]$head[1]; h = [int]$head[2] }
}
function Near($c, $r, $g, $b) { [Math]::Abs($c.R - $r) -le 6 -and [Math]::Abs($c.G - $g) -le 6 -and [Math]::Abs($c.B - $b) -le 6 }
function Click($x, $y, [switch]$now) {
    [void][W]::SetCursorPos($x, $y)
    if (-not $now) { Pump 300 }
    [W]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); [W]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
    Pump 500
}
function Key([byte]$vk, [byte[]]$mods = @()) {
    foreach ($m in $mods) { [W]::keybd_event($m, 0, 0, [IntPtr]::Zero) }
    [W]::keybd_event($vk, 0, 0, [IntPtr]::Zero); [W]::keybd_event($vk, 0, 2, [IntPtr]::Zero)
    foreach ($m in $mods) { [W]::keybd_event($m, 0, 2, [IntPtr]::Zero) }
    Pump 250
}
function FgPid { $p = [uint32]0; [void][W]::GetWindowThreadProcessId([W]::GetForegroundWindow(), [ref]$p); $p }

$u = & "$bin\unit.exe" | Out-String
if ($u -match 'all good') { Pass unit } else { Fail unit; Write-Host $u }

# a plain window under everything that logs clicks
$under = New-Object Windows.Forms.Form
$under.FormBorderStyle = 'None'; $under.StartPosition = 'Manual'; $under.Location = $scr.Location; $under.Size = $scr.Size; $under.TopMost = $true # over the runner terminal, hello still goes on top of it
$under.BackColor = [Drawing.Color]::FromArgb(48, 80, 112)
$script:underClicks = New-Object Collections.ArrayList
$under.Add_MouseDown({ param($s, $e) [void]$script:underClicks.Add("$($e.X),$($e.Y)") })
$under.Show(); $under.Activate(); Pump 500

$hello = Start-Process "$bin\hello.exe" -PassThru -RedirectStandardOutput "$out\hello.txt" -RedirectStandardError "$out\hello.err"
Pump 3000
function HelloSays($pat) { (Get-Content "$out\hello.txt" -Raw) -match $pat }

# 1 open
$txt = Get-Content "$out\hello.txt" -Raw
Write-Host $txt
$m = [regex]::Match($txt, 'monitor 0: \S+ (\d+)x(\d+) at (-?\d+),(-?\d+) scale ([\d.]+)')
if ($txt -match 'backend: win32 \+ win32' -and $m.Success) { Pass open } else { Fail open }
$s = if ($m.Success) { [double]$m.Groups[5].Value } else { 1.0 }
$mx = if ($m.Success) { [int]$m.Groups[3].Value } else { 0 }
$my = if ($m.Success) { [int]$m.Groups[4].Value } else { 0 }
function P($l, $o) { [int][Math]::Round($o + ($l - $o) * $s) } # logical -> real px, see the coords note in src/win32.c
function PX($l) { P $l $mx }
function PY($l) { P $l $my }
if ($txt -match 'no global hotkeys') { Fail 'hotkey register' } else { Pass 'hotkey register' }

# 2 draw, outside shot + our own grab
$b = Shot draw
$frame = $b.GetPixel((PX 81) - $scr.X, (PY 140) - $scr.Y); $fill = $b.GetPixel((PX 150) - $scr.X, (PY 100) - $scr.Y)
if ((Near $frame 128 144 160) -and $fill.R -lt 80 -and $fill.B -lt 90) { Pass 'draw (CopyFromScreen)' } else { Fail "draw (CopyFromScreen) frame=$frame fill=$fill" }
& "$bin\grab.exe" 0 80 80 280 120 "$out\box.ppm" | Write-Host
$g = Ppm "$out\box.ppm"; $bb = Bits $b
$bad = [W]::Diff($g.a, $g.off, $g.w, 0, 0, $bb.a, $bb.s, (PX 80) - $scr.X, (PY 80) - $scr.Y, $g.w, $g.h, 1, 6)
if ($g.w -eq (P 360 80) - (P 80 80) -and $bad -lt 50) { Pass "draw (own grab, $bad px off)" } else { Fail "draw (own grab) $($g.w)x$($g.h), $bad px off" }

# 3 click-through
$script:underClicks.Clear()
Click (PX 200) (PY 130)
if (HelloSays 'click 200,130') { Pass 'click on the box reaches hello' } else { Fail 'click on the box reaches hello' }
if ($script:underClicks.Count -eq 0) { Pass 'click on the box stays with hello' } else { Fail "click on the box leaked: $($script:underClicks)" }
Click (PX 600) (PY 400)
if ($script:underClicks.Count -ge 1) { Pass "click next to it reaches the window under ($($script:underClicks))" } else { Fail 'click next to it reaches the window under' }
if (HelloSays 'click 600,400') { Fail 'hello saw the outside click' } else { Pass 'hello didnt see the outside click' }
$script:underClicks.Clear()
[void][W]::SetCursorPos((PX 200), (PY 130)); Pump 300
Click (PX 600) (PY 400) -now # moved off the box and clicked before the next poll flipped us back to click-through
if ($script:underClicks.Count -ge 1 -and -not (HelloSays 'click 600,400')) { Pass 'fast click off the box still reaches the window under' } else { Fail "fast click off the box: under=$($script:underClicks)" }

# 4 keys
$before = FgPid
Click (PX 200) (PY 174)
if ((FgPid) -eq $hello.Id) { Pass 'keyboard on takes focus' } else { Fail "keyboard on takes focus (fg pid $(FgPid))" }
Key 0x48 @(0x10); Key 0x49; Key 0x25
if ((HelloSays "key 104 mods 100 text 'H'") -and (HelloSays "key 105 mods 0 text 'i'")) { Pass 'typed text' } else { Fail 'typed text' }
if (HelloSays "key 144 mods 0 text ''") { Pass 'arrow key' } else { Fail 'arrow key' }

# 8 hotkey alt+h
function Frame($b) { $c = $b.GetPixel((PX 81) - $scr.X, (PY 140) - $scr.Y); (Near $c 128 144 160) -or (Near $c 111 220 95) } # gray, green after an odd number of clicks
Key 0x48 @(0x12); Pump 300
$b = Shot hidden
if (-not (Frame $b)) { Pass 'alt+h hides the box' } else { Fail 'alt+h hides the box' }
Key 0x48 @(0x12); Pump 300
$b = Shot shown
if (Frame $b) { Pass 'alt+h brings it back' } else { Fail 'alt+h brings it back' }

Click (PX 200) (PY 174) # typing off
if ((FgPid) -ne $hello.Id) { Pass "keyboard off gives focus back (fg pid $(FgPid), was $before)" } else { Fail 'keyboard off gives focus back' }

Click (PX 343) (PY 90)
Pump 800
if ($hello.HasExited) { Pass 'x quits hello' } else { Fail 'x quits hello'; Stop-Process -Id $hello.Id -Force }
$under.Close()

# 3c a hit rect bigger than the drawing: its empty part still takes clicks
$hitexe = "$bin\win32_hit.exe"
if (Test-Path $hitexe) {
    $under = New-Object Windows.Forms.Form
    $under.FormBorderStyle = 'None'; $under.StartPosition = 'Manual'; $under.Location = $scr.Location; $under.Size = $scr.Size; $under.TopMost = $true # over the runner terminal, hello still goes on top of it
    $script:underClicks.Clear()
    $under.Add_MouseDown({ param($s2, $e) [void]$script:underClicks.Add("$($e.X),$($e.Y)") })
    $under.Show(); Pump 500
    $hp = Start-Process $hitexe -ArgumentList 9 -PassThru -RedirectStandardOutput "$out\hit.txt"
    Pump 3000
    Click (PX 300) (PY 200)
    $ht = Get-Content "$out\hit.txt" -Raw
    if ($ht -match 'click 300,200' -and $script:underClicks.Count -eq 0) { Pass 'click on an empty part of a hit reaches us' } else { Fail "empty part of a hit: under=$($script:underClicks)" }
    Click (PX 500) (PY 400)
    if ($script:underClicks.Count -ge 1) { Pass 'click outside the hit still goes through' } else { Fail 'click outside the hit still goes through' }
    if ($ht -match 'alpha at the empty spot 0, on the box 255') { Pass 'alpha 1 trick doesnt leak into goome_alpha_at' } else { Fail "alpha leak: $ht" }
    $hp.WaitForExit(10000) | Out-Null
    $under.Close()
} else { Info 'no win32_hit.exe (tests/win32.sh builds it), empty hit test skipped' }

# 5 capture, a known picture on screen with the mouse parked on it
$kb = New-Object Drawing.Bitmap 400, 300
$gr = [Drawing.Graphics]::FromImage($kb)
$rnd = New-Object Random 7
for ($y = 0; $y -lt 300; $y += 20) { for ($x = 0; $x -lt 400; $x += 20) {
        $br = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb($rnd.Next(256), $rnd.Next(256), $rnd.Next(256)))
        $gr.FillRectangle($br, $x, $y, 20, 20); $br.Dispose()
    } }
$gr.Dispose()
$pic = New-Object Windows.Forms.Form
$pic.FormBorderStyle = 'None'; $pic.StartPosition = 'Manual'; $pic.TopMost = $true; $pic.ShowInTaskbar = $false
$pic.Location = New-Object Drawing.Point ($scr.X + 100), ($scr.Y + 50); $pic.ClientSize = New-Object Drawing.Size 400, 300
$pic.BackgroundImage = $kb
$pic.Show(); Pump 800
[void][W]::SetCursorPos($scr.X + 300, $scr.Y + 200); Pump 300
$kbits = Bits $kb
if ($s -eq 1.0) {
    & "$bin\grab.exe" 0 100 50 400 300 "$out\region.ppm" | Write-Host
    & "$bin\grab.exe" "$out\full.ppm" | Write-Host
    $sb = Bits (Shot capture)
    $r = Ppm "$out\region.ppm"; $f = Ppm "$out\full.ppm"
    $d1 = [W]::Diff($r.a, $r.off, $r.w, 0, 0, $kbits.a, $kbits.s, 0, 0, 400, 300, 1, 0)
    $d2 = [W]::Diff($f.a, $f.off, $f.w, 100, 50, $kbits.a, $kbits.s, 0, 0, 400, 300, 1, 0)
    $d3 = [W]::Diff($f.a, $f.off, $f.w, 0, 0, $sb.a, $sb.s, 0, 0, $f.w, $f.h, 1, 0)
    if ($r.w -eq 400 -and $r.h -eq 300 -and $d1 -eq 0) { Pass 'region grab exact' } else { Fail "region grab $($r.w)x$($r.h), $d1 px off" }
    if ($d2 -eq 0) { Pass 'full grab exact on the picture' } else { Fail "full grab, $d2 px off on the picture" }
    if ($d3 -eq 0) { Pass 'full grab == CopyFromScreen (no cursor in it)' } else { Fail "full grab vs CopyFromScreen: $d3 px off" }
} else { Info "capture compare skipped, primary is at scale $s" }
$pic.Close()

# hide_self, the exe comes from tests/win32.sh or gets built here
$hide = "$bin\win32_hide.exe"
if (-not (Test-Path $hide) -and (Get-Command gcc -ErrorAction SilentlyContinue)) {
    gcc -std=c99 -O2 -Iinclude -o $hide tests/win32_hide.c "$bin\libgoome.a"
}
if (Test-Path $hide) {
    $h = & $hide | Out-String
    Write-Host $h.Trim()
    if ($h -match 'hide 1 has_self 0 center \w+ not magenta') { Pass 'hide_self keeps us out of grabs' }
    elseif ($h -match 'hide 0 has_self 1 center \w+ magenta') { Pass 'hide_self not available, grab says has_self' }
    else { Fail 'hide_self says one thing, the grab shows another' }
} else { Info 'no win32_hide.exe and no gcc, hide_self skipped' }

# window mode
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class WM {
    [StructLayout(LayoutKind.Sequential)] public struct CI { public int size, flags; public IntPtr cur; public int x, y; }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string title);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool GetCursorInfo(ref CI c);
    [DllImport("user32.dll")] public static extern IntPtr LoadCursorW(IntPtr i, IntPtr id);
    public static IntPtr Cursor() { CI c = new CI(); c.size = Marshal.SizeOf(typeof(CI)); GetCursorInfo(ref c); return c.cur; }
}
"@
$wmexe = "$bin\winmode.exe"
if (-not (Test-Path $wmexe) -and (Get-Command gcc -ErrorAction SilentlyContinue)) { gcc -std=c99 -O2 -Iinclude -o $wmexe tests/winmode.c "$bin\libgoome.a" }
function WmSays($f, $pat) { (Get-Content $f -Raw) -match $pat }
function WmReady($f) {
    for ($i = 0; $i -lt 80; $i++) { if ((Test-Path $f) -and (WmSays $f '(?m)^ready')) { return $true }; Pump 250 }
    $false
}
function WmClient($f) {
    $m = [regex]::Match((Get-Content $f -Raw), 'ready client (-?\d+),(-?\d+) size (\d+)x(\d+) scale ([\d.]+)')
    @{ x = [int]$m.Groups[1].Value; y = [int]$m.Groups[2].Value; w = [int]$m.Groups[3].Value; h = [int]$m.Groups[4].Value; s = [double]$m.Groups[5].Value }
}
if (Test-Path $wmexe) {
    $wf = "$out\wm.txt"
    $wp = Start-Process $wmexe -ArgumentList 'plain', 90 -PassThru -RedirectStandardOutput $wf -RedirectStandardError "$out\wm.err"
    if (WmReady $wf) { Pass 'window ready' } else { Fail 'window ready'; Get-Content $wf | Write-Host }
    foreach ($c in 'open', 'frame', 'clipboard', 'clipboard cut', 'cursor', 'resize', 'canvas follows', 'pos', 'minimize', 'restore', 'maximize', 'back to normal', 'capture') {
        if (WmSays $wf "(?m)^ok $c") { Pass "window $c" } else { Fail "window $c" }
    }
    if (WmSays $wf "(?m)^drop 50,60 'C:\\goome\\dropped .+ file\.txt' full 'C:\\goome\\dropped .+ file\.txt'") { Pass 'window drop' } else { Fail 'window drop' }
    $c = WmClient $wf
    function WX($l) { [int][Math]::Round($c.x + $l * $c.s) }
    function WY($l) { [int][Math]::Round($c.y + $l * $c.s) }
    $b = Shot window
    $red = $b.GetPixel((WX 60) - $scr.X, (WY 60) - $scr.Y); $bg = $b.GetPixel((WX 400) - $scr.X, (WY 300) - $scr.Y)
    if ((Near $red 224 64 96) -and (Near $bg 32 48 64)) { Pass 'window on screen' } else { Fail "window on screen red=$red bg=$bg" }
    $clip = Get-Clipboard -Raw
    if ($clip -eq "goome clip $([char]0xe9) 1`r`nline 2") { Pass 'clipboard reaches windows with crlf' } else { Fail "clipboard reaches windows: '$clip'" }
    Set-Clipboard -Value 'from ps1'
    Pump 1200
    if (WmSays $wf "(?m)^paste 'from ps1'") { Pass 'clipboard from windows' } else { Fail 'clipboard from windows' }
    Click (WX 300) (WY 200)
    if ((WmSays $wf '(?m)^down 300,200 1') -and (WmSays $wf '(?m)^up 300,200 1')) { Pass 'window click' } else { Fail 'window click' }
    [void][W]::SetCursorPos((WX 320), (WY 250)); Pump 400
    $ibeam = [WM]::LoadCursorW([IntPtr]::Zero, [IntPtr]32513)
    $cur = [WM]::Cursor()
    if ($cur -eq $ibeam) { Pass 'ibeam cursor over the window' } elseif ($cur -eq [IntPtr]::Zero) { Info 'no mouse on this desktop, the cursor is hidden so its shape cant be read' } else { Fail "ibeam cursor over the window ($cur vs $ibeam)" }
    Key 0x48 @(0x10); Key 0x49; Key 0x25
    if ((WmSays $wf "key 104 mods 100 text 'H'") -and (WmSays $wf "key 105 mods 0 text 'i'") -and (WmSays $wf 'key 144 mods 0')) { Pass 'window keys' } else { Fail 'window keys' }
    [void][W]::SetCursorPos((WX ($c.w - 8)), (WY ($c.h - 8))); Pump 300
    [W]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); Pump 300
    for ($i = 1; $i -le 10; $i++) { [void][W]::SetCursorPos((WX ($c.w - 8)) + 6 * $i, (WY ($c.h - 8)) + 4 * $i); Pump 60 }
    [W]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero); Pump 800
    $m = [regex]::Match((Get-Content $wf -Raw), 'sized ok (\d+)x(\d+)')
    if ($m.Success -and [Math]::Abs([int]$m.Groups[1].Value - $c.w - 60 / $c.s) -le 3 -and [Math]::Abs([int]$m.Groups[2].Value - $c.h - 40 / $c.s) -le 3) { Pass "edge drag resizes ($($m.Value))" } else { Fail "edge drag resizes: $($m.Value)" }
    if ($m.Success) { $c.w = [int]$m.Groups[1].Value; $c.h = [int]$m.Groups[2].Value }
    [void][W]::SetCursorPos((WX 60), (WY 15)); Pump 300
    [W]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); Pump 300
    for ($i = 1; $i -le 10; $i++) { [void][W]::SetCursorPos((WX 60) + 12 * $i, (WY 15) + 6 * $i); Pump 60 }
    [W]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero); Pump 800
    $m = [regex]::Match((Get-Content $wf -Raw), 'dragged ok pos (-?\d+),(-?\d+)')
    $c2 = $c
    if ($m.Success) { $c2 = @{ x = [int]$m.Groups[1].Value; y = [int]$m.Groups[2].Value; w = $c.w; h = $c.h; s = $c.s } }
    if ($m.Success -and [Math]::Abs($c2.x - $c.x - 120 / $c.s) -le 3 -and [Math]::Abs($c2.y - $c.y - 60 / $c.s) -le 3) { Pass "drag moves the window ($($c.x),$($c.y) -> $($c2.x),$($c2.y))" } else { Fail "drag moves the window: $($m.Value)" }
    $c = $c2
    if (WmSays $wf '(?m)^monitors \d+x\d+') { Pass 'resize sends monitors' } else { Fail 'resize sends monitors' }
    $h = [WM]::FindWindowW('goome_window', 'goome winmode')
    [void][WM]::PostMessageW($h, 0x10, [IntPtr]::Zero, [IntPtr]::Zero); Pump 600
    if ((WmSays $wf '(?m)^close 1') -and -not $wp.HasExited) { Pass 'WM_CLOSE sends close, the app stays up' } else { Fail 'WM_CLOSE sends close' }
    Click (WX 300) (WY 200)
    Key 0x73 @(0x12); Pump 1000
    if ($wp.HasExited) { Pass 'alt+f4 is the second close' } else { Fail 'alt+f4 is the second close'; Stop-Process -Id $wp.Id -Force }
    if (WmSays $wf '(?m)^bye fails 0') { Pass 'window run had no fails' } else { Fail 'window run had fails'; Get-Content $wf | Select-String '^bad' | Write-Host }

    foreach ($k in 'borderless', 'two') {
        $f = "$out\wm-$k.txt"
        $p = Start-Process $wmexe -ArgumentList $k, 3 -PassThru -RedirectStandardOutput $f
        $p.WaitForExit(60000) | Out-Null
        if (WmSays $f '(?m)^bye fails 0') { Pass "$k window" } else { Fail "$k window"; Get-Content $f | Write-Host }
    }

    $under = New-Object Windows.Forms.Form
    $under.FormBorderStyle = 'None'; $under.StartPosition = 'Manual'; $under.Location = $scr.Location; $under.Size = $scr.Size; $under.TopMost = $true
    $under.BackColor = [Drawing.Color]::FromArgb(48, 80, 112)
    $script:underClicks.Clear()
    $under.Add_MouseDown({ param($s3, $e) [void]$script:underClicks.Add("$($e.X),$($e.Y)") })
    $under.Show(); Pump 500
    $f = "$out\wm-trans.txt"
    $p = Start-Process $wmexe -ArgumentList 'trans', 20 -PassThru -RedirectStandardOutput $f
    if (WmReady $f) {
        Pump 800
        $c = WmClient $f
        $b = Shot window-trans
        $red = $b.GetPixel((WX 60) - $scr.X, (WY 60) - $scr.Y); $half = $b.GetPixel((WX 260) - $scr.X, (WY 80) - $scr.Y); $hole = $b.GetPixel((WX 400) - $scr.X, (WY 300) - $scr.Y)
        if ((Near $red 224 64 96) -and (Near $half 24 40 56) -and (Near $hole 48 80 112)) { Pass 'transparent window blends per pixel' } else { Fail "transparent window red=$red half=$half hole=$hole" }
        Click (WX 60) (WY 60)
        Click (WX 400) (WY 300)
        if ((WmSays $f '(?m)^down 60,60 1') -and $script:underClicks.Count -ge 1 -and -not (WmSays $f '(?m)^down 400,300')) { Pass 'transparent window: hit takes the click, the hole lets it through' } else { Fail "transparent clicks: under=$($script:underClicks)" }
    } else { Fail 'transparent window ready' }
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    $under.Close()
} else { Info 'no winmode.exe and no gcc, window mode skipped' }

foreach ($t in 'win_game', 'grab_ui') {
    if (-not (Test-Path "$bin\$t.exe") -and (Get-Command gcc -ErrorAction SilentlyContinue)) { gcc -std=c99 -O2 -Iinclude -o "$bin\$t.exe" "tests/$t.c" "$bin\libgoome.a" }
}
if ((Test-Path "$bin\win_game.exe") -and (Test-Path "$bin\grab_ui.exe")) {
    $gf = "$out\game.txt"; $uf = "$out\grab_ui.txt"
    function WaitIn($f, $pat) { for ($i = 0; $i -lt 50; $i++) { if ((Test-Path $f) -and ((Get-Content $f -Raw) -match $pat)) { return $true }; Pump 100 }; $false }
    function Lines($f) { @(Get-Content $f).Count }
    function After($f, $n, $pat) { for ($i = 0; $i -lt 50; $i++) { if (@(@(Get-Content $f) | Select-Object -Skip $n) -match $pat) { return $true }; Pump 100 }; $false }
    function MouseTo($x, $y) { [W]::mouse_event(0x8001, [int](($x - $scr.X) * 65535 / ($scr.Width - 1)), [int](($y - $scr.Y) * 65535 / ($scr.Height - 1)), 0, [IntPtr]::Zero); Pump 150 }
    $gp = Start-Process "$bin\win_game.exe" -ArgumentList (PX 450), (PY 260), (P 500 0), (P 300 0), 60 -PassThru -RedirectStandardOutput $gf
    $up = Start-Process "$bin\grab_ui.exe" -ArgumentList 50 -PassThru -RedirectStandardOutput $uf
    [void](WaitIn $uf '(?m)^ready'); [void](WaitIn $gf '(?m)^game'); Pump 500
    $caps = [regex]::Match((Get-Content $uf -Raw), 'caps ([0-9a-f]+)').Groups[1].Value
    if ($caps -and ([Convert]::ToInt32($caps, 16) -band 0x100)) { Pass "grab cap ($caps)" } else { Fail "grab cap ($caps)" }
    Click (PX 700) (PY 400)
    if (WaitIn $gf '(?m)^focus in') { Pass 'grab game has the focus' } else { Fail "grab game has the focus: $(Get-Content $gf -Raw)" }
    MouseTo (PX 720) (PY 410); MouseTo (PX 740) (PY 420)
    if (WaitIn $gf '(?m)^raw') { Pass 'grab game gets raw mouse input first' } else { Fail 'grab game gets raw mouse input first' }
    $n = Lines $gf
    MouseTo (PX 118) (PY 177); Pump 300; [W]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); Pump 300
    if (WaitIn $uf '(?m)^grab 1') { Pass 'grab on' } else { Fail "grab on: $(Get-Content $uf -Raw)" }
    if (After $gf $n '^focus out') { Pass 'grab game lost the focus' } else { Fail 'grab game lost the focus' }
    Pump 300; $n = Lines $gf
    foreach ($p in @(@(600, 350), @(700, 400), @(800, 450), @(900, 500))) { MouseTo (PX $p[0]) (PY $p[1]) }
    Pump 400
    $mid = @(@(Get-Content $gf) | Select-Object -Skip $n) -match '^(raw|move)'
    if ($mid.Count -eq 0) { Pass 'grab game got no raw input or moves during the drag' } else { Fail "grab game got input during the drag: $($mid | Select-Object -First 3)" }
    if (WaitIn $uf '(?m)^val 100') { Pass 'grab slider kept going outside the overlay' } else { Fail 'grab slider kept going outside the overlay' }
    $n = Lines $gf
    [W]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero); Pump 300
    if (WaitIn $uf '(?m)^grab 0') { Pass 'grab off on release' } else { Fail 'grab off on release' }
    if (After $gf $n '^focus in') { Pass 'grab game has the focus back' } else { Fail "grab game has the focus back: $(Get-Content $gf -Raw)" }
    MouseTo (PX 750) (PY 430); MouseTo (PX 760) (PY 440)
    if (After $gf $n '^raw') { Pass 'grab game gets raw input again' } else { Fail 'grab game gets raw input again' }
    foreach ($q in $gp, $up) { if (-not $q.HasExited) { Stop-Process -Id $q.Id -Force } }
} else { Info 'no win_game.exe / grab_ui.exe and no gcc, grab skipped' }

# msvc: the one file goome.h has to build with cl in c mode
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2>$null
if ($vs) {
    $one = "$out\hello_one.c"
    $root = (Get-Location).Path
    Set-Content $one "#define GOOME_IMPLEMENTATION`n#include `"goome.h`"`n#include `"examples/hello.c`""
    Push-Location $out
    $log = cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && cl /nologo /W3 /TC /I`"$root`" /Fehello_msvc.exe hello_one.c 2>&1"
    Pop-Location
    $log | Write-Host
    $warn = @($log | Select-String -Pattern 'warning C').Count
    if (Test-Path "$out\hello_msvc.exe") { Pass "msvc cl /W3 build ($warn warnings)" } else { Fail 'msvc cl /W3 build' }
} else { Info 'no visual studio here, msvc build skipped' }

Info 'hotplug and mixed dpi need a real multi monitor box'
if ($script:fails -eq 0) { Write-Host 'win32 desktop: all passed' } else { Write-Host "win32 desktop: $($script:fails) failed" }
exit $script:fails
