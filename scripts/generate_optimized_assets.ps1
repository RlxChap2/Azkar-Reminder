Add-Type -AssemblyName System.Drawing

$code = @"
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Collections.Generic;

public class FullAssetGenerator {
    public static Bitmap CropAndCenterMaster(string srcPath, int size = 1024) {
        using (Bitmap raw = new Bitmap(srcPath)) {
            Bitmap master = new Bitmap(size, size, PixelFormat.Format32bppArgb);
            using (Graphics g = Graphics.FromImage(master)) {
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                g.CompositingQuality = CompositingQuality.HighQuality;
                g.Clear(Color.Transparent);

                // Source bounding circle: center (511, 508.5), radius ~484
                // We crop closely to the circle leaving 8px margin
                float pad = 8f;
                float dstW = size - pad * 2f;
                RectangleF srcRect = new RectangleF(511f - 484f, 508.5f - 484f, 968f, 968f);
                RectangleF dstRect = new RectangleF(pad, pad, dstW, dstW);
                g.DrawImage(raw, dstRect, srcRect, GraphicsUnit.Pixel);
            }
            return master;
        }
    }

    public static Bitmap GenerateCrispIcon(Bitmap master1024, int targetSize) {
        Bitmap bmp = new Bitmap(targetSize, targetSize, PixelFormat.Format32bppArgb);
        using (Graphics g = Graphics.FromImage(bmp)) {
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.CompositingQuality = CompositingQuality.HighQuality;
            g.Clear(Color.Transparent);

            float pad = (targetSize <= 32) ? 0.75f : ((targetSize <= 64) ? 1.5f : 3.0f);
            float drawSize = targetSize - pad * 2.0f;
            RectangleF dstRect = new RectangleF(pad, pad, drawSize, drawSize);
            RectangleF srcRect = new RectangleF(0, 0, master1024.Width, master1024.Height);
            g.DrawImage(master1024, dstRect, srcRect, GraphicsUnit.Pixel);

            // Optical ring reinforcement for small icon sizes (<= 96px)
            // Ensures the outer neon ring never collapses below Nyquist sampling limit
            if (targetSize <= 96) {
                float ringWidth;
                if (targetSize <= 20) ringWidth = 1.35f;
                else if (targetSize <= 24) ringWidth = 1.50f;
                else if (targetSize <= 32) ringWidth = 1.75f;
                else if (targetSize <= 48) ringWidth = 2.10f;
                else if (targetSize <= 64) ringWidth = 2.60f;
                else ringWidth = 3.20f;

                float cx = targetSize * 0.5f;
                float cy = targetSize * 0.5f;
                float r = (drawSize - ringWidth) * 0.5f;

                // Subtle outer lime aura
                using (Pen aura = new Pen(Color.FromArgb(70, 183, 243, 74), ringWidth + 1.2f)) {
                    g.DrawEllipse(aura, cx - r, cy - r, r * 2f, r * 2f);
                }
                // Crisp vibrant lime stroke (#B7F34A)
                using (Pen ring = new Pen(Color.FromArgb(240, 195, 248, 45), ringWidth)) {
                    g.DrawEllipse(ring, cx - r, cy - r, r * 2f, r * 2f);
                }
            }
        }
        return bmp;
    }

    public static byte[] BitmapToPngBytes(Bitmap b) {
        using (MemoryStream ms = new MemoryStream()) {
            b.Save(ms, ImageFormat.Png);
            return ms.ToArray();
        }
    }

    public static byte[] BitmapToDibIcoFrame(Bitmap b) {
        int w = b.Width;
        int h = b.Height;
        int andStride = ((w + 31) / 32) * 4;
        int andSize = andStride * h;
        int xorSize = w * h * 4;
        int totalSize = 40 + xorSize + andSize;

        byte[] data = new byte[totalSize];
        using (MemoryStream ms = new MemoryStream(data))
        using (BinaryWriter bw = new BinaryWriter(ms)) {
            bw.Write((uint)40); // biSize
            bw.Write((int)w);   // biWidth
            bw.Write((int)(h * 2)); // biHeight (doubled for XOR + AND)
            bw.Write((ushort)1); // biPlanes
            bw.Write((ushort)32); // biBitCount
            bw.Write((uint)0); // biCompression = BI_RGB
            bw.Write((uint)xorSize); // biSizeImage
            bw.Write((int)0);
            bw.Write((int)0);
            bw.Write((uint)0);
            bw.Write((uint)0);

            BitmapData bd = b.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            byte[] row = new byte[w * 4];
            for (int y = h - 1; y >= 0; y--) {
                System.Runtime.InteropServices.Marshal.Copy((IntPtr)((long)bd.Scan0 + y * bd.Stride), row, 0, w * 4);
                bw.Write(row);
            }
            b.UnlockBits(bd);

            byte[] andMask = new byte[andSize];
            bw.Write(andMask);
        }
        return data;
    }

