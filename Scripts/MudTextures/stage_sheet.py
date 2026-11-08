"""Decode a photobooth.py result and build the stage sheet next to the concept images.

usage: stage_sheet.py [--lens] <out_dir> <tool-result files...>
Writes <out_dir>/<key>.png per still and <out_dir>/stage_sheet.png (dirt) or lens_sheet.png (--lens).
Concept images come from <repo>/Concept. Non-image values in the result are printed.
"""
import sys, os, json, base64
from PIL import Image, ImageDraw

args = sys.argv[1:]
lens = args and args[0] == '--lens'
if lens:
    args = args[1:]
out_dir, sources = args[0], args[1:]
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
        if not isinstance(v, str) or len(v) < 1000:
            print(k, v)
            continue
        p = os.path.join(out_dir, k + '.png')
        open(p, 'wb').write(base64.b64decode(v))
        stills[k] = Image.open(p).convert('RGB')


def lens_sheet():
    # keys "lNNN" = LensDirt NNN/100 from the chase pose; "lnone" = volume disabled
    cw, ch = 960, 540
    ref = Image.open(os.path.join(concept, 'mud-on-the-screen.jpg')).convert('RGB')
    tiles = [('concept', ref)] + [(k, stills[k]) for k in sorted(stills) if k.startswith('l')]
    cols = 2
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new('RGB', (cw * cols, (ch + 24) * rows), (24, 24, 24))
    d = ImageDraw.Draw(sheet)
    for i, (label, img) in enumerate(tiles):
        x, y = (i % cols) * cw, (i // cols) * (ch + 24)
        # fill the tile height, centre-crop the width (captures are wider than 16:9)
        s = ch / img.height
        im = img.resize((int(img.width * s), ch))
        off = (im.width - cw) // 2
        sheet.paste(im.crop((off, 0, off + cw, ch)) if off > 0 else im, (x, y + 24))
        name = label if not label[1:].isdigit() else 'lens %.2f' % (int(label[1:]) / 100)
        d.text((x + 6, y + 6), name, fill=(230, 230, 230))
    path = os.path.join(out_dir, 'lens_sheet.png')
    sheet.save(path)
    print(path)


def dirt_sheet():
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


if lens:
    lens_sheet()
elif stills:
    dirt_sheet()
