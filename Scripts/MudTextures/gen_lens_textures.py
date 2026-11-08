"""Generate the lens drop field for M_B1LensMud (ADR-022) and a preview sheet.

usage: gen_lens_textures.py <out_dir> [seed=22] [concept.jpg]

One tile = one splash slot; 4 full slots = the max look (S5-25: about 60 drops on screen, more at the bottom).
T_B1LensDrops RGBA: RG = drop normal xy (0.5-biased), B = 1 - normalized radius (0 outside, 1 at the centre),
A = per-drop reveal id (background 1). Ids are spread evenly over the drops (threshold t reveals fraction t).
Outlines wobble and some drops have a smaller blob merged in, so few are round.
Tiles wrap in both axes. Tile height maps to screen height, so 1 px ~ 1 px at 1080p.
"""
import sys, os, math
import numpy as np
from PIL import Image

OUT = sys.argv[1]
SEED = int(sys.argv[2]) if len(sys.argv) > 2 else 22
CONCEPT = sys.argv[3] if len(sys.argv) > 3 else None

W, H = 2048, 1024
DROP_COUNT = 26                # the shader thins the top of the screen (LensTopDensity) and enlarges the bottom
DROP_RADIUS = (9.0, 36.0)      # px, skewed small
DROP_ASPECT = (1.0, 1.5)
DROP_WOBBLE = (0.11, 0.06, 0.03)  # outline amplitude of the 2nd, 3rd, 5th harmonic (fraction of radius)
SATELLITE_CHANCE = 0.4
SATELLITE_SIZE = (0.35, 0.7)   # of the main radius
SATELLITE_DIST = (0.55, 0.95)  # centre distance, of the main radius
PREVIEW_DROP_FILL = 1.3        # stand-ins for the MI params
PREVIEW_SOFTNESS = 0.3


def blob(field_shape, cx, cy, r, asp, ang, phases, half):
    """Wrapped patch of one wobbly ellipse: (rows, cols, edge value 1 - r_eff, normal x, normal y)."""
    xs = np.arange(int(cx) - half, int(cx) + half + 1)
    ys = np.arange(int(cy) - half, int(cy) + half + 1)
    dx = xs[None, :] + 0.5 - cx
    dy = ys[:, None] + 0.5 - cy
    ca, sa = math.cos(ang), math.sin(ang)
    ex = (dx * ca + dy * sa) / asp
    ey = -dx * sa + dy * ca
    th = np.arctan2(ey, ex)
    rr = r * (1.0 + sum(a * np.sin(k * th + p) for a, k, p in zip(DROP_WOBBLE, (2, 3, 5), phases)))
    q = np.sqrt(ex * ex + ey * ey) / rr
    edge = np.clip(1.0 - q, 0.0, 1.0)
    # dome normal (unit sphere in the wobbled frame), rotated back to image axes
    qx, qy = np.where(q < 1, ex / rr, 0), np.where(q < 1, ey / rr, 0)
    nx, ny = qx * ca - qy * sa, qx * sa + qy * ca
    return ys % field_shape[0], xs % field_shape[1], edge, nx, ny


def reveal_ids(rng, n, size_u):
    """Even ids in (0,1); medium drops come first, the tiny and the largest later."""
    score = 0.5 * rng.random(n) + np.abs(size_u - 0.4)
    ids = np.empty(n)
    ids[np.argsort(score, kind='stable')] = (np.arange(n) + 0.5) / n
    return ids


def drops(rng):
    edge = np.zeros((H, W))
    nx = np.zeros((H, W))
    ny = np.zeros((H, W))
    ident = np.ones((H, W))
    size_u = rng.random(DROP_COUNT) ** 1.6
    ids = reveal_ids(rng, DROP_COUNT, size_u)
    for i in range(DROP_COUNT):
        r = DROP_RADIUS[0] + (DROP_RADIUS[1] - DROP_RADIUS[0]) * size_u[i]
        cx, cy = rng.random() * W, rng.random() * H
        parts = [(cx, cy, r, rng.uniform(*DROP_ASPECT), rng.random() * math.pi)]
        if rng.random() < SATELLITE_CHANCE:
            a = rng.random() * 2 * math.pi
            d = r * rng.uniform(*SATELLITE_DIST)
            parts.append((cx + math.cos(a) * d, cy + math.sin(a) * d, r * rng.uniform(*SATELLITE_SIZE),
                          rng.uniform(*DROP_ASPECT), rng.random() * math.pi))
        for px, py, pr, asp, ang in parts:
            phases = rng.random(3) * 2 * math.pi
            half = int(pr * asp * 1.35) + 2
            rows, cols, e, gx, gy = blob(edge.shape, px, py, pr, asp, ang, phases, half)
            ix = np.ix_(rows, cols)
            win = e > edge[ix]
            for arr, val in ((edge, e), (nx, gx), (ny, gy)):
                arr[ix] = np.where(win, val, arr[ix])
            ident[ix] = np.where(win & (e > 0), ids[i], ident[ix])
    nlen = np.sqrt(nx * nx + ny * ny)
    scale = np.where(nlen > 1.0, 1.0 / np.maximum(nlen, 1e-6), 1.0)
    return edge, nx * scale, ny * scale, ident


