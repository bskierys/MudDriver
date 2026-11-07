"""Bake an FBX scene into world-space triangle lists per mesh node (FBX units, FBX axes)."""
import math
import numpy as np
import fbxread


def _rot(euler_deg, order=0):
    x, y, z = [math.radians(a) for a in euler_deg]
    cx, sx, cy, sy, cz, sz = math.cos(x), math.sin(x), math.cos(y), math.sin(y), math.cos(z), math.sin(z)
    Rx = np.array([[1, 0, 0, 0], [0, cx, -sx, 0], [0, sx, cx, 0], [0, 0, 0, 1]])
    Ry = np.array([[cy, 0, sy, 0], [0, 1, 0, 0], [-sy, 0, cy, 0], [0, 0, 0, 1]])
    Rz = np.array([[cz, -sz, 0, 0], [sz, cz, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]])
    if order != 0:
        raise NotImplementedError(f"rotation order {order}")
    return Rz @ Ry @ Rx  # eEulerXYZ: X applied first


def _tr(v):
    m = np.eye(4); m[:3, 3] = v; return m


def _sc(v):
    return np.diag([v[0], v[1], v[2], 1.0])


def _vec(p, key, default):
    return list(p[key][:3]) if key in p else default


def local_matrix(p):
    T = _tr(_vec(p, 'Lcl Translation', [0, 0, 0]))
    R = _rot(_vec(p, 'Lcl Rotation', [0, 0, 0]), int(p.get('RotationOrder', [0])[0]))
    S = _sc(_vec(p, 'Lcl Scaling', [1, 1, 1]))
    Roff = _tr(_vec(p, 'RotationOffset', [0, 0, 0]))
    Rp = _tr(_vec(p, 'RotationPivot', [0, 0, 0]))
    Rpre = _rot(_vec(p, 'PreRotation', [0, 0, 0]))
    Rpost = _rot(_vec(p, 'PostRotation', [0, 0, 0]))
    Soff = _tr(_vec(p, 'ScalingOffset', [0, 0, 0]))
    Sp = _tr(_vec(p, 'ScalingPivot', [0, 0, 0]))
    return T @ Roff @ Rp @ Rpre @ R @ np.linalg.inv(Rpost) @ np.linalg.inv(Rp) @ Soff @ Sp @ S @ np.linalg.inv(Sp)


def geometric_matrix(p):
    return _tr(_vec(p, 'GeometricTranslation', [0, 0, 0])) @ _rot(_vec(p, 'GeometricRotation', [0, 0, 0])) @ _sc(_vec(p, 'GeometricScaling', [1, 1, 1]))


def _layer(geo, name, data_key, index_key):
    le = geo.find(name)
    if le is None:
        return None
    mapping = le.find('MappingInformationType').props[0]
    ref = le.find('ReferenceInformationType').props[0]
    data = le.find(data_key).props[0]
    idx = le.find(index_key).props[0] if (ref == 'IndexToDirect' and le.find(index_key)) else None
    return mapping, ref, data, idx


class MeshPart:
    def __init__(self, name, path, positions, normals, uvs, mat_names, tri_mats):
        self.name, self.path = name, path
        self.positions = positions      # (N*3, 3) world, per triangle corner
        self.normals = normals          # (N*3, 3)
        self.uvs = uvs                  # (N*3, 2)
        self.mat_names = mat_names
        self.tri_mats = tri_mats        # (N,) index into mat_names

    @property
    def center(self):
        return (self.positions.min(0) + self.positions.max(0)) / 2


def load_scene(path):
    root, _ = fbxread.load(path)
    objs = root.find('Objects')
    byid = {o.props[0]: o for o in objs.children}
    kids = {}
    for c in root.find('Connections').children:
        kids.setdefault(c.props[2], []).append(c.props[1])

    def nm(o):
        return o.props[1].split('\x00')[0]

    parts = []

    def walk(pid, parent_m, path):
        for cid in kids.get(pid, []):
            o = byid.get(cid)
            if o is None or o.name != 'Model':
                continue
            p = fbxread.props70(o)
            m = parent_m @ local_matrix(p)
            name = nm(o)
            if o.props[2] == 'Mesh':
                geo = next((byid[g] for g in kids.get(cid, []) if g in byid and byid[g].name == 'Geometry'), None)
                mats = [nm(byid[g]) for g in kids.get(cid, []) if g in byid and byid[g].name == 'Material']
                if geo is not None:
                    parts.append(_bake(geo, m @ geometric_matrix(p), name, path + [name], mats))
            walk(cid, m, path + [name])

    walk(0, np.eye(4), [])
    return parts


def _bake(geo, M, name, path, mats):
    verts = np.array(geo.find('Vertices').props[0], dtype=np.float64).reshape(-1, 3)
    pvi = geo.find('PolygonVertexIndex').props[0]
    nl = _layer(geo, 'LayerElementNormal', 'Normals', 'NormalsIndex')
    ul = _layer(geo, 'LayerElementUV', 'UV', 'UVIndex')
    ml = geo.find('LayerElementMaterial')
    mat_mapping = ml.find('MappingInformationType').props[0] if ml else 'AllSame'
    mat_arr = ml.find('Materials').props[0] if ml else [0]

    def fetch(layer, width, poly_i, corner_i, vert_i):
        mapping, ref, data, idx = layer
        if mapping == 'ByPolygonVertex':
            k = corner_i
        elif mapping in ('ByVertice', 'ByVertex'):
            k = vert_i
        elif mapping == 'ByPolygon':
            k = poly_i
        else:
            k = 0
        if idx is not None:
            k = idx[k]
        return data[k * width:(k + 1) * width]

    pos, nrm, uv, tmat = [], [], [], []
    poly, corner_start, poly_i = [], 0, 0
    for ci, v in enumerate(pvi):
        last = v < 0
        poly.append((ci, ~v if last else v))
        if last:
            pm = mat_arr[poly_i] if mat_mapping == 'ByPolygon' else mat_arr[0]
            for t in range(1, len(poly) - 1):
                for (c, vi) in (poly[0], poly[t], poly[t + 1]):
                    pos.append(verts[vi])
                    nrm.append(fetch(nl, 3, poly_i, c, vi) if nl else (0, 0, 1))
                    uv.append(fetch(ul, 2, poly_i, c, vi) if ul else (0, 0))
                tmat.append(pm)
            poly = []
            poly_i += 1
    pos = np.array(pos); nrm = np.array(nrm, dtype=np.float64); uv = np.array(uv, dtype=np.float64)
    P = (np.c_[pos, np.ones(len(pos))] @ M.T)[:, :3]
    N3 = np.linalg.inv(M[:3, :3]).T
    Nn = nrm @ N3.T
    Nn /= np.maximum(np.linalg.norm(Nn, axis=1, keepdims=True), 1e-12)
    if np.linalg.det(M[:3, :3]) < 0:
        P = P.reshape(-1, 3, 3)[:, ::-1].reshape(-1, 3)
        Nn = Nn.reshape(-1, 3, 3)[:, ::-1].reshape(-1, 3)
        uv = uv.reshape(-1, 3, 2)[:, ::-1].reshape(-1, 2)
    return MeshPart(name, path, P, Nn, uv, mats if mats else ['Default_Material'], np.array(tmat, dtype=np.int64))
