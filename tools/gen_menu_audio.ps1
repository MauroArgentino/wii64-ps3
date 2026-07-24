# Generate ambient synth pad WAV for wii64-ps3 menu
# Original composition - no copyrighted material used

$sampleRate = 48000
$channels = 2
$bitsPerSample = 16
$duration = 28.0
$numSamples = [int]($sampleRate * $duration)
$halfCycle = [Math]::PI

Write-Host "Generating $duration seconds of ambient synth pad..."

# Pad oscillators: frequencies for a relaxed Cm9 chord (C Eb G Bb D)
$oscillators = @(
    @{ freq = 130.81; amp = 0.18; detune = -1.2 }   # C3
    @{ freq = 155.56; amp = 0.14; detune = 0.8 }     # Eb3
    @{ freq = 196.00; amp = 0.13; detune = -0.5 }    # G3
    @{ freq = 233.08; amp = 0.10; detune = 1.5 }     # Bb3
    @{ freq = 293.66; amp = 0.08; detune = -0.7 }    # D4
    @{ freq = 65.41;  amp = 0.12; detune = 0.3 }     # C2 (sub bass)
    @{ freq = 261.63; amp = 0.06; detune = -1.0 }    # C4 (octave)
)

# Stereo detune offsets (cents)
$stereoCentsL = 3.0
$stereoCentsR = -3.0
$centsRatio = [Math]::Pow(2, 1.0 / 1200.0)

# LFO rates for movement
$lfoRates = @(
    @{ rate = 0.08; depth = 0.3 }   # Very slow amplitude modulation
    @{ rate = 0.13; depth = 0.2 }   # Slightly different rate
    @{ rate = 0.05; depth = 0.15 }  # Very slow pad swell
)

# Filter: simple 3-tap moving average for warmth
$filterSize = 12
$filterL = New-Object double[] $filterSize
$filterR = New-Object double[] $filterSize
$filterIdx = 0

# Simple low-pass: keep running sums
$filterSumL = 0.0
$filterSumR = 0.0

# Envelope: smooth fade in/out
$fadeInSamples = $sampleRate * 3    # 3 sec fade in
$fadeOutSamples = $sampleRate * 4   # 4 sec fade out

$samples = New-Object double[] ($numSamples * 2)  # interleaved L/R

Write-Host "Computing samples..."

for ($i = 0; $i -lt $numSamples; $i++) {
    $t = $i / $sampleRate
    $progress = [int]($i * 100 / $numSamples)
    if (($i % ($sampleRate * 2)) -eq 0) {
        Write-Host "  $progress%..."
    }

    $sampleL = 0.0
    $sampleR = 0.0

    # Compute all oscillators
    foreach ($osc in $oscillators) {
        $f = $osc.freq
        $a = $osc.amp
        $d = $osc.detune

        # LFO modulation on amplitude
        $lfoMod = 1.0
        foreach ($lfo in $lfoRates) {
            $lfoMod += $lfo.depth * [Math]::Sin(2.0 * $halfCycle * $lfo.rate * $t + $osc.freq * 0.01)
        }
        $lfoMod = [Math]::Max(0.0, [Math]::Min(2.0, $lfoMod))

        # Very slow pitch drift for analog feel
        $pitchDrift = 1.0 + 0.0003 * [Math]::Sin(2.0 * $halfCycle * 0.02 * $t)

        # Left channel (slightly detuned)
        $fL = $f * [Math]::Pow($centsRatio, $d + $stereoCentsL) * $pitchDrift
        $phaseL = 2.0 * $halfCycle * $fL * $t
        $sampleL += $a * $lfoMod * [Math]::Sin($phaseL)

        # Right channel (slightly detuned)
        $fR = $f * [Math]::Pow($centsRatio, $d + $stereoCentsR) * $pitchDrift
        $phaseR = 2.0 * $halfCycle * $fR * $t
        $sampleR += $a * $lfoMod * [Math]::Sin($phaseR)

        # Add 2nd harmonic for richness (quiet)
        $sampleL += ($a * 0.08) * $lfoMod * [Math]::Sin(2.0 * $phaseL)
        $sampleR += ($a * 0.08) * $lfoMod * [Math]::Sin(2.0 * $phaseR)
    }

    # Envelope
    $env = 1.0
    if ($i -lt $fadeInSamples) {
        # Smooth cosine fade in
        $env = 0.5 * (1.0 - [Math]::Cos($halfCycle * $i / $fadeInSamples))
    }
    elseif ($i -gt ($numSamples - $fadeOutSamples)) {
        # Smooth cosine fade out
        $remaining = $numSamples - $i
        $env = 0.5 * (1.0 + [Math]::Cos($halfCycle * $remaining / $fadeOutSamples))
    }

    $sampleL *= $env
    $sampleR *= $env

    # Simple low-pass filter (moving average)
    $filterSumL -= $filterL[$filterIdx]
    $filterSumR -= $filterR[$filterIdx]
    $filterL[$filterIdx] = $sampleL
    $filterR[$filterIdx] = $sampleR
    $filterSumL += $sampleL
    $filterSumR += $sampleR
    $filterIdx = ($filterIdx + 1) % $filterSize

    $filteredL = $filterSumL / $filterSize
    $filteredR = $filterSumR / $filterSize

    # Soft clip (tanh approximation) for warmth
    $filteredL = [Math]::Tanh($filteredL * 2.5)
    $filteredR = [Math]::Tanh($filteredR * 2.5)

    # Store interleaved
    $samples[$i * 2]     = $filteredL
    $samples[$i * 2 + 1] = $filteredR
}

