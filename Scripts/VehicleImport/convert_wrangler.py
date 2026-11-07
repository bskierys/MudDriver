"""Split the Sketchfab Wrangler FBX into SM body, SM tire and a wheel-bone SKM for a Chaos vehicle.

usage: convert_wrangler.py <source.fbx> <out_dir> [cm_per_unit] [wheel_scale] [lift_cm] [track_out_cm]
"""
import sys, os, json
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import fbxscene
from fbxwrite import FbxScene

src, out_dir = sys.argv[1], sys.argv[2]
S = float(sys.argv[3]) if len(sys.argv) > 3 else 19.5
WHEEL_SCALE = float(sys.argv[4]) if len(sys.argv) > 4 else 1.2
LIFT = float(sys.argv[5]) if len(sys.argv) > 5 else 6.0
TRACK_OUT = float(sys.argv[6]) if len(sys.argv) > 6 else 3.0

parts = [p for p in fbxscene.load_scene(src) if p.name != 'Turntable']


def is_tire(n): return n.startswith('tire_org')
def is_rim(n): return n == 'Rim_org2' or n.endswith('Nut9') or n == 'Rear_wheel1_Wheel_L_Center_circle'
def is_hub(n): return n.startswith('Wheel_cylinder')
def is_axle(n): return n in ('pCylinder3', 'pCylinder4')


ground = min(p.positions[:, 1].min() for p in parts)
tires = [p for p in parts if is_tire(p.name)]
zs = sorted({round(float(t.center[2]), 2) for t in tires})
zmid = (zs[0] + zs[-1]) / 2


def to_ue(P):
    return np.c_[(P[:, 2] - zmid), -P[:, 0], (P[:, 1] - ground)] * S


def nrm_ue(N):
    return np.c_[N[:, 2], -N[:, 0], N[:, 1]]


for p in parts:
    p.ue = to_ue(p.positions)
    p.uen = nrm_ue(p.normals)

wheels = {}
for t in tires:
    c = (t.ue.min(0) + t.ue.max(0)) / 2
    key = ('F' if c[0] > 0 else 'B') + ('L' if c[1] < 0 else 'R')
    wheels[key] = {'center': c, 'radius': float((t.ue[:, 2].max() - t.ue[:, 2].min()) / 2),
                   'width': float(t.ue[:, 1].max() - t.ue[:, 1].min())}
assert len(wheels) == 4, wheels


def _nearest(p):
    c = (p.ue.min(0) + p.ue.max(0)) / 2
    return min(wheels, key=lambda k: np.linalg.norm(wheels[k]['center'] - c))


# Bigger wheels: scale each wheel about its centre, push it down (lift) and out (track), then re-ground.
for p in parts:
    p.wheel = _nearest(p) if (is_tire(p.name) or is_rim(p.name) or is_hub(p.name)) else None
for k, w in wheels.items():
    side = -1.0 if k.endswith('L') else 1.0
    w['old_center'] = w['center'].copy()
    w['center'] = w['center'] + np.array([0.0, side * TRACK_OUT, -LIFT])
    w['radius'] *= WHEEL_SCALE
    w['width'] *= WHEEL_SCALE
for p in parts:
    if p.wheel:
        w = wheels[p.wheel]
        p.ue = w['center'] + (p.ue - w['old_center']) * WHEEL_SCALE
    elif is_axle(p.name):
        p.ue = p.ue + np.array([0.0, 0.0, -LIFT])
new_ground = min(p.ue[:, 2].min() for p in parts if is_tire(p.name))
for p in parts:
    p.ue = p.ue - np.array([0.0, 0.0, new_ground])
for w in wheels.values():
    w['center'] = w['center'] - np.array([0.0, 0.0, new_ground])
    del w['old_center']
BODY_RAISE = -new_ground


def nearest_wheel(p):
    return p.wheel or _nearest(p)


def merge(plist, offset=np.zeros(3)):
    names = []
    for p in plist:
        for m in p.mat_names:
            if m not in names:
                names.append(m)
    P = np.concatenate([p.ue - offset for p in plist])
    N = np.concatenate([p.uen for p in plist])
    U = np.concatenate([p.uvs for p in plist])
    M = np.concatenate([np.array([names.index(p.mat_names[i]) for i in p.tri_mats]) for p in plist])
    return P, N, U, M, names


os.makedirs(out_dir, exist_ok=True)

# body
body = [p for p in parts if not (is_tire(p.name) or is_rim(p.name) or is_hub(p.name) or is_axle(p.name))]
P, N, U, M, names = merge(body)
sc = FbxScene(); sc.add_mesh('SM_Wrangler_Body', P, N, U, M, names); sc.save(os.path.join(out_dir, 'SM_Wrangler_Body.fbx'))
body_min, body_max = P.min(0), P.max(0)

# tire (front-left tire + rim, centred on the wheel)
fl = [p for p in parts if (is_tire(p.name) or is_rim(p.name)) and nearest_wheel(p) == 'FL']
assert len(fl) == 2, [p.name for p in fl]
P, N, U, M, names = merge(fl, wheels['FL']['center'])
sc = FbxScene(); sc.add_mesh('SM_Wrangler_Tire', P, N, U, M, names); sc.save(os.path.join(out_dir, 'SM_Wrangler_Tire.fbx'))
tire_min, tire_max = P.min(0), P.max(0)

# skeletal: axles on Root, hubs on VisWheel_*
skin_parts = [p for p in parts if is_axle(p.name) or is_hub(p.name)]
P, N, U, M, names = merge(skin_parts)
corner_bone = np.concatenate([np.full(len(p.ue), 0 if is_axle(p.name) else -1) for p in skin_parts])
# Bone names are a subset of SKM_Offroad_Skeleton (same parents), so the mesh shares that skeleton.
order = ['FL', 'FR', 'BL', 'BR']
sc = FbxScene()
root = sc.add_bone('OffroadCar', (0, 0, 0))
bones = [(root, (0, 0, 0))]
vis_index = {}
for k in order:
    c = wheels[k]['center']
    phys = sc.add_bone(f'PhysWheel_{k}', c, root)
    vis_index[k] = len(bones)
    bones.append((phys, c))
off = 0
for p in skin_parts:
    n = len(p.ue)
    if is_hub(p.name):
        corner_bone[off:off + n] = vis_index[nearest_wheel(p)]
    off += n
gid, mid, inv, nuniq = sc.add_mesh('SKM_Wrangler', P, N, U, M, names)
vert_bone = np.zeros(nuniq, dtype=np.int64)
vert_bone[inv] = corner_bone
sc.add_skin(gid, mid, bones, vert_bone)
sc.save(os.path.join(out_dir, 'SKM_Wrangler.fbx'))

info = {
    'cm_per_unit': S, 'wheel_scale': WHEEL_SCALE, 'lift': LIFT, 'track_out': TRACK_OUT,
    'body_raise': round(float(BODY_RAISE), 2),
    'wheels': {k: {'center': [round(float(x), 2) for x in v['center']], 'radius': round(v['radius'], 2), 'width': round(v['width'], 2)} for k, v in wheels.items()},
    'body_bounds': [body_min.round(1).tolist(), body_max.round(1).tolist()],
    'tire_bounds': [tire_min.round(1).tolist(), tire_max.round(1).tolist()],
    'body_tris': int(sum(len(p.tri_mats) for p in body)),
    'tire_tris': int(sum(len(p.tri_mats) for p in fl)),
    'skin_parts': [p.name for p in skin_parts],
}
print(json.dumps(info, indent=1))
json.dump(info, open(os.path.join(out_dir, 'wrangler_info.json'), 'w'), indent=1)
