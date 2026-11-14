# Generates neutral demo media for the README screenshots (no third-party content):
#   .toolchain\tmp\demo\video\*.avi   animated colour art (uncompressed AVI, plays anywhere mpv does)
#   .toolchain\tmp\demo\music\...     tagged WAV tones, 2 artists x 2 albums x 3 songs
#   .\scripts\make-demo-media.ps1
param([string]$Out = "")
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
if (-not $Out) { $Out = Join-Path $Root ".toolchain\tmp\demo" }
if (Test-Path "$Out\.done") { Write-Host "demo media already there: $Out"; return }
New-Item -ItemType Directory -Force "$Out\video", "$Out\music" | Out-Null

Add-Type -TypeDefinition @'
using System; using System.IO; using System.Text;
public static class DemoGen {
    static void Tag(BinaryWriter w, string id) { w.Write(Encoding.ASCII.GetBytes(id)); }
    // Uncompressed 24-bit AVI: always decodable, no codec needed.
    public static void Avi(string path, int W, int H, int frames, int fps, int hueA, int hueB, int variant) {
        int frameBytes = W * H * 3; int pad = (4 - (W * 3) % 4) % 4; frameBytes = (W * 3 + pad) * H;
        using (var fs = new FileStream(path, FileMode.Create)) using (var w = new BinaryWriter(fs)) {
            Tag(w, "RIFF"); w.Write(0); Tag(w, "AVI ");
            Tag(w, "LIST"); w.Write(4 + 8 + 56 + 12 + 8 + 56 + 8 + 40 + 4); Tag(w, "hdrl");
            Tag(w, "avih"); w.Write(56); w.Write(1000000 / fps); w.Write(0); w.Write(0); w.Write(0x10); w.Write(frames); w.Write(0); w.Write(1); w.Write(frameBytes); w.Write(W); w.Write(H); w.Write(0); w.Write(0); w.Write(0); w.Write(0);
            Tag(w, "LIST"); w.Write(4 + 8 + 56 + 8 + 40); Tag(w, "strl");
            Tag(w, "strh"); w.Write(56); Tag(w, "vids"); Tag(w, "DIB "); w.Write(0); w.Write((short)0); w.Write((short)0); w.Write(0); w.Write(1); w.Write(fps); w.Write(0); w.Write(frames); w.Write(frameBytes); w.Write(-1); w.Write(0); w.Write((short)0); w.Write((short)0); w.Write((short)W); w.Write((short)H);
            Tag(w, "strf"); w.Write(40); w.Write(40); w.Write(W); w.Write(H); w.Write((short)1); w.Write((short)24); w.Write(0); w.Write(frameBytes); w.Write(0); w.Write(0); w.Write(0); w.Write(0);
            long listPos = fs.Position; Tag(w, "LIST"); w.Write(0); Tag(w, "movi");
            var idx = new long[frames]; var row = new byte[W * 3 + pad]; var rnd = new Random(variant * 7919);
            for (int f = 0; f < frames; f++) {
                idx[f] = fs.Position - (listPos + 8); Tag(w, "00db"); w.Write(frameBytes);
                double t = (double)f / frames; double cx = W * (0.25 + 0.5 * t), cy = H * (0.45 + 0.1 * Math.Sin(t * 6.28 + variant));
                for (int y = H - 1; y >= 0; y--) {                         // bottom-up DIB
                    for (int x = 0; x < W; x++) {
                        double u = (double)x / W, v = (double)y / H;
                        double h = hueA + (hueB - hueA) * (0.6 * v + 0.4 * u + 0.15 * Math.Sin(u * 6 + t * 6.28 + variant));
                        double dx = x - cx, dy = (y - cy) * 1.0; double d = Math.Sqrt(dx * dx + dy * dy); double glow = Math.Exp(-d * d / (2.0 * (H * 0.22) * (H * 0.22)));
                        double r, g, b; Hsv(h, 0.55 - 0.25 * glow, 0.30 + 0.55 * glow + 0.25 * (1 - v), out r, out g, out b);
                        if (variant % 2 == 1) { double ridge = 0.5 + 0.5 * Math.Sin(u * 14 + v * 5 + t * 6.28); r *= 0.85 + 0.15 * ridge; g *= 0.85 + 0.15 * ridge; b *= 0.85 + 0.15 * ridge; }
                        row[x * 3] = (byte)(b * 255); row[x * 3 + 1] = (byte)(g * 255); row[x * 3 + 2] = (byte)(r * 255);
                    }
                    w.Write(row);
                }
            }
            long moviEnd = fs.Position; fs.Position = listPos + 4; w.Write((int)(moviEnd - listPos - 8)); fs.Position = moviEnd;
            Tag(w, "idx1"); w.Write(16 * frames);
            for (int f = 0; f < frames; f++) { Tag(w, "00db"); w.Write(0x10); w.Write((int)idx[f] + 4); w.Write(frameBytes); }
            long end = fs.Position; fs.Position = 4; w.Write((int)(end - 8));
        }
    }
    static void Hsv(double h, double s, double v, out double r, out double g, out double b) {
        h = ((h % 360) + 360) % 360; double c = v * s, x = c * (1 - Math.Abs((h / 60) % 2 - 1)), m = v - c; double rr, gg, bb;
        if (h < 60) { rr = c; gg = x; bb = 0; } else if (h < 120) { rr = x; gg = c; bb = 0; } else if (h < 180) { rr = 0; gg = c; bb = x; }
        else if (h < 240) { rr = 0; gg = x; bb = c; } else if (h < 300) { rr = x; gg = 0; bb = c; } else { rr = c; gg = 0; bb = x; }
        r = rr + m; g = gg + m; b = bb + m;
    }
    static void Info(BinaryWriter w, string id, string val) { var b = Encoding.UTF8.GetBytes(val + "\0"); Tag(w, id); w.Write(b.Length); w.Write(b); if (b.Length % 2 == 1) w.Write((byte)0); }
    // Tagged 8 kHz mono WAV (INAM / IART / IPRD / ITRK / ICRD in a LIST INFO chunk, which mpv/ffmpeg read as tags).
    public static void Wav(string path, string title, string artist, string album, int track, int year, double freq, int seconds) {
        int rate = 8000, n = rate * seconds;
        var info = new MemoryStream(); using (var iw = new BinaryWriter(info)) { Tag(iw, "INFO"); Info(iw, "INAM", title); Info(iw, "IART", artist); Info(iw, "IPRD", album); Info(iw, "ITRK", track.ToString()); Info(iw, "ICRD", year.ToString()); }
        var ib = info.ToArray();
        using (var w = new BinaryWriter(new FileStream(path, FileMode.Create))) {
            Tag(w, "RIFF"); w.Write(4 + 24 + 8 + n * 2 + 8 + ib.Length); Tag(w, "WAVE");
            Tag(w, "fmt "); w.Write(16); w.Write((short)1); w.Write((short)1); w.Write(rate); w.Write(rate * 2); w.Write((short)2); w.Write((short)16);
            Tag(w, "data"); w.Write(n * 2);
            for (int i = 0; i < n; i++) { double env = Math.Min(1.0, Math.Min(i / 800.0, (n - i) / 800.0)); w.Write((short)(env * 6000 * Math.Sin(2 * Math.PI * freq * i / rate))); }
            Tag(w, "LIST"); w.Write(ib.Length); w.Write(ib);
        }
    }
}
'@

