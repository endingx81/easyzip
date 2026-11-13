import math
from PIL import Image, ImageDraw, ImageFilter, ImageEnhance

def make_precision_silver_icon(out_png="assets/EasyZip_silver.png", out_ico="assets/EasyZip_silver.ico"):
    S = 1024  # render resolution
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    
    # Coordinates for squircle container
    pad = 90
    x0, y0, x1, y1 = pad, pad, S - pad, S - pad
    r = 210  # corner radius for modern squircle
    
    # 1. Base Drop Shadow
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    s_draw = ImageDraw.Draw(shadow)
    s_draw.rounded_rectangle([x0, y0 + 35, x1, y1 + 35], radius=r, fill=(15, 23, 42, 110))
    shadow = shadow.filter(ImageFilter.GaussianBlur(32))
    img.alpha_composite(shadow)
    
    # 2. Outer Bezel / Border (Polished Chrome Rim)
    bezel = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    b_draw = ImageDraw.Draw(bezel)
    b_draw.rounded_rectangle([x0, y0, x1, y1], radius=r, fill=(255, 255, 255, 255))
    
    # Gradient on bezel: bright chrome reflection top-left to dark steel bottom-right
    for y in range(y0, y1):
        t = (y - y0) / (y1 - y0)
        # Chrome reflection curves
        factor = 0.5 + 0.5 * math.cos(t * math.pi)
        r_c = int(240 * factor + 120 * (1 - factor))
        g_c = int(245 * factor + 130 * (1 - factor))
        b_c = int(255 * factor + 145 * (1 - factor))
        b_draw.line([(x0, y), (x1, y)], fill=(r_c, g_c, b_c, 255))
    
    # Mask bezel into rounded rect
    mask_outer = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask_outer).rounded_rectangle([x0, y0, x1, y1], radius=r, fill=255)
    bezel.putalpha(mask_outer)
    img.alpha_composite(bezel)
    
    # 3. Inner Body (Brushed Platinum / Silver Metal Surface)
    inner_pad = pad + 14
    ix0, iy0, ix1, iy1 = inner_pad, inner_pad, S - inner_pad, S - inner_pad
    ir = r - 12
    
    inner_body = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    i_draw = ImageDraw.Draw(inner_body)
    
    for y in range(iy0, iy1):
        t = (y - iy0) / (iy1 - iy0)
        # Luxury platinum silver gradient with sheen
        # Top is luminous silver-white, middle has metallic sheen, bottom is cool titanium
        val = 245 - 90 * t + 25 * math.sin(t * math.pi * 2.5)
        val = max(130, min(255, int(val)))
        # Subtle cool blue-silver tint
        r_col = int(val * 0.96)
        g_col = int(val * 0.98)
        b_col = int(val * 1.02)
        b_col = min(255, b_col)
        i_draw.line([(ix0, y), (ix1, y)], fill=(r_col, g_col, b_col, 255))
        
    mask_inner = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask_inner).rounded_rectangle([ix0, iy0, ix1, iy1], radius=ir, fill=255)
    inner_body.putalpha(mask_inner)
    
    # Inner top highlight gloss
    gloss = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    g_draw = ImageDraw.Draw(gloss)
    g_draw.rounded_rectangle([ix0 + 4, iy0 + 4, ix1 - 4, iy0 + 260], radius=ir-4, fill=(255, 255, 255, 120))
    gloss = gloss.filter(ImageFilter.GaussianBlur(15))
    # Clip gloss to inner body
    gloss.putalpha(ImageEnhance.Brightness(mask_inner).enhance(0.5))
    
    img.alpha_composite(inner_body)
    img.alpha_composite(gloss)
    
    # 4. Zipper Track (Metallic Titanium Dark Trench down the center)
    cx = S // 2
    track_w = 72
    tx0 = cx - track_w // 2
    tx1 = cx + track_w // 2
    ty0 = iy0 + 20
    ty1 = iy1 - 20
    
    track = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    t_draw = ImageDraw.Draw(track)
    # Trench shadow
    t_draw.rounded_rectangle([tx0, ty0, tx1, ty1], radius=16, fill=(30, 41, 59, 230))
    track = track.filter(ImageFilter.GaussianBlur(4))
    img.alpha_composite(track)
    
    # 5. Interlocking Polished Chrome Zipper Teeth
    teeth = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    teeth_draw = ImageDraw.Draw(teeth)
    
    tooth_h = 24
    tooth_gap = 14
    step = tooth_h + tooth_gap
    tooth_w = 42
    
    tooth_y = ty0 + 30
    side = 0
    while tooth_y < ty1 - 40:
        if side == 0:  # Left tooth
            lx0 = cx - tooth_w + 6
            lx1 = cx + 8
            # tooth body
            teeth_draw.rounded_rectangle([lx0, tooth_y, lx1, tooth_y + tooth_h], radius=6, fill=(240, 245, 255, 255))
            teeth_draw.line([(lx0, tooth_y), (lx1, tooth_y)], fill=(255, 255, 255, 255), width=2)
            teeth_draw.line([(lx0, tooth_y + tooth_h), (lx1, tooth_y + tooth_h)], fill=(140, 155, 175, 255), width=2)
            side = 1
        else:  # Right tooth
            rx0 = cx - 8
            rx1 = cx + tooth_w - 6
            teeth_draw.rounded_rectangle([rx0, tooth_y, rx1, tooth_y + tooth_h], radius=6, fill=(225, 235, 248, 255))
            teeth_draw.line([(rx0, tooth_y), (rx1, tooth_y)], fill=(255, 255, 255, 255), width=2)
            teeth_draw.line([(rx0, tooth_y + tooth_h), (rx1, tooth_y + tooth_h)], fill=(130, 145, 165, 255), width=2)
            side = 0
        tooth_y += step
        
    img.alpha_composite(teeth)
    
    # 6. Zipper Slider & Pull-Tab (Sleek Chrome & Metallic Mirror Finish)
    # Slider Body
    slider_y = S // 2 - 40
    slider_w = 110
    slider_h = 100
    sx0 = cx - slider_w // 2
    sx1 = cx + slider_w // 2
    sy0 = slider_y - slider_h // 2
    sy1 = slider_y + slider_h // 2
    
    # Slider Shadow
    sl_shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(sl_shadow).rounded_rectangle([sx0, sy0 + 10, sx1, sy1 + 10], radius=24, fill=(15, 23, 42, 140))
    sl_shadow = sl_shadow.filter(ImageFilter.GaussianBlur(16))
    img.alpha_composite(sl_shadow)
    
    # Slider Chrome
    slider = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sl_draw = ImageDraw.Draw(slider)
    sl_draw.rounded_rectangle([sx0, sy0, sx1, sy1], radius=24, fill=(245, 250, 255, 255))
    
    # Metallic gradient across slider
    for y in range(sy0, sy1):
        t = (y - sy0) / (sy1 - sy0)
        c_val = int(255 * (1 - 0.45 * t) + 20 * math.sin(t * math.pi * 3))
        c_val = max(140, min(255, c_val))
        sl_draw.line([(sx0, y), (sx1, y)], fill=(c_val, c_val, int(min(255, c_val * 1.05)), 255))
        
    sl_mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(sl_mask).rounded_rectangle([sx0, sy0, sx1, sy1], radius=24, fill=255)
    slider.putalpha(sl_mask)
    img.alpha_composite(slider)
    
    # Slider Rim/Bevel
    slider_rim = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(slider_rim).rounded_rectangle([sx0, sy0, sx1, sy1], radius=24, outline=(255, 255, 255, 240), width=4)
    img.alpha_composite(slider_rim)
    
    # 7. Pull-Tab hanging from slider
    tab_w = 90
    tab_h = 240
    tx0 = cx - tab_w // 2
    tx1 = cx + tab_w // 2
    ty0 = sy1 - 20
    ty1 = ty0 + tab_h
    
    # Pull-tab shadow
    tab_shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(tab_shadow).rounded_rectangle([tx0, ty0 + 15, tx1, ty1 + 15], radius=28, fill=(15, 23, 42, 160))
    tab_shadow = tab_shadow.filter(ImageFilter.GaussianBlur(18))
    img.alpha_composite(tab_shadow)
    
    # Pull-tab body
    tab = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    tab_draw = ImageDraw.Draw(tab)
    
    for y in range(ty0, ty1):
        t = (y - ty0) / (ty1 - ty0)
        # Chrome sheen on pull tab
        val = int(250 - 70 * t + 30 * math.sin(t * math.pi * 2))
        val = max(130, min(255, val))
        tab_draw.line([(tx0, y), (tx1, y)], fill=(val, int(val * 1.01), int(min(255, val * 1.06)), 255))
        
    tab_mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(tab_mask).rounded_rectangle([tx0, ty0, tx1, ty1], radius=28, fill=255)
    tab.putalpha(tab_mask)
    img.alpha_composite(tab)
    
    # Tab Rim
    tab_rim = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(tab_rim).rounded_rectangle([tx0, ty0, tx1, ty1], radius=28, outline=(255, 255, 255, 250), width=5)
    img.alpha_composite(tab_rim)
    
    # 8. Pull-Tab Ring & 'Z' (EasyZip Emblem)
    # Ring hole at top of tab
    hole_r = 16
    hole_y = ty0 + 40
    hole_draw = ImageDraw.Draw(img)
    hole_draw.ellipse([cx - hole_r, hole_y - hole_r, cx + hole_r, hole_y + hole_r], fill=(30, 41, 59, 230), outline=(255, 255, 255, 180), width=3)
    
    # Modern Stylized 'Z' or Lightning on the pull tab
    # Coordinates for crisp bold Z
    zy0 = ty0 + 95
    zy1 = ty1 - 40
    zw = 46
    zx0 = cx - zw // 2
    zx1 = cx + zw // 2
    zh = zy1 - zy0
    
    # Metallic Silver / Cyan-Steel Engraved Z
    z_points = [
        (zx0, zy0), (zx1, zy0), (zx1, zy0 + 16),
        (zx0 + 18, zy1 - 16), (zx1, zy1 - 16), (zx1, zy1),
        (zx0, zy1), (zx0, zy1 - 16), (zx1 - 18, zy0 + 16),
        (zx0, zy0 + 16)
    ]
    # Engraved inner shadow for Z
    hole_draw.polygon(z_points, fill=(51, 65, 85, 240))
    # Bevel on Z
    hole_draw.line([(zx0, zy0), (zx1, zy0)], fill=(100, 116, 139, 255), width=2)
    hole_draw.line([(zx0, zy1), (zx1, zy1)], fill=(255, 255, 255, 220), width=2)
    
    # Downsample using high quality Lanczos filter to 256x256
    img_256 = img.resize((256, 256), Image.Resampling.LANCZOS)
    img_256.save(out_png, "PNG")
    
    # Save multi-size ICO
    sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    icon_images = [img.resize(sz, Image.Resampling.LANCZOS) for sz in sizes]
    icon_images[0].save(out_ico, format="ICO", sizes=[(im.width, im.height) for im in icon_images], append_images=icon_images[1:])
    print(f"Generated {out_png} and {out_ico} successfully!")

if __name__ == "__main__":
    make_precision_silver_icon()

