import os
from PIL import Image

def generate_icons(png_path, icon_dir):
    if not os.path.exists(png_path):
        print(f"Error: {png_path} not found.")
        return
        
    if not os.path.exists(icon_dir):
        os.makedirs(icon_dir)
        
    img = Image.open(png_path).convert('RGBA')
    
    # Standard Windows square icon dimensions
    icon_sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
    frames = []
    for s in icon_sizes:
        w, h = img.size
        scale = min(s / w, s / h)
        nw = max(1, int(round(w * scale)))
        nh = max(1, int(round(h * scale)))
        resized = img.resize((nw, nh), Image.Resampling.LANCZOS)
        canvas = Image.new('RGBA', (s, s), (0, 0, 0, 0))
        canvas.paste(resized, ((s - nw) // 2, (s - nh) // 2), resized)
        frames.append(canvas)
    
    # Generate multi-resolution ICO for the main app
    app_ico_path = os.path.join(icon_dir, 'app.ico')
    frames[-1].save(app_ico_path, format='ICO', append_images=frames[:-1])
    print(f"Saved: {app_ico_path}")
    
    # Generate small icon (16x16, 24x24, 32x32, 48x48)
    small_sizes = [16, 24, 32, 48]
    small_frames = [f for s, f in zip(icon_sizes, frames) if s in small_sizes]
    small_ico_path = os.path.join(icon_dir, 'app_small.ico')
    small_frames[-1].save(small_ico_path, format='ICO', append_images=small_frames[:-1])
    print(f"Saved: {small_ico_path}")

if __name__ == '__main__':
    generate_icons('assets/images/logo.png', 'assets/icons')
