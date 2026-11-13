import os
from PIL import Image, ImageDraw, ImageFilter

def create_simple_icon():
    # Render at 1024x1024 for supersampled sharpness
    W, H = 1024, 1024
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Margins and squircle geometry
    margin = 80
    r = 200
    box = [margin, margin, W - margin, H - margin]

    # 1. Subtle drop shadow for the squircle
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    s_draw = ImageDraw.Draw(shadow)
    s_box = [margin + 10, margin + 25, W - margin + 10, H - margin + 25]
    s_draw.rounded_rectangle(s_box, radius=r, fill=(0, 0, 0, 90))
    shadow = shadow.filter(ImageFilter.GaussianBlur(28))
    img.paste(shadow, (0, 0), shadow)

    # 2. Base Squircle with vibrant modern blue gradient
    base_mask = Image.new("L", (W, H), 0)
    bm_draw = ImageDraw.Draw(base_mask)
    bm_draw.rounded_rectangle(box, radius=r, fill=255)

    base = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    # Gradient from bright cobalt blue (#2684FF) to deep rich blue (#004EB5)
    for y in range(box[1], box[3]):
        t = (y - box[1]) / (box[3] - box[1])
        # Smooth interpolation
        red = int(38 * (1 - t) + 0 * t)
        green = int(132 * (1 - t) + 78 * t)
        blue = int(255 * (1 - t) + 181 * t)
        ImageDraw.Draw(base).line([(box[0], y), (box[2], y)], fill=(red, green, blue, 255))

    # Apply rounded mask
    img.paste(base, (0, 0), base_mask)

    # 3. Inner border highlight (1px subtle glow on top)
    highlight = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    h_draw = ImageDraw.Draw(highlight)
    h_draw.rounded_rectangle(box, radius=r, outline=(255, 255, 255, 70), width=6)
    img.alpha_composite(highlight)

    # 4. Minimalist Zipper Motif (Clean, bold, pure white #FFFFFF)
    # Center X = 512
    cx = 512
    teeth_color = (255, 255, 255, 240)
    accent_color = (255, 255, 255, 255)

    # Alternating zipper teeth below the slider (Y: 530 to 820)
    tooth_w = 70
    tooth_h = 24
    tooth_gap = 40
    y = 550
    left_side = True
    while y <= 810:
        if left_side:
            # Left tooth: from cx - tooth_w to cx + 10
            t_box = [cx - tooth_w, y, cx + 15, y + tooth_h]
        else:
            # Right tooth: from cx - 15 to cx + tooth_w
            t_box = [cx - 15, y, cx + tooth_w, y + tooth_h]
        
        draw.rounded_rectangle(t_box, radius=8, fill=teeth_color)
        left_side = not left_side
        y += tooth_gap

    # Top V-Opening teeth (Y: 200 to 330)
    # The zipper splits open at the top
    top_y = 200
    spread = 150
    while top_y < 330:
        t = (330 - top_y) / 130.0
        # Left tooth angled outwards
        lx = cx - 20 - int(t * spread)
        draw.rounded_rectangle([lx - tooth_w, top_y, lx + 10, top_y + tooth_h], radius=8, fill=teeth_color)
        
        # Right tooth angled outwards
        rx = cx + 20 + int(t * spread)
        draw.rounded_rectangle([rx - 10, top_y, rx + tooth_w, top_y + tooth_h], radius=8, fill=teeth_color)
        
        top_y += tooth_gap

    # 5. Central Zipper Slider & Pull Tab (The focal hero element)
    # Slider Body: Rounded trapezoid / rectangle at y = 330 to 470
    slider_w = 170
    slider_h = 130
    slider_y = 330
    slider_box = [cx - slider_w // 2, slider_y, cx + slider_w // 2, slider_y + slider_h]
    
    # Slider subtle shadow
    slider_shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ss_draw = ImageDraw.Draw(slider_shadow)
    ss_box = [slider_box[0] - 5, slider_box[1] + 8, slider_box[2] + 5, slider_box[3] + 16]
    ss_draw.rounded_rectangle(ss_box, radius=24, fill=(0, 20, 60, 110))
    slider_shadow = slider_shadow.filter(ImageFilter.GaussianBlur(10))
    img.alpha_composite(slider_shadow)

    # Slider body fill (Pure Crisp White)
    draw.rounded_rectangle(slider_box, radius=22, fill=(255, 255, 255, 255))
    
    # Slider inner accent groove (subtle blue accent)
    draw.rounded_rectangle([cx - 40, slider_y + 24, cx + 40, slider_y + 44], radius=10, fill=(0, 102, 204, 230))

    # Pull tab connector loop
    draw.rounded_rectangle([cx - 30, slider_y + 90, cx + 30, slider_y + 160], radius=12, fill=(255, 255, 255, 255))

    # Pull Tab (Hanging down)
    tab_w = 110
    tab_h = 210
    tab_y = slider_y + 130
    tab_box = [cx - tab_w // 2, tab_y, cx + tab_w // 2, tab_y + tab_h]

    # Tab shadow
    tab_shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ts_draw = ImageDraw.Draw(tab_shadow)
    ts_box = [tab_box[0] - 4, tab_box[1] + 10, tab_box[2] + 4, tab_box[3] + 18]
    ts_draw.rounded_rectangle(ts_box, radius=26, fill=(0, 20, 60, 120))
    tab_shadow = tab_shadow.filter(ImageFilter.GaussianBlur(12))
    img.alpha_composite(tab_shadow)

    # Tab body
    draw.rounded_rectangle(tab_box, radius=26, fill=(255, 255, 255, 255))

    # Tab inner cutout slot (Modern minimalist pill hole)
    hole_w = 44
    hole_h = 100
    hole_y = tab_y + 60
    draw.rounded_rectangle([cx - hole_w // 2, hole_y, cx + hole_w // 2, hole_y + hole_h], radius=18, fill=(0, 80, 175, 240))

    # 6. Generate Multi-resolution ICO
    sizes = [256, 128, 64, 48, 32, 24, 16]
    frames = []
    for s in sizes:
        # High quality Lanczos resampling
        resized = img.resize((s, s), Image.Resampling.LANCZOS)
        frames.append(resized)

    out_ico = "res/EasyZip.ico"
    frames[0].save(
        out_ico,
        format="ICO",
        sizes=[(s, s) for s in sizes],
        append_images=frames[1:]
    )
    
    # Also save as 256x256 PNG for preview
    frames[0].save("res/EasyZip_256.png", format="PNG")
    print(f"Generated {out_ico} with sizes: {sizes}")

if __name__ == "__main__":
    create_simple_icon()

