Add-Type -AssemblyName System.Drawing

$code = @"
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Collections.Generic;

public class ThemeAssetGenerator {
    public static Bitmap CropAndCenterMaster(string srcPath, bool isWhiteBg, int size = 1024) {
        using (Bitmap raw = new Bitmap(srcPath)) {
            Bitmap master = new Bitmap(size, size, PixelFormat.Format32bppArgb);
            using (Graphics g = Graphics.FromImage(master)) {
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                g.CompositingQuality = CompositingQuality.HighQuality;
                g.Clear(Color.Transparent);

                // Source geometry: center (511, 511), radius ~484
                float pad = 8f;
                float dstW = size - pad * 2f;
                RectangleF dstRect = new RectangleF(pad, pad, dstW, dstW);
                RectangleF srcRect = new RectangleF(511f - 484f, 511f - 484f, 968f, 968f);

                using (GraphicsPath clipPath = new GraphicsPath()) {
                    clipPath.AddEllipse(dstRect);
                    g.SetClip(clipPath);
                    g.DrawImage(raw, dstRect, srcRect, GraphicsUnit.Pixel);
                }
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
        }
        return bmp;
    }

    public static void SaveIcoFile(string outputPath, List<Bitmap> bitmaps) {
        using (FileStream fs = new FileStream(outputPath, FileMode.Create, FileAccess.Write))
        using (BinaryWriter bw = new BinaryWriter(fs)) {
            bw.Write((short)0);
            bw.Write((short)1);
            bw.Write((short)bitmaps.Count);

            int offset = 6 + bitmaps.Count * 16;
            List<byte[]> pngDataList = new List<byte[]>();

            foreach (Bitmap b in bitmaps) {
                using (MemoryStream ms = new MemoryStream()) {
                    b.Save(ms, ImageFormat.Png);
                    byte[] pngBytes = ms.ToArray();
                    pngDataList.Add(pngBytes);

                    byte wByte = (byte)(b.Width >= 256 ? 0 : b.Width);
                    byte hByte = (byte)(b.Height >= 256 ? 0 : b.Height);
                    bw.Write(wByte);
                    bw.Write(hByte);
                    bw.Write((byte)0);
                    bw.Write((byte)0);
                    bw.Write((short)1);
                    bw.Write((short)32);
                    bw.Write((int)pngBytes.Length);
                    bw.Write(offset);

                    offset += pngBytes.Length;
                }
            }

            foreach (byte[] pngBytes in pngDataList) {
                bw.Write(pngBytes);
            }
        }
    }
}
"@

Add-Type -TypeDefinition $code -ReferencedAssemblies "System.Drawing"

$uploadsDir = "C:\Users\kamah\.gemini\antigravity\brain\3a88f21f-7118-44fe-9ec6-e2dae6026bd8\.user_uploaded"
$imagesDir  = "E:\project\Dhikr Reminder\Azkar Reminder\assets\images"
$iconsDir   = "E:\project\Dhikr Reminder\Azkar Reminder\assets\icons"

$themes = @(
    @{ Name = "dormant"; Src = "$uploadsDir\media_1791744735709_9858a86c.jpg"; WhiteBg = $false },
    @{ Name = "red";     Src = "$uploadsDir\media_1791744735712_42559359.jpg"; WhiteBg = $false },
    @{ Name = "blue";    Src = "$uploadsDir\media_1791744735716_5b1c5f9d.jpg"; WhiteBg = $false },
    @{ Name = "yellow";  Src = "$uploadsDir\media_1791744735720_496bb539.jpg"; WhiteBg = $false },
    @{ Name = "gold";    Src = "$uploadsDir\media_1791746317755_122f693c.jpg"; WhiteBg = $true  },
    @{ Name = "silver";  Src = "$uploadsDir\media_1791744735724_3aaed088.jpg"; WhiteBg = $true  }
)

foreach ($t in $themes) {
    Write-Host "Processing theme: $($t.Name)..."
    $master = [ThemeAssetGenerator]::CropAndCenterMaster($t.Src, $t.WhiteBg, 1024)
    $masterPath = Join-Path $imagesDir ("logo_{0}.png" -f $t.Name)
    $master.Save($masterPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Host "  Saved $masterPath"

    $small = [ThemeAssetGenerator]::GenerateCrispIcon($master, 48)
    $smallPath = Join-Path $imagesDir ("logo_{0}_small.png" -f $t.Name)
    $small.Save($smallPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $small.Dispose()
    Write-Host "  Saved $smallPath"

    # Multi-res icon
    $sizesFull = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
    $listFull = New-Object 'System.Collections.Generic.List[System.Drawing.Bitmap]'
    foreach ($sz in $sizesFull) {
        $listFull.Add([ThemeAssetGenerator]::GenerateCrispIcon($master, $sz))
    }
    $icoFullPath = Join-Path $iconsDir ("app_{0}.ico" -f $t.Name)
    [ThemeAssetGenerator]::SaveIcoFile($icoFullPath, $listFull)
    foreach ($b in $listFull) { $b.Dispose() }
    Write-Host "  Saved $icoFullPath"

    # Small icon
    $sizesSmall = @(16, 20, 24, 32, 48)
    $listSmall = New-Object 'System.Collections.Generic.List[System.Drawing.Bitmap]'
    foreach ($sz in $sizesSmall) {
        $listSmall.Add([ThemeAssetGenerator]::GenerateCrispIcon($master, $sz))
    }
    $icoSmallPath = Join-Path $iconsDir ("app_{0}_small.ico" -f $t.Name)
    [ThemeAssetGenerator]::SaveIcoFile($icoSmallPath, $listSmall)
    foreach ($b in $listSmall) { $b.Dispose() }
    Write-Host "  Saved $icoSmallPath"

    $master.Dispose()
}

Write-Host "All theme assets generated successfully!"
