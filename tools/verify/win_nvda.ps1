param([string]$out = "verify-out")
$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force $out | Out-Null
function Emit($cell, $ran, $result, $detail, $ver = "") { python -I tools/verify/emit.py $cell $ran $result $detail $ver }
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$A = [System.Windows.Automation.AutomationElement]
$scope = [System.Windows.Automation.TreeScope]
function Find($root, $name) {
    $c = New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, $name)
    return $root.FindFirst($scope::Descendants, $c)
}
function Drive($app, $tag) {
    $p = Start-Process -FilePath $app -ArgumentList "60" -RedirectStandardOutput "$out\a11y_app-$tag.log" -PassThru -NoNewWindow
    $cond = New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, "goome a11y")
    $win = $null
    for ($i = 0; $i -lt 60 -and $win -eq $null; $i++) { Start-Sleep -Milliseconds 250; $win = $A::RootElement.FindFirst($scope::Children, $cond) }
    if ($win -eq $null) { return "goome window not found in uia" }
    Start-Sleep -Seconds 1
    Add-Type -AssemblyName Microsoft.VisualBasic
    Add-Type -AssemblyName System.Windows.Forms
    try { [Microsoft.VisualBasic.Interaction]::AppActivate($p.Id) } catch {}
    Start-Sleep -Seconds 1
    foreach ($i in 1..4) { [System.Windows.Forms.SendKeys]::SendWait("{TAB}"); Start-Sleep -Milliseconds 1200 }
    $log = @()
    foreach ($n in "Name", "Press me", "Dark mode", "Volume", "Press me") {
        $el = Find $win $n
        if ($el) { try { $el.SetFocus(); $log += "focused $n" } catch { $log += "focus $n failed: $_" } } else { $log += "no element $n" }
        Start-Sleep -Milliseconds 1500
    }
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    return ($log -join "; ")
}

$cell = "screenreader:windows-nvda"
$nvdaLog = Join-Path (Resolve-Path $out) "nvda.log"
choco install nvda -y --no-progress *> "$out\choco-nvda.log"
$exe = Get-ChildItem "C:\Program Files (x86)\NVDA\nvda.exe", "C:\Program Files\NVDA\nvda.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $exe) {
    $tail = (Get-Content "$out\choco-nvda.log" -Tail 8) -join " | "
    Emit $cell 0 fail "NVDA did not install through chocolatey: $tail"
} else {
    $ver = (Get-Item $exe.FullName).VersionInfo.ProductVersion
    $proc = Start-Process -FilePath $exe.FullName -ArgumentList "-m", "--log-file=$nvdaLog", "--log-level=10", "--disable-addons" -PassThru
    Start-Sleep -Seconds 15
    $alive = -not $proc.HasExited
    $drive = Drive "build-win/a11y_app.exe" "nvda"
    Start-Sleep -Seconds 3
    $lines = if (Test-Path $nvdaLog) { Get-Content $nvdaLog } else { @() }
    $speak = $lines | Where-Object { $_ -match "Speaking" } 
    $hit = $speak | Where-Object { $_ -match "Press me" } | Select-Object -First 3
    $nspeak = ($speak | Measure-Object).Count
    if ($hit) {
        Emit $cell 1 pass "NVDA $ver ran with -m, log shows it speaking the button label: $($hit -join ' ; '); $nspeak Speaking lines in total; drive: $drive" $ver
    } else {
        $some = ($speak | Select-Object -First 4) -join " ; "
        $tail = ($lines | Select-Object -Last 6) -join " | "
        Emit $cell 1 fail "NVDA $ver alive=$alive; no speech line with Press me. Speaking lines: $nspeak ($some); drive: $drive; log tail: $tail" $ver
    }
    Stop-Process -Name nvda -Force -ErrorAction SilentlyContinue
}

$cell = "screenreader:windows-narrator"
$n = Start-Process narrator.exe -PassThru -ErrorAction SilentlyContinue
Start-Sleep -Seconds 8
$alive = (Get-Process -Name Narrator -ErrorAction SilentlyContinue | Measure-Object).Count
$logs = (Get-WinEvent -ListLog "*Narrator*" -ErrorAction SilentlyContinue | ForEach-Object { $_.LogName }) -join ","
$drive = Drive "build-win/a11y_app.exe" "narrator"
$ev = ""
if ($logs) { $ev = (Get-WinEvent -LogName ($logs -split ",")[0] -MaxEvents 5 -ErrorAction SilentlyContinue | ForEach-Object { $_.Message }) -join " | " }
Stop-Process -Name Narrator -Force -ErrorAction SilentlyContinue
Emit $cell 1 fail "Narrator process count $alive after launch; Narrator has no speech log or capture hook and the runner has no audio device, so what it said could not be read. event logs: '$logs' $ev; drive: $drive"
exit 0
