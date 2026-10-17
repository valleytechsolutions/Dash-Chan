# Regenerate the two original offline phrases on Windows. No network service.
param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$dashRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $dashRoot
New-Item -ItemType Directory -Force -Path .tools\audio | Out-Null
Add-Type -AssemblyName System.Speech
$dashSynth = New-Object System.Speech.Synthesis.SpeechSynthesizer
try {
    $dashVoice = $dashSynth.GetInstalledVoices() | Where-Object { $_.Enabled -and $_.VoiceInfo.Culture.Name -eq 'en-US' -and $_.VoiceInfo.Gender -eq 'Female' } | Select-Object -First 1
    if ($dashVoice) { $dashSynth.SelectVoice($dashVoice.VoiceInfo.Name) }
    $dashSynth.Volume = 100
    $dashSynth.SetOutputToWaveFile((Join-Path $dashRoot '.tools\audio\pig.wav'))
    $dashSynth.SpeakSsml('<speak version="1.0" xmlns="http://www.w3.org/2001/10/synthesis" xml:lang="en-US"><prosody rate="fast" pitch="+22%" volume="x-loud"><emphasis level="strong">Pig detected!</emphasis><break time="80ms"/><prosody rate="x-fast" pitch="+30%">Act natural! Act natural!</prosody></prosody></speak>')
    $dashSynth.SetOutputToNull()
    $dashSynth.SetOutputToWaveFile((Join-Path $dashRoot '.tools\audio\fuzz.wav'))
    $dashSynth.SpeakSsml('<speak version="1.0" xmlns="http://www.w3.org/2001/10/synthesis" xml:lang="en-US"><prosody rate="fast" pitch="+20%" volume="x-loud">Oh man!<break time="70ms"/><emphasis level="strong"><prosody pitch="+30%">The fuzz!</prosody></emphasis><break time="70ms"/>Act natural!</prosody></speak>')
} finally { $dashSynth.Dispose() }
& $Python tools\pack_voice.py
if ($LASTEXITCODE -ne 0) { throw 'Voice packing failed' }
