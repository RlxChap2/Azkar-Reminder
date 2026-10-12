Add-Type -AssemblyName System.Drawing

$code = @"
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Collections.Generic;

public class DormantAssetGenerator {
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
                float pad = 8f;
                float dstW = size - pad * 2f;
                RectangleF dstRect = new RectangleF(pad, pad, dstW, dstW);
                RectangleF srcRect = new RectangleF(511f - 484f, 508.5f - 484f, 968f, 968f);

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

            // Optical ring reinforcement for small icon sizes (<= 96px) in silver/chrome
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

                // Subtle outer silver aura
                using (Pen aura = new Pen(Color.FromArgb(60, 142, 153, 145), ringWidth + 1.2f)) {
                    g.DrawEllipse(aura, cx - r, cy - r, r * 2f, r * 2f);
                }
                // Crisp silver/chrome rim stroke (#D0D8D2)
                using (Pen ring = new Pen(Color.FromArgb(230, 208, 216, 210), ringWidth)) {
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

    public static void SaveIco(List<Bitmap> bitmaps, string filePath) {
        using (FileStream fs = new FileStream(filePath, FileMode.Create, FileAccess.Write))
        using (BinaryWriter bw = new BinaryWriter(fs)) {
            bw.Write((ushort)0); // reserved
            bw.Write((ushort)1); // type 1 = ICO
            bw.Write((ushort)bitmaps.Count);

            int headerSize = 6 + bitmaps.Count * 16;
            int offset = headerSize;

            List<byte[]> payloadList = new List<byte[]>();
            for (int i = 0; i < bitmaps.Count; i++) {
                Bitmap b = bitmaps[i];
                byte[] raw = (b.Width >= 256) ? BitmapToPngBytes(b) : BitmapToDibIcoFrame(b);
                payloadList.Add(raw);

                byte wByte = (b.Width >= 256) ? (byte)0 : (byte)b.Width;
                byte hByte = (b.Height >= 256) ? (byte)0 : (byte)b.Height;
                bw.Write(wByte);
                bw.Write(hByte);
                bw.Write((byte)0); // color palette
                bw.Write((byte)0); // reserved
                bw.Write((ushort)1); // color planes
                bw.Write((ushort)32); // bpp
                bw.Write((uint)raw.Length);
                bw.Write((uint)offset);

                offset += raw.Length;
            }

            foreach (byte[] p in payloadList) {
                bw.Write(p);
            }
        }
    }
}
"@

Add-Type -TypeDefinition $code -ReferencedAssemblies "System.Drawing.dll"

$srcImage = "C:\Users\kamah\.gemini\antigravity\brain\3a88f21f-7118-44fe-9ec6-e2dae6026bd8\.user_uploaded\media_1791743874343_0f25259b.jpg"
$assetsDir = Join-Path $PSScriptRoot "..\assets"

Write-Host "1. Cropping & Centering Dormant Master Logo (1024x1024)..."
$master = [DormantAssetGenerator]::CropAndCenterMaster($srcImage, 1024)
$masterPng = Join-Path $assetsDir "images\logo_dormant.png"
$master.Save($masterPng, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host "   Saved: $masterPng"

Write-Host "2. Generating crisp small dormant logo (48x48)..."
$small = [DormantAssetGenerator]::GenerateCrispIcon($master, 48)
$smallPng = Join-Path $assetsDir "images\logo_dormant_small.png"
$small.Save($smallPng, [System.Drawing.Imaging.ImageFormat]::Png)
$small.Dispose()
Write-Host "   Saved: $smallPng"

Write-Host "3. Generating app_dormant.ico (Multi-res 16 to 256px)..."
$icoSizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$icoBitmaps = New-Object 'System.Collections.Generic.List[System.Drawing.Bitmap]'
foreach ($sz in $icoSizes) {
    $icoBitmaps.Add([DormantAssetGenerator]::GenerateCrispIcon($master, $sz))
}
$appDormantIcoPath = Join-Path $assetsDir "icons\app_dormant.ico"
[DormantAssetGenerator]::SaveIco($icoBitmaps, $appDormantIcoPath)
Write-Host "   Saved: $appDormantIcoPath"

Write-Host "4. Generating app_dormant_small.ico (16 to 48px)..."
$smallIcoSizes = @(16, 20, 24, 32, 48)
$smallIcoBitmaps = New-Object 'System.Collections.Generic.List[System.Drawing.Bitmap]'
foreach ($sz in $smallIcoSizes) {
    $found = $null
    foreach ($b in $icoBitmaps) { if ($b.Width -eq $sz) { $found = $b; break } }
    if ($found) { $smallIcoBitmaps.Add($found) }
}
$appDormantSmallIcoPath = Join-Path $assetsDir "icons\app_dormant_small.ico"
[DormantAssetGenerator]::SaveIco($smallIcoBitmaps, $appDormantSmallIcoPath)
Write-Host "   Saved: $appDormantSmallIcoPath"

foreach ($b in $icoBitmaps) { $b.Dispose() }
$master.Dispose()

Write-Host "SUCCESS: Dormant assets generated successfully!"
