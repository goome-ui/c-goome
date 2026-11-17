param([string]$out = "verify-out")
$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force $out | Out-Null
function Emit($cell, $ran, $result, $detail, $ver = "") { python -I tools/verify/emit.py $cell $ran $result $detail $ver }
Add-Type -AssemblyName System.Windows.Forms
$os = (Get-CimInstance Win32_OperatingSystem).Caption + " " + (Get-CimInstance Win32_OperatingSystem).Version
$before = (Get-WinUserLanguageList | ForEach-Object { $_.LanguageTag + ":" + ($_.InputMethodTips -join ",") }) -join " ; "
Write-Host "os $os; languages before: $before"
$steps = @()
$cap = Get-WindowsCapability -Online -Name "Language.Basic~~~zh-CN*" -ErrorAction SilentlyContinue
$steps += "zh-CN basic capability state: " + (($cap | ForEach-Object { $_.Name + "=" + $_.State }) -join ",")
try { Add-WindowsCapability -Online -Name "Language.Basic~~~zh-CN~0.0.1.0" -ErrorAction Stop | Out-Null; $steps += "Add-WindowsCapability zh-CN ok" } catch { $steps += "Add-WindowsCapability zh-CN failed: $($_.Exception.Message)" }
try { Install-Language -Language zh-CN -ErrorAction Stop | Out-Null; $steps += "Install-Language ok" } catch { $steps += "Install-Language failed: $($_.Exception.Message)" }
$tip = "0804:{81D4E9C9-1D3B-41BC-9E6C-4B40BF79E35E}{FA550B04-5AD7-411F-A5AC-CA038EC515D7}"
try {
    $l = New-WinUserLanguageList zh-CN
    Set-WinUserLanguageList $l -Force -ErrorAction Stop
    $steps += "Set-WinUserLanguageList zh-CN ok"
} catch { $steps += "Set-WinUserLanguageList failed: $($_.Exception.Message)" }
$after = (Get-WinUserLanguageList | ForEach-Object { $_.LanguageTag + ":" + ($_.InputMethodTips -join ",") }) -join " ; "
$steps += "languages after: $after"
$cell = "ime:windows-ms-pinyin"
$tipc = @"
#include <windows.h>
#include <initguid.h>
#include <msctf.h>
#include <stdio.h>
#define TF_IPPMF_ENABLEPROFILE 0x1
#define TF_IPPMF_FORPROCESS 0x10000000
#define TF_IPPMF_FORSESSION 0x20000000
DEFINE_GUID(CLSID_X, 0x81D4E9C9, 0x1D3B, 0x41BC, 0x9E, 0x6C, 0x4B, 0x40, 0xBF, 0x79, 0xE3, 0x5E);
DEFINE_GUID(PROF_X, 0xFA550B04, 0x5AD7, 0x411F, 0xA5, 0xAC, 0xCA, 0x03, 0x8E, 0xC5, 0x15, 0xD7);
DEFINE_GUID(CLSID_PROFILES, 0x33C53A50, 0xF456, 0x4884, 0xB0, 0x49, 0x85, 0xFD, 0x64, 0x3E, 0xCF, 0xED);
DEFINE_GUID(IID_MGR, 0x71C6E74C, 0x0F28, 0x11D8, 0xA8, 0x2A, 0x00, 0x06, 0x5B, 0x84, 0x43, 0x5C);
int main(void)
{
    CoInitialize(NULL);
    ITfInputProcessorProfileMgr *m = NULL;
    HRESULT h = CoCreateInstance(&CLSID_PROFILES, NULL, CLSCTX_INPROC_SERVER, &IID_MGR, (void **)&m);
    printf("create %lx\n", (long)h);
    if (FAILED(h)) return 1;
    h = m->lpVtbl->ActivateProfile(m, TF_PROFILETYPE_INPUTPROCESSOR, 0x0804, &CLSID_X, &PROF_X, NULL, TF_IPPMF_FORSESSION | TF_IPPMF_FORPROCESS | TF_IPPMF_ENABLEPROFILE);
    printf("activate %lx\n", (long)h);
    return FAILED(h) ? 2 : 0;
}
"@
Set-Content -Path "$out\tip.c" -Value $tipc
$ccout = & gcc -o "$out\tip.exe" "$out\tip.c" -lole32 -luuid 2>&1
$steps += "tip helper build: " + ($(if (Test-Path "$out\tip.exe") { "ok" } else { "failed " + ($ccout -join " ") }))
$tiplog = ""
if (Test-Path "$out\tip.exe") { $tiplog = (& "$out\tip.exe" 2>&1) -join " " }
$steps += "tip activate: $tiplog"
$log = "$out\ime_ui-win.log"
$p = Start-Process -FilePath "build-win/ime_ui.exe" -RedirectStandardOutput $log -PassThru -NoNewWindow
Start-Sleep -Seconds 4
Add-Type -AssemblyName Microsoft.VisualBasic
try { [Microsoft.VisualBasic.Interaction]::AppActivate($p.Id) } catch { $steps += "AppActivate failed" }
Start-Sleep -Seconds 1
$tipc2 = & "$out\tip.exe" 2>&1
Start-Sleep -Seconds 1
foreach ($k in "n", "i", "h", "a", "o") { [System.Windows.Forms.SendKeys]::SendWait($k); Start-Sleep -Milliseconds 300 }
Start-Sleep -Seconds 1
[System.Windows.Forms.SendKeys]::SendWait(" ")
Start-Sleep -Seconds 1
[System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
Start-Sleep -Seconds 2
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
$lines = if (Test-Path $log) { Get-Content $log -Encoding UTF8 } else { @() }
$pre = ($lines | Where-Object { $_ -match "^preedit" } | Select-Object -First 4) -join " ; "
$txt = ($lines | Where-Object { $_ -match "^(text|buf) " }) -join " ; "
$detail = "$os; steps: $($steps -join ' | '); goome log: preedit [$pre] text [$txt] all: $(($lines | Select-Object -First 8) -join ' / ')"
$zh = [string]::new([char[]]@(0x4F60, 0x597D))
if ($txt -match $zh) { Emit $cell 1 pass $detail $os } else { Emit $cell 1 fail $detail $os }
exit 0
