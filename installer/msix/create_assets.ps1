# create_assets.ps1 — Generates all required Microsoft Store / MSIX tile & logo assets from logo.png
Add-Type -AssemblyName System.Drawing

$srcPath = Join-Path $PSScriptRoot "..\..\assets\images\logo.png"
if (!(Test-Path $srcPath)) {
    throw "Source logo not found at $srcPath"
}

$assetsDir = Join-Path $PSScriptRoot "Assets"
if (!(Test-Path $assetsDir)) {
    New-Item -ItemType Directory -Path $assetsDir -Force | Out-Null
}

$srcImage = [System.Drawing.Image]::FromFile($srcPath)

function Resize-Image {
    param (
        [string]$outputPath,
        [int]$width,
        [int]$height,
        [bool]$letterbox = $false,
        [System.Drawing.Color]$bgColor = [System.Drawing.Color]::FromArgb(15, 23, 42) # Slate-900 #0F172A
    )

    $bmp = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bmp)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    if ($letterbox) {
        $graphics.Clear($bgColor)
        # Scale to fit inside without distortion
        $scale = [Math]::Min([double]$width / $srcImage.Width, [double]$height / $srcImage.Height) * 0.75
        $drawW = [int]($srcImage.Width * $scale)
        $drawH = [int]($srcImage.Height * $scale)
        $posX = [int](($width - $drawW) / 2)
        $posY = [int](($height - $drawH) / 2)
        $graphics.DrawImage($srcImage, $posX, $posY, $drawW, $drawH)
    } else {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.DrawImage($srcImage, 0, 0, $width, $height)
    }

    $graphics.Dispose()
    $bmp.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "Created: $(Split-Path $outputPath -Leaf) (${width}x${height})"
}

# 1. Square150x150Logo (Main medium tile)
Resize-Image (Join-Path $assetsDir "Square150x150Logo.png") 150 150
Resize-Image (Join-Path $assetsDir "Square150x150Logo.scale-100.png") 150 150
Resize-Image (Join-Path $assetsDir "Square150x150Logo.scale-200.png") 300 300

# 2. Square44x44Logo (App list icon & taskbar)
Resize-Image (Join-Path $assetsDir "Square44x44Logo.png") 44 44
Resize-Image (Join-Path $assetsDir "Square44x44Logo.scale-100.png") 44 44
Resize-Image (Join-Path $assetsDir "Square44x44Logo.scale-200.png") 88 88
Resize-Image (Join-Path $assetsDir "Square44x44Logo.targetsize-24.png") 24 24
Resize-Image (Join-Path $assetsDir "Square44x44Logo.targetsize-48.png") 48 48

# 3. StoreLogo (Microsoft Store listing icon)
Resize-Image (Join-Path $assetsDir "StoreLogo.png") 50 50
Resize-Image (Join-Path $assetsDir "StoreLogo.scale-100.png") 50 50
Resize-Image (Join-Path $assetsDir "StoreLogo.scale-200.png") 100 100

# 4. Wide310x150Logo (Wide Start menu tile)
Resize-Image (Join-Path $assetsDir "Wide310x150Logo.png") 310 150 $true
Resize-Image (Join-Path $assetsDir "Wide310x150Logo.scale-100.png") 310 150 $true

# 5. SplashScreen (Startup screen)
Resize-Image (Join-Path $assetsDir "SplashScreen.png") 620 300 $true
Resize-Image (Join-Path $assetsDir "SplashScreen.scale-100.png") 620 300 $true

# 6. Small & Large Tiles
Resize-Image (Join-Path $assetsDir "SmallTile.png") 71 71
Resize-Image (Join-Path $assetsDir "LargeTile.png") 310 310

$srcImage.Dispose()
Write-Host "All Microsoft Store assets generated successfully!"
