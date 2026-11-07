"""ASCII FBX 7.4 writer for static and rigidly skinned meshes.

Coordinates are written in UE space (cm, X forward, Y right, Z up); the file uses
Z-up / -Y front / right-handed, which the UE importer maps back without conversion.
"""
import numpy as np


def _arr(name, vals, fmt):
    vals = list(vals)
    body = ",".join(fmt(v) for v in vals)
    return f"\t\t{name}: *{len(vals)} {{\n\t\t\ta: {body}\n\t\t}}\n"


def _f(v):
    return repr(float(round(v, 6)))


def _i(v):
    return str(int(v))


def _mat16(m):
    return ",".join(_f(v) for v in np.asarray(m, float).T.reshape(-1))  # column-major


def to_file_space(p):
    p = np.asarray(p, float).copy()
    p[..., 1] *= -1.0
    return p


class FbxScene:
    def __init__(self):
        self._id = 1000000
        self.objects = []
        self.conns = []
        self.counts = {}

    def nid(self):
        self._id += 1
        return self._id

    def _count(self, typ):
        self.counts[typ] = self.counts.get(typ, 0) + 1

    def add_mesh(self, name, positions, normals, uvs, tri_mats, mat_names, parent=0):
        """positions/normals: (3T, 3) UE space; uvs (3T, 2) with V up (FBX convention); tri_mats (T,)."""
        P = to_file_space(positions)
        N = to_file_space(normals)
        key = np.c_[P.round(5)]
        uniq, inv = np.unique(key, axis=0, return_inverse=True)
        inv = inv.reshape(-1)
        T = len(tri_mats)
        pvi = inv.copy().astype(np.int64)
        pvi[2::3] = -pvi[2::3] - 1
        uvk = np.round(uvs, 6)
        uuniq, uinv = np.unique(uvk, axis=0, return_inverse=True)
        uinv = uinv.reshape(-1)

        gid, mid = self.nid(), self.nid()
        g = [f'\tGeometry: {gid}, "Geometry::{name}", "Mesh" {{\n']
        g.append(_arr("Vertices", uniq.reshape(-1), _f))
        g.append(_arr("PolygonVertexIndex", pvi, _i))
        g.append("\t\tGeometryVersion: 124\n")
        g.append('\t\tLayerElementNormal: 0 {\n\t\t\tVersion: 101\n\t\t\tName: ""\n\t\t\tMappingInformationType: "ByPolygonVertex"\n\t\t\tReferenceInformationType: "Direct"\n')
        g.append(_arr("Normals", N.reshape(-1), _f).replace("\t\t", "\t\t\t", 1))
        g.append("\t\t}\n")
        g.append('\t\tLayerElementUV: 0 {\n\t\t\tVersion: 101\n\t\t\tName: "UVMap"\n\t\t\tMappingInformationType: "ByPolygonVertex"\n\t\t\tReferenceInformationType: "IndexToDirect"\n')
        g.append(_arr("UV", uuniq.reshape(-1), _f))
        g.append(_arr("UVIndex", uinv, _i))
        g.append("\t\t}\n")
        g.append('\t\tLayerElementMaterial: 0 {\n\t\t\tVersion: 101\n\t\t\tName: ""\n\t\t\tMappingInformationType: "ByPolygon"\n\t\t\tReferenceInformationType: "IndexToDirect"\n')
        g.append(_arr("Materials", tri_mats, _i))
        g.append("\t\t}\n")
        g.append('\t\tLayer: 0 {\n\t\t\tVersion: 100\n'
                 '\t\t\tLayerElement:  {\n\t\t\t\tType: "LayerElementNormal"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n'
                 '\t\t\tLayerElement:  {\n\t\t\t\tType: "LayerElementMaterial"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n'
                 '\t\t\tLayerElement:  {\n\t\t\t\tType: "LayerElementUV"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n'
                 '\t\t}\n')
        g.append("\t}\n")
        self.objects.append("".join(g)); self._count("Geometry")
        self.objects.append(
            f'\tModel: {mid}, "Model::{name}", "Mesh" {{\n\t\tVersion: 232\n\t\tProperties70:  {{\n'
            f'\t\t\tP: "DefaultAttributeIndex", "int", "Integer", "",0\n'
            f'\t\t\tP: "Lcl Translation", "Lcl Translation", "", "A",0,0,0\n'
            f'\t\t\tP: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0\n'
            f'\t\t\tP: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1\n'
            f'\t\t}}\n\t\tShading: T\n\t\tCulling: "CullingOff"\n\t}}\n'); self._count("Model")
        self.conns.append(f'\tC: "OO",{mid},{parent}\n')
        self.conns.append(f'\tC: "OO",{gid},{mid}\n')
        for mn in mat_names:
            matid = self.nid()
            self.objects.append(
                f'\tMaterial: {matid}, "Material::{mn}", "" {{\n\t\tVersion: 102\n\t\tShadingModel: "phong"\n\t\tMultiLayer: 0\n'
                f'\t\tProperties70:  {{\n\t\t\tP: "DiffuseColor", "Color", "", "A",0.8,0.8,0.8\n\t\t}}\n\t}}\n'); self._count("Material")
            self.conns.append(f'\tC: "OO",{matid},{mid}\n')
        return gid, mid, inv, len(uniq)

    def add_bone(self, name, local_t_ue, parent_model=0):
        aid, mid = self.nid(), self.nid()
        t = to_file_space(np.array(local_t_ue, float))
        self.objects.append(f'\tNodeAttribute: {aid}, "NodeAttribute::{name}", "LimbNode" {{\n\t\tProperties70:  {{\n\t\t\tP: "Size", "double", "Number", "",10\n\t\t}}\n\t\tTypeFlags: "Skeleton"\n\t}}\n'); self._count("NodeAttribute")
        self.objects.append(
            f'\tModel: {mid}, "Model::{name}", "LimbNode" {{\n\t\tVersion: 232\n\t\tProperties70:  {{\n'
            f'\t\t\tP: "DefaultAttributeIndex", "int", "Integer", "",0\n'
            f'\t\t\tP: "Lcl Translation", "Lcl Translation", "", "A",{_f(t[0])},{_f(t[1])},{_f(t[2])}\n'
            f'\t\t\tP: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0\n'
            f'\t\t\tP: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1\n'
            f'\t\t}}\n\t\tShading: T\n\t\tCulling: "CullingOff"\n\t}}\n'); self._count("Model")
        self.conns.append(f'\tC: "OO",{aid},{mid}\n')
        self.conns.append(f'\tC: "OO",{mid},{parent_model}\n')
        return mid

    def add_skin(self, geo_id, mesh_model_id, bones, vert_bone):
        """bones: list of (model_id, global_t_ue). vert_bone: per unique vertex, index into bones (rigid)."""
        sid = self.nid()
        self.objects.append(f'\tDeformer: {sid}, "Deformer::Skin", "Skin" {{\n\t\tVersion: 101\n\t\tLink_DeformAcuracy: 50\n\t}}\n'); self._count("Deformer")
        self.conns.append(f'\tC: "OO",{sid},{geo_id}\n')
        pose = []
        for bi, (bmid, gt) in enumerate(bones):
            idx = np.nonzero(vert_bone == bi)[0]
            if len(idx) == 0:
                continue
            cid = self.nid()
            link = np.eye(4); link[:3, 3] = to_file_space(np.array(gt, float))
            xf = np.linalg.inv(link)
            c = [f'\tDeformer: {cid}, "SubDeformer::Cluster {bi}", "Cluster" {{\n\t\tVersion: 100\n\t\tUserData: "", ""\n']
            c.append(_arr("Indexes", idx, _i))
            c.append(_arr("Weights", np.ones(len(idx)), _f))
            c.append(f"\t\tTransform: *16 {{\n\t\t\ta: {_mat16(xf)}\n\t\t}}\n")
            c.append(f"\t\tTransformLink: *16 {{\n\t\t\ta: {_mat16(link)}\n\t\t}}\n\t}}\n")
            self.objects.append("".join(c)); self._count("Deformer")
            self.conns.append(f'\tC: "OO",{cid},{sid}\n')
            self.conns.append(f'\tC: "OO",{bmid},{cid}\n')
        pid = self.nid()
        nodes = [(mesh_model_id, np.eye(4))]
        for bmid, gt in bones:
            m = np.eye(4); m[:3, 3] = to_file_space(np.array(gt, float)); nodes.append((bmid, m))
        p = [f'\tPose: {pid}, "Pose::BindPose", "BindPose" {{\n\t\tType: "BindPose"\n\t\tVersion: 100\n\t\tNbPoseNodes: {len(nodes)}\n']
        for nid_, m in nodes:
            p.append(f"\t\tPoseNode:  {{\n\t\t\tNode: {nid_}\n\t\t\tMatrix: *16 {{\n\t\t\t\ta: {_mat16(m)}\n\t\t\t}}\n\t\t}}\n")
        p.append("\t}\n")
        self.objects.append("".join(p)); self._count("Pose")

    def save(self, path):
        head = ('; FBX 7.4.0 project file\n'
                'FBXHeaderExtension:  {\n\tFBXHeaderVersion: 1003\n\tFBXVersion: 7400\n\tCreator: "mudriver fbx tools"\n}\n'
                'GlobalSettings:  {\n\tVersion: 1000\n\tProperties70:  {\n'
                '\t\tP: "UpAxis", "int", "Integer", "",2\n\t\tP: "UpAxisSign", "int", "Integer", "",1\n'
                '\t\tP: "FrontAxis", "int", "Integer", "",1\n\t\tP: "FrontAxisSign", "int", "Integer", "",-1\n'
                '\t\tP: "CoordAxis", "int", "Integer", "",0\n\t\tP: "CoordAxisSign", "int", "Integer", "",1\n'
                '\t\tP: "OriginalUpAxis", "int", "Integer", "",2\n\t\tP: "OriginalUpAxisSign", "int", "Integer", "",1\n'
                '\t\tP: "UnitScaleFactor", "double", "Number", "",1\n\t\tP: "OriginalUnitScaleFactor", "double", "Number", "",1\n'
                '\t}\n}\n')
        defs = "Definitions:  {\n\tVersion: 100\n\tCount: %d\n" % (sum(self.counts.values()) + 1)
        defs += '\tObjectType: "GlobalSettings" {\n\t\tCount: 1\n\t}\n'
        templates = {"Model": "FbxNode", "Geometry": "FbxMesh", "Material": "FbxSurfacePhong", "NodeAttribute": "FbxSkeleton"}
        for typ, n in self.counts.items():
            tpl = templates.get(typ)
            tpl_txt = f'\t\tPropertyTemplate: "{tpl}" {{\n\t\t\tProperties70:  {{\n\t\t\t}}\n\t\t}}\n' if tpl else ""
            defs += f'\tObjectType: "{typ}" {{\n\t\tCount: {n}\n{tpl_txt}\t}}\n'
        defs += "}\n"
        with open(path, "w", encoding="ascii") as f:
            f.write(head)
            f.write(defs)
            f.write("Objects:  {\n")
            for o in self.objects:
                f.write(o)
            f.write("}\nConnections:  {\n")
            for c in self.conns:
                f.write(c)
            f.write("}\n")