$clips = @(
    @{n='Aurora';      a=150; b=280; v=0}, @{n='Desert Dunes'; a=15;  b=55;  v=1},
    @{n='Neon City';   a=290; b=200; v=2}, @{n='Tide';          a=180; b=230; v=3}
)
foreach ($c in $clips) { [DemoGen]::Avi("$Out\video\$($c.n).avi", 854, 480, 24, 12, $c.a, $c.b, $c.v); Write-Host "video: $($c.n)" }

$albums = @(
    @{artist='Mira Lowe';   album='Night Drive';  year=2023; songs=@('Headlights','Overpass','Last Exit');      f=220},
    @{artist='Mira Lowe';   album='Soft Light';   year=2021; songs=@('Morning Room','Paper Boats','Linen');     f=262},
    @{artist='The Quiet Hours'; album='Long Way Home'; year=2022; songs=@('Platform 9','Porch Lights','Meridian'); f=330},
    @{artist='The Quiet Hours'; album='Small Rooms';   year=2019; songs=@('Radiator','Kettle','Tuesday');          f=392}
)
Add-Type -AssemblyName System.Drawing
foreach ($al in $albums) {
    $dir = "$Out\music\$($al.artist)\$($al.album)"; New-Item -ItemType Directory -Force $dir | Out-Null
    for ($i = 0; $i -lt $al.songs.Count; $i++) {
        $t = $al.songs[$i]
        [DemoGen]::Wav("$dir\0$($i+1) - $t.wav", $t, $al.artist, $al.album, $i + 1, $al.year, $al.f * [math]::Pow(2, $i / 12.0), 4)
    }
    # a cover picture next to the songs (folder.jpg), so the Music tab has album art to show
    $bmp = New-Object System.Drawing.Bitmap 300, 300
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $c1 = [System.Drawing.Color]::FromArgb(255, [int](60 + ($al.f % 120)), [int](90 + ($al.year % 100)), 170)
    $c2 = [System.Drawing.Color]::FromArgb(255, 235, [int](150 + ($al.f % 70)), 90)
    $br = New-Object System.Drawing.Drawing2D.LinearGradientBrush ([System.Drawing.Point]::new(0, 0)), ([System.Drawing.Point]::new(300, 300)), $c1, $c2
    $g.FillRectangle($br, 0, 0, 300, 300)
    $g.Dispose(); $br.Dispose()
    $bmp.Save("$dir\folder.jpg", [System.Drawing.Imaging.ImageFormat]::Jpeg); $bmp.Dispose()
}
"ok" | Set-Content "$Out\.done"
Write-Host "demo media ready: $Out"