Write-Host "Writing WAV file..."

# Write WAV
$outPath = "C:\Users\tupri\OneDrive\Documentos\PROGRAMACION\wii64-ps3\menu_audio.wav"
$fs = [System.IO.File]::Create($outPath)
$bw = [System.IO.BinaryWriter]::new($fs)

# RIFF header
$dataSize = $numSamples * $channels * ($bitsPerSample / 8)
$fileSize = 36 + $dataSize

$bw.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
$bw.Write([BitConverter]::GetBytes($fileSize))
$bw.Write([System.Text.Encoding]::ASCII.GetBytes("WAVE"))

# fmt chunk
$bw.Write([System.Text.Encoding]::ASCII.GetBytes("fmt "))
$bw.Write([BitConverter]::GetBytes(16))                    # chunk size
$bw.Write([BitConverter]::GetBytes([uint16]1))              # PCM
$bw.Write([BitConverter]::GetBytes([uint16]$channels))
$bw.Write([BitConverter]::GetBytes([uint32]$sampleRate))
$bw.Write([BitConverter]::GetBytes([uint32]($sampleRate * $channels * ($bitsPerSample / 8))))  # byte rate
$bw.Write([BitConverter]::GetBytes([uint16]($channels * ($bitsPerSample / 8))))  # block align
$bw.Write([BitConverter]::GetBytes([uint16]$bitsPerSample))

# data chunk
$bw.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
$bw.Write([BitConverter]::GetBytes($dataSize))

# Write samples (interleaved stereo 16-bit)
$bytes = New-Object byte[] ($dataSize)
for ($i = 0; $i -lt $numSamples; $i++) {
    # Clamp to [-1, 1] and convert to 16-bit
    $valL = [Math]::Max(-1.0, [Math]::Min(1.0, $samples[$i * 2]))
    $valR = [Math]::Max(-1.0, [Math]::Min(1.0, $samples[$i * 2 + 1]))

    $intL = [int16]([Math]::Round($valL * 32767))
    $intR = [int16]([Math]::Round($valR * 32767))

    $bytes[$i * 4]     = [byte]($intL -band 0xFF)
    $bytes[$i * 4 + 1] = [byte](($intL -shr 8) -band 0xFF)
    $bytes[$i * 4 + 2] = [byte]($intR -band 0xFF)
    $bytes[$i * 4 + 3] = [byte](($intR -shr 8) -band 0xFF)
}

$bw.Write($bytes)
$bw.Close()
$fs.Close()

$sizeMB = [Math]::Round((Get-Item $outPath).Length / 1MB, 2)
Write-Host "Done! Output: $outPath ($sizeMB MB, ${duration}s, ${sampleRate}Hz, ${channels}ch, ${bitsPerSample}bit)"
