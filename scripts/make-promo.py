# Builds the 30-second promo: assets/readme/promo.mp4 (+ promo-poster.png) from the current screenshots.
#   python scripts\make-promo.py        (needs Pillow and ffmpeg on PATH; run after scripts\refresh-visuals.ps1)
# The frames are the app's own off-screen screenshots, so nothing is recorded from a real screen and no third-party media is used.
import os, subprocess, sys, tempfile
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = os.path.join(ROOT, "assets", "screenshots")
OUT = os.path.join(ROOT, "assets", "readme")
FONTS = os.path.join(ROOT, "resources", "fonts")
W, H = 1920, 1080
FPS = 30
XFADE = 0.5

# (screenshot, caption, seconds)
SCENES = [
    ("setup-choose-dark", "Download, run the setup. No administrator rights needed.", 3.2),
    ("setup-installing-dark", "It installs for you only, on the drive you pick.", 2.8),
    ("setup-done-dark", "Ready in seconds. Set it as your default player if you like.", 2.8),
    ("home-dark", "Add a folder, click a file, it plays. Click as fast as you like.", 3.2),
    ("hero-dark", "After two seconds the controls fade and the picture gets the whole window.", 3.4),
    ("library-marks-dark", "Keep, reject, next unseen: skim a whole folder in minutes.", 3.0),
    ("palette-dark", "Ctrl+K finds any file or command.", 2.6),
    ("settings-privacy-light", "Private mode and Clean identity wipe your history in one click.", 3.2),
    ("setup-doctor-dark", "The Doctor checks the install and repairs what it can.", 3.2),
]
END_CARD_SECONDS = 2.2

def font(weight, size):
    return ImageFont.truetype(os.path.join(FONTS, f"AtkinsonHyperlegibleNext-{weight}.ttf"), size)

def background():
    from PIL import ImageChops
    top, bottom = Image.new("RGB", (W, H), (16, 24, 24)), Image.new("RGB", (W, H), (8, 9, 8))
    bg = Image.composite(bottom, top, Image.linear_gradient("L").resize((W, H)))
    glow = Image.new("RGB", (W, H), (0, 0, 0))
    ImageDraw.Draw(glow).ellipse((W * 0.25, -H * 0.5, W * 0.95, H * 0.5), fill=(26, 92, 94))
    return ImageChops.add(bg, glow.filter(ImageFilter.GaussianBlur(220)))

def rounded(img, radius):
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, img.size[0] - 1, img.size[1] - 1), radius, fill=255)
    out = img.convert("RGBA"); out.putalpha(mask)
    return out

def scene_frame(bg, shot_name, caption):
    canvas = bg.copy().convert("RGBA")
    shot = Image.open(os.path.join(SHOTS, shot_name + ".png")).convert("RGB")
    target_h = 850
    target_w = int(shot.width * target_h / shot.height)
    shot = shot.resize((target_w, target_h), Image.LANCZOS)
    card = rounded(shot, 22)
    x, y = (W - target_w) // 2, 54
    shadow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle((x - 6, y + 14, x + target_w + 6, y + target_h + 26), 28, fill=(0, 0, 0, 190))
    canvas = Image.alpha_composite(canvas, shadow.filter(ImageFilter.GaussianBlur(26)))
    canvas.alpha_composite(card, (x, y))
    d = ImageDraw.Draw(canvas)
    d.rounded_rectangle((x, y, x + target_w - 1, y + target_h - 1), 22, outline=(255, 255, 255, 46), width=2)
    f = font("Bold", 44)
    tw = d.textlength(caption, font=f)
    while tw > W - 160 and f.size > 28:
        f = font("Bold", f.size - 2); tw = d.textlength(caption, font=f)
    d.text(((W - tw) / 2, 960), caption, font=f, fill=(246, 246, 240))
    return canvas.convert("RGB")

def end_card(bg):
    canvas = bg.copy()
    d = ImageDraw.Draw(canvas)
    t = font("Bold", 200); tw = d.textlength("kinoz", font=t)
    d.text(((W - tw) / 2, 330), "kinoz", font=t, fill=(246, 246, 240))
    s = font("Bold", 54); line = "Free. Offline. Private."
    d.text(((W - d.textlength(line, font=s)) / 2, 590), line, font=s, fill=(79, 209, 214))
    p = font("Regular", 40); line2 = "Windows  ·  macOS  ·  Linux      github.com/p4inz-code/kinoz"
    d.text(((W - d.textlength(line2, font=p)) / 2, 700), line2, font=p, fill=(176, 190, 188))
    return canvas

def poster(frame):
    img = frame.copy().convert("RGBA")
    ov = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    cx, cy, r = W // 2, 470, 110
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(10, 14, 14, 200), outline=(255, 255, 255, 230), width=6)
    d.polygon([(cx - 36, cy - 56), (cx - 36, cy + 56), (cx + 62, cy)], fill=(255, 255, 255, 240))
    return Image.alpha_composite(img, ov).convert("RGB")

def main():
    for name, _, _ in SCENES:
        if not os.path.exists(os.path.join(SHOTS, name + ".png")):
            sys.exit(f"missing screenshot: {name}.png (run scripts\\refresh-visuals.ps1 first)")
    bg = background()
    tmp = tempfile.mkdtemp(prefix="kinoz-promo-")
    paths, durs = [], []
    first = None
    for i, (name, cap, sec) in enumerate(SCENES):
        fr = scene_frame(bg, name, cap)
        if first is None: first = fr
        p = os.path.join(tmp, f"s{i:02d}.png"); fr.save(p); paths.append(p); durs.append(sec)
    p = os.path.join(tmp, "end.png"); end_card(bg).save(p); paths.append(p); durs.append(END_CARD_SECONDS)
    # the poster uses the playing-video scene
    poster(scene_frame(bg, "hero-dark", "Watch the 30 second tour")).save(os.path.join(OUT, "promo-poster.png"), optimize=True)

    cmd = ["ffmpeg", "-y", "-loglevel", "error"]
    for p, s in zip(paths, durs):
        cmd += ["-loop", "1", "-framerate", str(FPS), "-t", f"{s + XFADE:.2f}", "-i", p]
    parts, last, t = [], "[0:v]", durs[0]
    for i in range(len(paths)):                                   # a slow push-in on every scene
        parts.append(f"[{i}:v]scale=2400:-1,zoompan=z='1+0.00045*on':x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)':d=1:s={W}x{H}:fps={FPS},format=yuv420p[v{i}]")
    last = "[v0]"
    for i in range(1, len(paths)):
        out = f"[x{i}]"
        parts.append(f"{last}[v{i}]xfade=transition=fade:duration={XFADE}:offset={t:.2f}{out}")
        last = out; t += durs[i]
    cmd += ["-filter_complex", ";".join(parts), "-map", last, "-c:v", "libx264", "-preset", "medium", "-crf", "24", "-pix_fmt", "yuv420p", "-movflags", "+faststart", "-r", str(FPS),
            os.path.join(OUT, "promo.mp4")]
    print("encoding", sum(durs), "seconds ...")
    subprocess.run(cmd, check=True)
    size = os.path.getsize(os.path.join(OUT, "promo.mp4")) / 1e6
    print(f"promo.mp4 written ({size:.1f} MB), poster written")

if __name__ == "__main__":
    main()
