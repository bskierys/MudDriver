"""Decode a photobooth.py result and build the stage sheet next to the concept images.

usage: stage_sheet.py <out_dir> <tool-result files...>
Writes <out_dir>/<key>.png per still and <out_dir>/stage_sheet.png. Concept images come from <repo>/Concept.
"""
import sys, os, json, base64
from PIL import Image, ImageDraw

out_dir, sources = sys.argv[1], sys.argv[2:]
concept = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Concept')

os.makedirs(out_dir, exist_ok=True)
stills = {}
for src in sources:
    raw = json.load(open(src, encoding='utf-8'))
    if isinstance(raw, list):
        raw = json.loads(raw[0]['text'])
    val = raw['returnValue']
    if isinstance(val, str):
        val = json.loads(val)
    for k, v in val.items():
        p = os.path.join(out_dir, k + '.png')
        open(p, 'wb').write(base64.b64decode(v))
        stills[k] = Image.open(p).convert('RGB')

stages = sorted({k.split('_')[0] for k in stills})
poses = ['front', 'rear']
cw, ch = 560, 365
# crop the centre of the capture, where the jeep is
def crop(img):
    w, h = img.size
    box = (int(w * 0.24), int(h * 0.22), int(w * 0.76), int(h * 0.78))
    return img.crop(box).resize((cw, ch))

ref = Image.open(os.path.join(concept, 'jeep_wrangler_mud_reference.jpg')).convert('RGB')
clean = Image.open(os.path.join(concept, 'jeep-wrangler.png')).convert('RGB')
rw = ref.width // 2
refs = {'front': ref.crop((0, 0, rw, ref.height)), 'rear': ref.crop((rw, 0, ref.width, ref.height))}

cols = len(stages) + 2
sheet = Image.new('RGB', (cw * cols, (ch + 24) * len(poses)), (24, 24, 24))
d = ImageDraw.Draw(sheet)
for r, pose in enumerate(poses):
    y = r * (ch + 24)
    tiles = [('clean ref', clean if pose == 'front' else None)] + \
            [('dirt %.2f' % (int(s[1:]) / 100), stills.get(s + '_' + pose)) for s in stages] + \
            [('mud ref', refs[pose])]
    for c, (label, img) in enumerate(tiles):
        if img is None:
            continue
        tile = crop(img) if label.startswith('dirt') else img.resize((cw, ch))
        sheet.paste(tile, (c * cw, y + 24))
        d.text((c * cw + 6, y + 6), label, fill=(230, 230, 230))
sheet.save(os.path.join(out_dir, 'stage_sheet.png'))
print(os.path.join(out_dir, 'stage_sheet.png'))