def to8(a):
    return np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8)


def id8(ident):
    # Background exactly 255 (1.0); ids stay below it.
    return np.where(ident >= 1.0, 255, np.clip(np.round(ident * 254.0), 1, 254)).astype(np.uint8)


def box_blur(img, r):
    out = img.copy()
    for axis in (0, 1):
        acc = np.zeros_like(out)
        for k in range(-r, r + 1):
            acc += np.roll(out, k, axis=axis)
        out = acc / (2 * r + 1)
    return out


def preview(dr, rng, strength, bg):
    """Rough numpy stand-in for M_B1LensMud at one LensDirt level (all 4 slots at strength)."""
    h, w = bg.shape[:2]
    tw, th = dr[0].shape[1], dr[0].shape[0]
    out = bg.copy()
    blurred = box_blur(bg, 3)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    edge, nx, ny, did = dr
    best_a = np.zeros((h, w))
    best_nx = np.zeros((h, w))
    best_ny = np.zeros((h, w))
    for slot in range(4):
        off = rng.random(2)
        var = rng.integers(8)
        u = ((xx / h) * 0.5 + off[0]) % 1.0
        v = (yy / h + off[1]) % 1.0
        u = 1.0 - u if var & 1 else u
        v = 1.0 - v if var & 2 else v
        tx = np.clip((u * tw).astype(int), 0, tw - 1)
        ty = np.clip((v * th).astype(int), 0, th - 1)
        e = edge[ty, tx]
        di = did[ty, tx]
        di = np.where((var & 4) and True, np.where(e > 0, 1.0 - di, di), di)
        thr = max(1.0 - strength * PREVIEW_DROP_FILL, 0.0)
        a = np.where((di < strength) & (e > 0), np.clip((e - thr) / PREVIEW_SOFTNESS, 0, 1), 0)
        win = a > best_a
        best_a = np.where(win, a, best_a)
        best_nx = np.where(win, nx[ty, tx] * (-1 if var & 1 else 1), best_nx)
        best_ny = np.where(win, ny[ty, tx] * (-1 if var & 2 else 1), best_ny)
    sx = np.clip((xx - best_nx * 0.04 * h).astype(int), 0, w - 1)
    sy = np.clip((yy - best_ny * 0.04 * h).astype(int), 0, h - 1)
    refr = blurred[sy, sx]
    nl2 = best_nx ** 2 + best_ny ** 2
    col = refr + (np.array([0.32, 0.22, 0.12]) - refr) * (0.3 * (0.25 + 0.75 * nl2))[..., None]
    a = (best_a * 0.85)[..., None]
    return np.clip(out * (1 - a) + col * a, 0, 1)


def background(w, h):
    yy = np.linspace(0, 1, h)[:, None, None]
    sky = np.array([0.45, 0.62, 0.85]) * (1 - yy) + np.array([0.8, 0.85, 0.9]) * yy
    ground = np.array([0.55, 0.38, 0.18])
    bg = np.where(yy < 0.42, sky, ground) * np.ones((h, w, 3))
    bg[int(h * 0.45):int(h * 0.75), int(w * 0.4):int(w * 0.6)] = (0.1, 0.11, 0.12)  # a dark "jeep"
    return bg


def main():
    os.makedirs(OUT, exist_ok=True)
    rng = np.random.default_rng(SEED)
    de, dnx, dny, did = drops(rng)
    Image.fromarray(np.dstack([to8(0.5 + 0.5 * dnx), to8(0.5 + 0.5 * dny), to8(de), id8(did)]), 'RGBA') \
        .save(os.path.join(OUT, 'T_B1LensDrops.png'))

    # read back the 8-bit file, so the preview sees what the engine sees; preview at half res
    d8 = np.asarray(Image.open(os.path.join(OUT, 'T_B1LensDrops.png'))).astype(np.float64)[::2, ::2] / 255.0
    inside = d8[..., 2] > 0
    dr = (d8[..., 2], np.where(inside, d8[..., 0] * 2 - 1, 0), np.where(inside, d8[..., 1] * 2 - 1, 0), d8[..., 3])
    pw, ph = 960, 540
    bg = background(pw, ph)
    tiles = []
    if CONCEPT:
        c = Image.open(CONCEPT).convert('RGB').resize((pw, ph))
        tiles.append(np.asarray(c).astype(np.float64) / 255.0)
    for s in (0.25, 0.5, 0.75, 1.0):
        tiles.append(preview(dr, np.random.default_rng(SEED + 1), s, bg))
    cols = 2
    rows = (len(tiles) + cols - 1) // cols
    sheet = np.ones((rows * ph, cols * pw, 3))
    for i, t in enumerate(tiles):
        sheet[(i // cols) * ph:(i // cols + 1) * ph, (i % cols) * pw:(i % cols + 1) * pw] = t
    Image.fromarray(to8(sheet)).save(os.path.join(OUT, 'lens_contact_sheet.png'))
    print(f'seed={SEED} drops={DROP_COUNT} drop_cover={np.mean(de > 0):.3f}')


main()
