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
