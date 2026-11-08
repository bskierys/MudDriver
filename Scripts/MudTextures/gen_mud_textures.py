"""Generate the dirt masks and the spray clump mesh.

usage: gen_mud_textures.py <out_dir> [seed=7]

Masks are "reveal order" fields: histogram-equalized to 0..1, so a material threshold t
covers a fraction t of the area. Splatter is 0 outside the splats.
"""
import sys, os, math
import numpy as np
from PIL import Image

OUT = sys.argv[1]
SEED = int(sys.argv[2]) if len(sys.argv) > 2 else 7

BLOTCH_SIZE = 1024
BLOTCH_CUTOFF = 6.0        # cycles per tile where the spectrum starts falling off
BLOTCH_FALLOFF = 2.4       # spectral exponent (higher = smoother)
SPLAT_SIZE = 1024
SPLAT_COUNT = 260
SPLAT_RADIUS = (3.0, 26.0) # px, skewed to small
SPLAT_DRIP_CHANCE = 0.35
NOISE_SIZE = 512
CLUMP_SEGMENTS, CLUMP_RINGS = 8, 4
CLUMP_LUMP = 0.28          # radial noise amplitude (fraction of radius)


def periodic_noise(size, cutoff, falloff, rng):
    """Tileable noise by shaping white noise in frequency space."""
    f = np.fft.fftfreq(size) * size
    fx, fy = np.meshgrid(f, f)
    r = np.sqrt(fx * fx + fy * fy)
    amp = 1.0 / (1.0 + (r / cutoff) ** falloff)
    amp[0, 0] = 0.0
    spec = np.fft.fft2(rng.standard_normal((size, size))) * amp
    return np.real(np.fft.ifft2(spec))


def equalize(a):
    flat = a.ravel()
    order = np.argsort(flat, kind='stable')
    out = np.empty_like(flat, dtype=np.float64)
    out[order] = np.linspace(0.0, 1.0, flat.size)
    return out.reshape(a.shape)


def save_gray(a, name):
    img = Image.fromarray(np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8), 'L')
    img.save(os.path.join(OUT, name))
    return img


def blotch(rng):
    base = periodic_noise(BLOTCH_SIZE, BLOTCH_CUTOFF, BLOTCH_FALLOFF, rng)
    detail = periodic_noise(BLOTCH_SIZE, 40.0, 2.0, rng)
    return equalize(base + 0.18 * detail / detail.std() * base.std())


def splatter(rng):
    n = SPLAT_SIZE
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float64)
    edge = periodic_noise(n, 60.0, 2.0, rng)
    edge /= np.abs(edge).max()
    out = np.zeros((n, n))
    # arrival in (0,1]: higher = earlier. Bigger splats arrive later on average.
    for i in range(SPLAT_COUNT):
        u = rng.random()
        rad = SPLAT_RADIUS[0] + (SPLAT_RADIUS[1] - SPLAT_RADIUS[0]) * u ** 3
        cx, cy = rng.random() * n, rng.random() * n
        arrival = 1.0 - 0.85 * rng.random() * (0.4 + 0.6 * u)
        sx = 1.0 + 0.5 * rng.random()  # stretch (impact angle)
        ang = rng.random() * math.pi
        dx = (xx - cx + n / 2) % n - n / 2
        dy = (yy - cy + n / 2) % n - n / 2
        ca, sa = math.cos(ang), math.sin(ang)
        ex = (dx * ca + dy * sa) / sx
        ey = -dx * sa + dy * ca
        d = np.sqrt(ex * ex + ey * ey) / rad + 0.35 * edge
        shape = d < 1.0
        if rng.random() < SPLAT_DRIP_CHANCE:
            # drip runs down (+V in image = down on side projections)
            length = rad * (1.5 + 3.0 * rng.random())
            w = rad * (0.18 + 0.15 * rng.random())
            t = np.clip(dy / length, 0.0, 1.0)
            dw = np.abs(dx) / (w * (1.0 - 0.7 * t) + 1e-6) + 0.25 * edge
            shape |= (dy > 0) & (dy < length) & (dw < 1.0)
            shape |= (np.sqrt(dx * dx + (dy - length) ** 2) < w * 0.9)
        out = np.where(shape, np.maximum(out, arrival), out)
    # spread arrival values evenly over the covered pixels; background stays 0
    mask = out > 0
    vals = out[mask]
    order = np.argsort(vals, kind='stable')
    eq = np.empty_like(vals)
    eq[order] = np.linspace(1.0 / 255.0, 1.0, vals.size)
    res = np.zeros_like(out)
    res[mask] = eq
    return res, mask.mean()


