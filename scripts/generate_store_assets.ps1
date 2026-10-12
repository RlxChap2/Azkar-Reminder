Add-Type -AssemblyName System.Drawing

$srcPath = Join-Path $PSScriptRoot "..\assets\images\logo.png"
if (-not (Test-Path $srcPath)) {
    $srcPath = Join-Path $PSScriptRoot "..\..\logo.png"
}

$assetsDir = Join-Path $PSScriptRoot "..\package\Assets"
if (-not (Test-Path $assetsDir)) {
    New-Item -ItemType Directory -Path $assetsDir -Force | Out-Null
}

$srcImg = [System.Drawing.Image]::FromFile($srcPath)

function Resize-Canvas {
    param(
        [int]$canvasW,
        [int]$canvasH,
        [int]$logoMaxW,
        [int]$logoMaxH,
        [string]$outPath,
        [System.Drawing.Color]$bgColor = [System.Drawing.Color]::Transparent
    )

    $bmp = New-Object System.Drawing.Bitmap($canvasW, $canvasH, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality

    if ($bgColor -ne [System.Drawing.Color]::Transparent) {
        $g.Clear($bgColor)
    } else {
        $g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
    }

    $scale = [Math]::Min([double]$logoMaxW / $script:srcImg.Width, [double]$logoMaxH / $script:srcImg.Height)
    $destW = [int]($script:srcImg.Width * $scale)
    $destH = [int]($script:srcImg.Height * $scale)
    $destX = [int](($canvasW - $destW) / 2)
    $destY = [int](($canvasH - $destH) / 2)

    $g.DrawImage($script:srcImg, $destX, $destY, $destW, $destH)
    $g.Dispose()

    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "Generated: $outPath ($canvasW x $canvasH)"
}

# 1. Square44x44Logo
Resize-Canvas 44 44 36 36 (Join-Path $assetsDir "Square44x44Logo.png")

# 2. Square150x150Logo
Resize-Canvas 150 150 120 120 (Join-Path $assetsDir "Square150x150Logo.png")

# 3. Wide310x150Logo
Resize-Canvas 310 150 120 120 (Join-Path $assetsDir "Wide310x150Logo.png")

# 4. Square310x310Logo
Resize-Canvas 310 310 240 240 (Join-Path $assetsDir "Square310x310Logo.png")

# 5. StoreLogo (50x50)
Resize-Canvas 50 50 40 40 (Join-Path $assetsDir "StoreLogo.png")

# 6. SplashScreen (620x300) with dark modern theme background #0F172A
$splashBg = [System.Drawing.Color]::FromArgb(255, 15, 23, 42)
Resize-Canvas 620 300 160 160 (Join-Path $assetsDir "SplashScreen.png") $splashBg

$srcImg.Dispose()
Write-Host "All Microsoft Store visual assets successfully generated!"
