# goome installer for windows, run it inside your project folder
#   irm https://raw.githubusercontent.com/goome-ui/c-goome/main/get.ps1 | iex
& {
param([string]$Version)
$ErrorActionPreference = 'Stop'
$repo = 'goome-ui/c-goome'
if (-not $Version) { $Version = if ($env:GOOME_VERSION) { $env:GOOME_VERSION } else { 'latest' } }
$Version = $Version.TrimStart('v')
$branch = $env:GOOME_BRANCH
function Say($m) { Write-Host "> $m" -ForegroundColor Green }
function Warn($m) { Write-Host "! $m" -ForegroundColor Yellow }

function Fetch($path, $out) {
    foreach ($b in $refs) {
        try {
            Invoke-WebRequest -UseBasicParsing "https://raw.githubusercontent.com/$repo/$b/$path" -OutFile "$out.part"
            Move-Item -Force "$out.part" $out
            return $true
        } catch {}
        if (Get-Command gh -ErrorAction SilentlyContinue) {
            $raw = & gh api "repos/$repo/contents/$($path)?ref=$b" -H 'Accept: application/vnd.github.raw' 2>$null
            if ($LASTEXITCODE -eq 0 -and $raw) {
                [IO.File]::WriteAllText((Join-Path (Get-Location) $out), ($raw -join "`n") + "`n")
                return $true
            }
        }
    }
    Remove-Item -Force -ErrorAction SilentlyContinue "$out.part"
    return $false
}

function Find-Cc {
    foreach ($c in @('gcc', 'clang', 'cl')) {
        if (Get-Command $c -ErrorAction SilentlyContinue) { return $c }
    }
    return $null
}

$tag = $null
if ($branch) {
    $refs = @($branch, 'dev')
} elseif ($Version -eq 'latest') {
    try { $tag = (Invoke-RestMethod -UseBasicParsing "https://api.github.com/repos/$repo/releases/latest").tag_name } catch {}
    if ($tag) { $refs = @($tag) } else {
        $refs = @('main', 'dev')
        Warn 'couldnt find the latest release, getting goome.h from main instead'
    }
} else {
    $tag = "v$Version"
    $refs = @($tag)
}

if ($tag) { Say "getting goome $tag into $(Get-Location)" } else { Say "getting goome from $(if ($branch) { $branch } else { 'main' }) into $(Get-Location)" }
if ($tag) {
    $got = $false
    try {
        Invoke-WebRequest -UseBasicParsing "https://github.com/$repo/releases/download/$tag/goome.h" -OutFile goome.h.new.part
        Move-Item -Force goome.h.new.part goome.h.new
        $got = $true
    } catch { Remove-Item -Force -ErrorAction SilentlyContinue goome.h.new.part }
    if (-not $got -and -not (Fetch 'goome.h' 'goome.h.new')) { Warn "couldnt download goome $tag, check theres a $tag on https://github.com/$repo/releases (or no internet)"; return }
    if (-not (Select-String -Quiet -SimpleMatch "#define GOOME_VERSION `"$($tag.TrimStart('v'))`"" goome.h.new)) { Warn "the goome.h i got doesnt say its $($tag.TrimStart('v')), have a look at it" }
} elseif (-not (Fetch 'goome.h' 'goome.h.new')) { Warn 'couldnt download goome.h (no internet?)'; return }
if (Test-Path goome.h) {
    Copy-Item -Force goome.h goome.h.old
    Warn 'you already had goome.h, the old one is now goome.h.old'
}
Move-Item -Force goome.h.new goome.h
if (-not (Test-Path hello.c)) {
    if (-not (Fetch 'examples/hello.c' 'hello.c')) { Warn 'couldnt get the example, goome.h is there though' }
}

$cc = Find-Cc
if (-not $cc) {
    Say 'no compiler found, installing gcc from winlibs (winget)'
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Warn 'no winget on this pc. get Visual Studio (Desktop development with C++) or https://winlibs.com first'
        return
    }
    winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT --accept-source-agreements --accept-package-agreements
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
    $cc = Find-Cc
    if (-not $cc) { Warn 'gcc got installed but this window cant see it yet, open a new one and run this again'; return }
}
Say "using $cc"

if (Test-Path hello.c) {
    if (-not (Test-Path goome.c)) { Set-Content -Encoding ascii goome.c "#define GOOME_IMPLEMENTATION`n#include `"goome.h`"" }
    if ($cc -eq 'cl') { & cl /nologo /O2 hello.c goome.c /Fe:hello.exe *> goome-build.log }
    else { & $cc -O2 -o hello.exe hello.c goome.c *> goome-build.log }
    if ($LASTEXITCODE -eq 0) {
        Remove-Item -Force -ErrorAction SilentlyContinue goome-build.log, hello.obj, goome.obj
        Say 'built hello.exe, run it to see a box you can click (alt+h hides it, esc quits)'
    } else {
        Warn 'the example didnt build, whats wrong is in goome-build.log'
        return
    }
}

$build = if ($cc -eq 'cl') { 'cl /O2 myapp.c goome.c' } else { "$cc -O2 -o myapp.exe myapp.c goome.c" }
Write-Host @"

all set. in your own code:
  #include "goome.h"      everywhere you use it
  goome.c                 the one file that has the implementation, just compile it with the rest
build: $build
"@
} @args
