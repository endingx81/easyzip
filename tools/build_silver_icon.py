import math
from PIL import Image, ImageDraw, ImageFilter, ImageOps

def build_final_silver_icon():
    raw_path = r"C:\Users\itsmyzone\.gemini\antigravity\brain\df0df82e-cb6a-4446-ad3a-df7cc16fd416\easyzip_metallic_silver_icon_1794303029663.jpg"
    raw = Image.open(raw_path).convert("RGB")
    
    # The icon squircle is centered around (512, 508)
    # Let's crop an exact square region of size 880x880 centered at (512, 512)
    crop_size = 920
    cx, cy = 512, 510
    box = (cx - crop_size//2, cy - crop_size//2, cx + crop_size//2, cy + crop_size//2)
    cropped = raw.crop(box)
    
    # We want a clean transparent background around the squircle.
    # The background in the raw crop is near white (#F8 to #FF).
    # Let's create an alpha mask by measuring the distance from pure white on the exterior.
    cw, ch = cropped.size
    pixels = cropped.load()
    
    # BFS flood fill from edges to find pure exterior background
    from collections import deque
    visited = bytearray(cw * ch)
    queue = deque()
    
    for x in range(cw):
        queue.append((x, 0))
        queue.append((x, ch - 1))
        visited[0 * cw + x] = 1
        visited[(ch - 1) * cw + x] = 1
    for y in range(ch):
        queue.append((0, y))
        queue.append((cw - 1, y))
        visited[y * cw + 0] = 1
        visited[y * cw + (cw - 1)] = 1
        
    while queue:
        qx, qy = queue.popleft()
        for dx, dy in ((-1,0), (1,0), (0,-1), (0,1)):
            nx, ny = qx + dx, qy + dy
            if 0 <= nx < cw and 0 <= ny < ch:
                idx = ny * cw + nx
                if not visited[idx]:
                    r, g, b = pixels[nx, ny]
                    # Exterior background is white/near-white or light gray shadow fringe
                    # Background pixels have r,g,b > 238
                    if r >= 236 and g >= 236 and b >= 236:
                        visited[idx] = 1
                        queue.append((nx, ny))
                    elif (r >= 220 and g >= 220 and b >= 220) and abs(r - g) < 4 and abs(g - b) < 4:
                        # soft shadow edge
                        visited[idx] = 1
                        queue.append((nx, ny))

    # Construct smooth alpha mask
    alpha = Image.new("L", (cw, ch), 255)
    a_pix = alpha.load()
    for y in range(ch):
        for x in range(cw):
            if visited[y * cw + x]:
                r, g, b = pixels[x, y]
                # If visited, it's either completely transparent (white) or semi-transparent shadow
                min_c = min(r, g, b)
                if min_c >= 248:
                    a_pix[x, y] = 0
                else:
                    # smooth shadow gradient
                    shadow_density = (248 - min_c) / 30.0
                    a_pix[x, y] = int(min(255, max(0, shadow_density * 90)))
                    
    # Smooth alpha slightly
    alpha = alpha.filter(ImageFilter.GaussianBlur(1.2))
    
    rgba = Image.new("RGBA", (cw, ch))
    rgba.paste(cropped, (0, 0))
    rgba.putalpha(alpha)
    
    # Resize down to 256x256 master with Lanczos
    master_256 = rgba.resize((256, 256), Image.Resampling.LANCZOS)
    
    # Save PNGs
    master_256.save("assets/EasyZip.png", "PNG")
    master_256.save("installer/assets/EasyZip.png", "PNG")
    master_256.save("C:/Users/itsmyzone/.gemini/antigravity/brain/df0df82e-cb6a-4446-ad3a-df7cc16fd416/EasyZip_silver_final.png", "PNG")
    
    # Generate multi-size Windows ICO: 256, 128, 64, 48, 32, 24, 16
    sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    icon_images = [rgba.resize(sz, Image.Resampling.LANCZOS) for sz in sizes]
    
    master_ico_path = "assets/EasyZip.ico"
    installer_ico_path = "installer/assets/EasyZip.ico"
    
    icon_images[0].save(master_ico_path, format="ICO", sizes=[(im.width, im.height) for im in icon_images], append_images=icon_images[1:])
    icon_images[0].save(installer_ico_path, format="ICO", sizes=[(im.width, im.height) for im in icon_images], append_images=icon_images[1:])
    
    print("Master silver icon generated and deployed successfully to assets and installer/assets!")

if __name__ == "__main__":
    build_final_silver_icon()

