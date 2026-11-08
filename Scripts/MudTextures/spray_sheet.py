"""Decode spray_capture.py results and build the spray sheet next to frames of Concept/mud-car-drive.mp4.

usage: spray_sheet.py <out_dir> <tool-result files...>
Keys "cNN" are captures at NN/10 s into the rig script; other keys are printed. Writes <out_dir>/cNN.png,
<out_dir>/spray_sheet.png. Needs numpy, pillow and opencv-python-headless.
"""
import sys, os, json, base64, re
import cv2
import numpy as np
from PIL import Image, ImageDraw

out_dir, sources = sys.argv[1], sys.argv[2:]
concept = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Concept')
os.makedirs(out_dir, exist_ok=True)

shots = {}
for src in sources:
    raw = json.load(open(src, encoding='utf-8'))
    if isinstance(raw, list):
        raw = json.loads(raw[0]['text'])
    val = raw['returnValue']
    if isinstance(val, str):
        val = json.loads(val)
    for k, v in val.items():
        if not re.fullmatch(r'c\d+', k):
            print(src, k, v)
            continue
        p = os.path.join(out_dir, k + '.png')
        open(p, 'wb').write(base64.b64decode(v))
        shots[k] = Image.open(p).convert('RGB')


def viewport(img):
    # CaptureEditorImage returns the whole editor: the 3D view is the block that isn't dark UI grey.
    a = np.asarray(img).astype(np.int16)
    bright = a.max(axis=2) > 60
    rows = np.where(bright.mean(axis=1) > 0.5)[0]
    cols = np.where(bright[rows].mean(axis=0) > 0.9)[0] if len(rows) else []
    if len(rows) < 50 or len(cols) < 50:
        return img
    vp = img.crop((cols.min(), rows.min(), cols.max() + 1, rows.max() + 1))
    # zoom on the jeep's path (B1Spray_Medium camera): middle band of the view
    w, h = vp.size
    return vp.crop((int(w * 0.18), int(h * 0.28), int(w * 0.78), int(h * 0.62)))


cap = cv2.VideoCapture(os.path.join(concept, 'mud-car-drive.mp4'))
n = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
refs = []
for i in (0, n // 3, 2 * n // 3):
    cap.set(cv2.CAP_PROP_POS_FRAMES, i)
    ok, f = cap.read()
    if ok:
        refs.append(Image.fromarray(cv2.cvtColor(f, cv2.COLOR_BGR2RGB)))

cw, ch = 640, 360
keys = sorted(shots, key=lambda k: int(k[1:]))
rows = max(len(keys), len(refs))
sheet = Image.new('RGB', (cw * 2, (ch + 24) * rows), (24, 24, 24))
d = ImageDraw.Draw(sheet)
for r in range(rows):
    y = r * (ch + 24)
    if r < len(keys):
        k = keys[r]
        sheet.paste(viewport(shots[k]).resize((cw, ch)), (0, y + 24))
        d.text((6, y + 6), 'B1Spray_Medium  t=%.1f s' % (int(k[1:]) / 10), fill=(230, 230, 230))
    if r < len(refs):
        sheet.paste(refs[r].resize((cw, ch)), (cw, y + 24))
        d.text((cw + 6, y + 6), 'mud-car-drive.mp4', fill=(230, 230, 230))
sheet.save(os.path.join(out_dir, 'spray_sheet.png'))
print(os.path.join(out_dir, 'spray_sheet.png'))
