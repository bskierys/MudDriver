"""Pack Sketchfab PBR maps into UE-style D / N / MRA PNGs at <= 2048 px.

usage: pack_textures.py <src_textures_dir> <out_dir>
"""
import sys, os
from PIL import Image

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
SIZE = 2048
SETS = {
    'CarPaint': 'CarPaint_1011', 'Bumper': 'Bumper1_1003', 'MetalPart': 'Metal_part_1_1004',
    'PlasticPart': 'Plastic_Part_1_1014', 'RoofTop': 'Roof_top_1012', 'Sheets': 'Sheets1_1005',
    'Tire': 'Tire1_1001', 'Glass': 'Windows_Glass_1013',
}


def load(name, mode):
    for ext in ('.jpg', '.png', '.jpeg'):
        p = os.path.join(src, name + ext)
        if os.path.exists(p):
            return Image.open(p).convert(mode).resize((SIZE, SIZE), Image.LANCZOS)
    raise FileNotFoundError(name)


for short, base in SETS.items():
    load(base + '_albedo', 'RGB').save(os.path.join(out, f'T_Wrangler_{short}_D.png'))
    n = load(base + '_normal', 'RGB')
    r, g, b = n.split()
    g = g.point(lambda v: 255 - v)  # OpenGL -> DirectX green
    Image.merge('RGB', (r, g, b)).save(os.path.join(out, f'T_Wrangler_{short}_N.png'))
    m, ro, ao = load(base + '_metallic', 'L'), load(base + '_roughness', 'L'), load(base + '_AO', 'L')
    Image.merge('RGB', (m, ro, ao)).save(os.path.join(out, f'T_Wrangler_{short}_MRA.png'))
    print('packed', short)