def noise(rng):
    lo = periodic_noise(NOISE_SIZE, 8.0, 2.2, rng)
    hi = periodic_noise(NOISE_SIZE, 32.0, 2.2, rng)
    a = lo / lo.std() + 0.4 * hi / hi.std()
    return equalize(a)


def clump_obj(rng, path):
    verts, uvs = [], []
    lump = rng.random((CLUMP_RINGS + 1, CLUMP_SEGMENTS))
    for r in range(CLUMP_RINGS + 1):
        th = math.pi * r / CLUMP_RINGS
        for s in range(CLUMP_SEGMENTS):
            ph = 2 * math.pi * s / CLUMP_SEGMENTS
            k = 1.0 + CLUMP_LUMP * (lump[r, s] - 0.5) * 2.0
            if r in (0, CLUMP_RINGS):
                k = 1.0 + CLUMP_LUMP * (lump[r, 0] - 0.5)
            x = math.sin(th) * math.cos(ph) * k
            y = math.sin(th) * math.sin(ph) * k
            z = math.cos(th) * k * 0.8  # a bit flattened
            verts.append((x, y, z))
            uvs.append((s / CLUMP_SEGMENTS, r / CLUMP_RINGS))
    V = np.array(verts)
    tris = []
    idx = lambda r, s: r * CLUMP_SEGMENTS + (s % CLUMP_SEGMENTS)
    for r in range(CLUMP_RINGS):
        for s in range(CLUMP_SEGMENTS):
            a, b, c, d = idx(r, s), idx(r, s + 1), idx(r + 1, s), idx(r + 1, s + 1)
            if r != 0:
                tris.append((a, c, b))
            if r != CLUMP_RINGS - 1:
                tris.append((b, c, d))
    # pole rings collapse to one point each; weld them so normals are clean
    N = np.zeros_like(V)
    for a, b, c in tris:
        n = np.cross(V[b] - V[a], V[c] - V[a])
        N[a] += n; N[b] += n; N[c] += n
    top = [idx(0, s) for s in range(CLUMP_SEGMENTS)]
    bot = [idx(CLUMP_RINGS, s) for s in range(CLUMP_SEGMENTS)]
    for ring in (top, bot):
        n = N[ring].sum(0)
        N[ring] = n
    N /= np.linalg.norm(N, axis=1, keepdims=True)
    V -= (V.min(0) + V.max(0)) / 2
    with open(path, 'w') as f:
        f.write('# SM_B1MudClump, 1 cm radius\no SM_B1MudClump\n')
        for v in V:
            f.write(f'v {v[0]:.5f} {v[1]:.5f} {v[2]:.5f}\n')
        for uv in uvs:
            f.write(f'vt {uv[0]:.5f} {uv[1]:.5f}\n')
        for n in N:
            f.write(f'vn {n[0]:.5f} {n[1]:.5f} {n[2]:.5f}\n')
        for a, b, c in tris:
            f.write(f'f {a+1}/{a+1}/{a+1} {b+1}/{b+1}/{b+1} {c+1}/{c+1}/{c+1}\n')
    return len(tris)


def contact_sheet(imgs, path):
    tile = 512
    sheet = Image.new('L', (tile * len(imgs) * 2, tile), 0)
    for i, img in enumerate(imgs):
        im = img.resize((tile // 2, tile // 2))
        # 2x2 repeat shows the tiling seams
        rep = Image.new('L', (tile, tile))
        for ox in (0, tile // 2):
            for oy in (0, tile // 2):
                rep.paste(im, (ox, oy))
        sheet.paste(img.resize((tile, tile)), (i * tile * 2, 0))
        sheet.paste(rep, (i * tile * 2 + tile, 0))
    sheet.save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    rng = np.random.default_rng(SEED)
    b = save_gray(blotch(rng), 'T_B1MudBlotch.png')
    s_arr, cover = splatter(rng)
    s = save_gray(s_arr, 'T_B1MudSplatter.png')
    nz = save_gray(noise(rng), 'T_B1MudNoise.png')
    tris = clump_obj(rng, os.path.join(OUT, 'SM_B1MudClump.obj'))
    contact_sheet([b, s, nz], os.path.join(OUT, 'contact_sheet.png'))
    print(f'seed={SEED} splatter_cover={cover:.2f} clump_tris={tris}')


main()