    public static void SaveIco(List<Bitmap> bitmaps, string outPath) {
        List<byte[]> frames = new List<byte[]>();
        List<int> sizes = new List<int>();
        foreach (var b in bitmaps) {
            if (b.Width >= 256) {
                frames.Add(BitmapToPngBytes(b));
            } else {
                frames.Add(BitmapToDibIcoFrame(b));
            }
            sizes.Add(b.Width);
        }

        using (FileStream fs = new FileStream(outPath, FileMode.Create, FileAccess.Write))
        using (BinaryWriter bw = new BinaryWriter(fs)) {
            bw.Write((ushort)0); // reserved
            bw.Write((ushort)1); // type 1 = icon
            bw.Write((ushort)frames.Count); // image count

            int offset = 6 + 16 * frames.Count;
            for (int i = 0; i < frames.Count; i++) {
                int s = sizes[i];
                byte w = (byte)(s >= 256 ? 0 : s);
                byte h = (byte)(s >= 256 ? 0 : s);
                bw.Write(w);
                bw.Write(h);
                bw.Write((byte)0); // palette colors
                bw.Write((byte)0); // reserved
                bw.Write((ushort)1); // planes
                bw.Write((ushort)32); // bpp
                bw.Write((uint)frames[i].Length); // byte length
                bw.Write((uint)offset); // offset
                offset += frames[i].Length;
            }

            for (int i = 0; i < frames.Count; i++) {
                bw.Write(frames[i]);
            }
        }
    }
}
"@

Add-Type -TypeDefinition $code -ReferencedAssemblies "System.Drawing.dll"

$srcMaster = "C:\Users\kamah\.gemini\antigravity\brain\3a88f21f-7118-44fe-9ec6-e2dae6026bd8\.user_uploaded\media_1791616860422_88ee7eed.png"
if (-not (Test-Path $srcMaster)) {
    throw "Source master not found: $srcMaster"
}

Write-Host "1. Processing Master 1024x1024..."
$master = [FullAssetGenerator]::CropAndCenterMaster($srcMaster, 1024)
$masterPath = Join-Path $PSScriptRoot "..\assets\images\logo.png"
$master.Save($masterPath, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "   Saved: $masterPath"

Write-Host "2. Generating logo_small.png (128x128 with optical ring preservation)..."
$small128 = [FullAssetGenerator]::GenerateCrispIcon($master, 128)
$smallPath = Join-Path $PSScriptRoot "..\assets\images\logo_small.png"
$small128.Save($smallPath, [System.Drawing.Imaging.ImageFormat]::Png)
$small128.Dispose()
Write-Host "   Saved: $smallPath"

Write-Host "3. Generating multi-resolution ICO files..."
$icoSizes = @(256, 128, 64, 48, 32, 24, 16)
$icoBitmaps = New-Object System.Collections.Generic.List[System.Drawing.Bitmap]
foreach ($s in $icoSizes) {
    $bmp = [FullAssetGenerator]::GenerateCrispIcon($master, $s)
    $icoBitmaps.Add($bmp)
}

$appIcoPath = Join-Path $PSScriptRoot "..\assets\icons\app.ico"
[FullAssetGenerator]::SaveIco($icoBitmaps, $appIcoPath)
Write-Host "   Saved: $appIcoPath ($($icoSizes -join ', '))"

$smallIcoSizes = @(48, 32, 24, 16)
$smallIcoBitmaps = New-Object System.Collections.Generic.List[System.Drawing.Bitmap]
foreach ($b in $icoBitmaps) {
    if ($smallIcoSizes -contains $b.Width) {
        $smallIcoBitmaps.Add($b)
    }
}
$appSmallIcoPath = Join-Path $PSScriptRoot "..\assets\icons\app_small.ico"
[FullAssetGenerator]::SaveIco($smallIcoBitmaps, $appSmallIcoPath)
Write-Host "   Saved: $appSmallIcoPath ($($smallIcoSizes -join ', '))"

# Cleanup icon bitmaps
foreach ($b in $icoBitmaps) { $b.Dispose() }

Write-Host "4. Generating Windows Store / MSIX packaging assets..."
function Make-StoreAsset {
    param($w, $h, $logoW, $outPath, $bgColor = [System.Drawing.Color]::Transparent)
    $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.Clear($bgColor)

    $logo = [FullAssetGenerator]::GenerateCrispIcon($master, $logoW)
    $x = ($w - $logoW) / 2.0
    $y = ($h - $logoW) / 2.0
    $g.DrawImage($logo, $x, $y, $logoW, $logoW)
    $logo.Dispose()
    $g.Dispose()

    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "   Saved Store Asset: $outPath ($w x $h)"
}

$dirs = @(
    (Join-Path $PSScriptRoot "..\package\Assets"),
    (Join-Path $PSScriptRoot "..\package_staging\Assets")
)

foreach ($d in $dirs) {
    if (-not (Test-Path $d)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }
    Make-StoreAsset 44 44 38 (Join-Path $d "Square44x44Logo.png")
    Make-StoreAsset 150 150 126 (Join-Path $d "Square150x150Logo.png")
    Make-StoreAsset 310 310 260 (Join-Path $d "Square310x310Logo.png")
    Make-StoreAsset 310 150 126 (Join-Path $d "Wide310x150Logo.png")
    Make-StoreAsset 50 50 42 (Join-Path $d "StoreLogo.png")
    
    $splashBg = [System.Drawing.Color]::FromArgb(255, 12, 18, 13) # dark olive moss
    Make-StoreAsset 620 300 160 (Join-Path $d "SplashScreen.png") $splashBg
}

$master.Dispose()
Write-Host "SUCCESS: All multi-resolution assets generated with crisp optical clarity!"
