# Regenerate the two original offline phrases on Windows. No network service.
param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$dashRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $dashRoot
New-Item -ItemType Directory -Force -Path .tools\audio | Out-Null
Add-Type -AssemblyName System.Speech
$dashSynth = New-Object System.Speech.Synthesis.SpeechSynthesizer
try {
    $dashSynth.Rate = 1
    $dashSynth.SetOutputToWaveFile((Join-Path $dashRoot '.tools\audio\pig.wav'))
    $dashSynth.Speak('Pig detected! Act natural!')
    $dashSynth.SetOutputToNull()
    $dashSynth.SetOutputToWaveFile((Join-Path $dashRoot '.tools\audio\fuzz.wav'))
    $dashSynth.Speak('Oh man, the fuzz!')
} finally { $dashSynth.Dispose() }
& $Python tools\pack_voice.py
if ($LASTEXITCODE -ne 0) { throw 'Voice packing failed' }
