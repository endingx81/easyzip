from PIL import Image, ImageOps, ImageFilter
from collections import deque

def flood_fill_background(
    input_path=r"C:\Users\itsmyzone\.gemini\antigravity\brain\df0df82e-cb6a-4446-ad3a-df7cc16fd416\easyzip_metallic_silver_icon_1794303029663.jpg",
    out_png="assets/ai_silver.png",
    out_ico="assets/ai_silver.ico"
):
    im = Image.open(input_path).convert("RGB")
    w, h = im.size
    pixels = im.load()
    
    # We will do a breadth-first search flood fill from all 4 corners/edges
    visited = bytearray(w * h)
    queue = deque()
    
    # Seed queue with perimeter pixels
    for x in range(w):
        queue.append((x, 0))
        queue.append((x, h - 1))
        visited[0 * w + x] = 1
        visited[(h - 1) * w + x] = 1
        
    for y in range(h):
        queue.append((0, y))
        queue.append((w - 1, y))
        visited[y * w + 0] = 1
        visited[y * w + (w - 1)] = 1
        
    # Background threshold: near white (> 240 in all channels)
    # or close to average border color
    while queue:
        cx, cy = queue.popleft()
        for dx, dy in ((-1,0), (1,0), (0,-1), (0,1)):
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < w and 0 <= ny < h:
                idx = ny * w + nx
                if not visited[idx]:
                    r, g, b = pixels[nx, ny]
                    # If it's near-white background
                    if r > 240 and g > 240 and b > 240:
                        visited[idx] = 1
                        queue.append((nx, ny))
                    elif r > 230 and g > 230 and b > 230 and (abs(r-g) < 6 and abs(g-b) < 6):
                        # also faint light-gray edge
                        visited[idx] = 1
                        queue.append((nx, ny))
                        
    # Now create alpha channel: 0 where visited, 255 where not visited
    alpha = Image.new("L", (w, h), 255)
    a_pixels = alpha.load()
    for y in range(h):
        for x in range(w):
            if visited[y * w + x]:
                a_pixels[x, y] = 0
                
    # Smooth the alpha mask slightly to remove jagged edge
    alpha = alpha.filter(ImageFilter.GaussianBlur(1.0))
    
    rgba = Image.new("RGBA", (w, h))
    rgba.paste(im, (0, 0))
    rgba.putalpha(alpha)
    
    # Find bounding box
    bbox = rgba.getbbox()
    print("Bounding box:", bbox)
    if bbox:
        bw = bbox[2] - bbox[0]
        bh = bbox[3] - bbox[1]
        cx = (bbox[0] + bbox[2]) // 2
        cy = (bbox[1] + bbox[3]) // 2
        max_dim = int(max(bw, bh) * 1.10)
        sq_box = (
            max(0, cx - max_dim // 2),
            max(0, cy - max_dim // 2),
            min(w, cx + max_dim // 2),
            min(h, cy + max_dim // 2)
        )
        cropped = rgba.crop(sq_box)
    else:
        cropped = rgba
        
    icon_256 = cropped.resize((256, 256), Image.Resampling.LANCZOS)
    icon_256.save(out_png, "PNG")
    
    sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    icon_images = [cropped.resize(sz, Image.Resampling.LANCZOS) for sz in sizes]
    icon_images[0].save(out_ico, format="ICO", sizes=[(i.width, i.height) for i in icon_images], append_images=icon_images[1:])
    print(f"Generated {out_png} and {out_ico} with pristine interior chrome intact!")

if __name__ == "__main__":
    flood_fill_background()
