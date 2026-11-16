param([string]$exe = "build-win/a11y_app.exe", [string]$out = "ci-out")
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out "a11y_app.log"
$A = [System.Windows.Automation.AutomationElement]
$scope = [System.Windows.Automation.TreeScope]
$bad = 0
function need($ok, $what) {
    if ($ok) { Write-Host "ok   $what" } else { Write-Host "FAIL $what"; $script:bad++ }
}
function byname($root, $name) {
    $c = New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, $name)
    return $root.FindFirst($scope::Descendants, $c)
}
function dump($el, $depth) {
    if ($depth -gt 6) { return }
    $w = [System.Windows.Automation.TreeWalker]::ControlViewWalker
    $c = $w.GetFirstChild($el)
    while ($c -ne $null) {
        Write-Host ("  " * $depth + $c.Current.ControlType.ProgrammaticName + " '" + $c.Current.Name + "'")
        dump $c ($depth + 1)
        $c = $w.GetNextSibling($c)
    }
}
$p = Start-Process -FilePath $exe -ArgumentList "40" -RedirectStandardOutput $log -PassThru -NoNewWindow
$win = $null
$cond = New-Object System.Windows.Automation.PropertyCondition($A::NameProperty, "goome a11y")
for ($i = 0; $i -lt 60 -and $win -eq $null; $i++) {
    Start-Sleep -Milliseconds 250
    $win = $A::RootElement.FindFirst($scope::Children, $cond)
}
need ($win -ne $null) "the goome window shows up in uia"
if ($win -eq $null) { Stop-Process -Id $p.Id -Force; exit 1 }
Start-Sleep -Milliseconds 800
$win = $A::RootElement.FindFirst($scope::Children, $cond)
dump $win 1
$CT = [System.Windows.Automation.ControlType]
$btn = byname $win "Press me"
$chk = byname $win "Dark mode"
$sld = byname $win "Volume"
$ent = byname $win "Name"
$cmb = byname $win "Fruit"
need ($btn -ne $null -and $btn.Current.ControlType -eq $CT::Button) "button by name"
need ($chk -ne $null -and $chk.Current.ControlType -eq $CT::CheckBox) "check box by name"
need ($sld -ne $null -and $sld.Current.ControlType -eq $CT::Slider) "slider by name"
need ($ent -ne $null -and $ent.Current.ControlType -eq $CT::Edit) "edit by name"
need ($cmb -ne $null -and $cmb.Current.ControlType -eq $CT::ComboBox) "combo by name"
need ((byname $win "Screen reader test") -ne $null) "label text"
if ($btn) {
    $r = $btn.Current.BoundingRectangle
    need ($r.Width -gt 10 -and $r.Height -gt 10) "button has a size $($r.Width)x$($r.Height)"
    $btn.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
}
if ($chk) { $chk.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Toggle() }
if ($sld) {
    $rv = $sld.GetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern)
    need ($rv.Current.Value -eq 3 -and $rv.Current.Maximum -eq 10) "slider is 3 of 0..10"
    $rv.SetValue(7.5)
}
if ($ent) {
    $vp = $ent.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    need ($vp.Current.Value -eq "Bo") "edit says Bo"
    $vp.SetValue("Robin")
}
if ($cmb) { $cmb.GetCurrentPattern([System.Windows.Automation.ExpandCollapsePattern]::Pattern).Expand() }
Start-Sleep -Milliseconds 1000
if ($chk) { need ($chk.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Current.ToggleState -eq [System.Windows.Automation.ToggleState]::On) "check box is on now" }
if ($sld) { need ($sld.GetCurrentPattern([System.Windows.Automation.RangeValuePattern]::Pattern).Current.Value -eq 7.5) "slider reads 7.5 back" }
if ($ent) { need ($ent.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value -eq "Robin") "edit reads Robin back" }
$fig = byname $win "Fig"
need ($fig -ne $null) "combo list is open with Fig in it"
if ($fig) { $fig.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Select() }
Start-Sleep -Milliseconds 600
$q = byname $win "Quit"
if ($q) { $q.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() }
if (-not $p.WaitForExit(10000)) { Stop-Process -Id $p.Id -Force }
Start-Sleep -Milliseconds 300
$said = Get-Content $log -Raw
Write-Host "--- app said:"
Write-Host $said
need ($said -match "pressed 1") "app saw the press"
need ($said -match "dark 1") "app saw the toggle"
need ($said -match "volume 7.5") "app saw the slider value"
need ($said -match "name Robin") "app saw the new name"
need ($said -match "bye fruit 2") "app saw the combo pick"
if ($bad) { Write-Host "uia FAILED ($bad)"; exit 1 }
Write-Host "uia ok"
