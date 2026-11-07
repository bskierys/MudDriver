"""Minimal binary FBX reader: returns node tree of (name, props, children)."""
import struct, zlib

class Node:
    __slots__ = ("name", "props", "children")
    def __init__(self, name, props, children):
        self.name, self.props, self.children = name, props, children
    def find(self, name):
        for c in self.children:
            if c.name == name:
                return c
        return None
    def findall(self, name):
        return [c for c in self.children if c.name == name]
    def __repr__(self):
        return f"Node({self.name}, {len(self.props)} props, {len(self.children)} children)"

_ARR = {b'f': ('f', 4), b'd': ('d', 8), b'l': ('q', 8), b'i': ('i', 4), b'b': ('?', 1)}

def _read_prop(data, off):
    t = data[off:off+1]; off += 1
    if t == b'Y': return struct.unpack_from('<h', data, off)[0], off + 2
    if t == b'C': return bool(data[off]), off + 1
    if t == b'I': return struct.unpack_from('<i', data, off)[0], off + 4
    if t == b'F': return struct.unpack_from('<f', data, off)[0], off + 4
    if t == b'D': return struct.unpack_from('<d', data, off)[0], off + 8
    if t == b'L': return struct.unpack_from('<q', data, off)[0], off + 8
    if t in (b'S', b'R'):
        n = struct.unpack_from('<I', data, off)[0]; off += 4
        raw = data[off:off+n]; off += n
        if t == b'S':
            return raw.decode('utf-8', 'replace'), off
        return raw, off
    if t in _ARR:
        fmt, sz = _ARR[t]
        count, enc, clen = struct.unpack_from('<III', data, off); off += 12
        raw = data[off:off+clen]; off += clen
        if enc == 1:
            raw = zlib.decompress(raw)
        return list(struct.unpack('<%d%s' % (count, fmt), raw[:count*sz])), off
    raise ValueError(f"unknown prop type {t} at {off}")

def _read_node(data, off, v75):
    if v75:
        end, nprops, plen = struct.unpack_from('<QQQ', data, off); off += 24
    else:
        end, nprops, plen = struct.unpack_from('<III', data, off); off += 12
    nlen = data[off]; off += 1
    if end == 0:
        return None, off
    name = data[off:off+nlen].decode('ascii', 'replace'); off += nlen
    props = []
    for _ in range(nprops):
        p, off = _read_prop(data, off)
        props.append(p)
    children = []
    sentinel = 25 if v75 else 13
    while off < end:
        if end - off == sentinel and data[off:end] == b'\0' * sentinel:
            off = end; break
        ch, off = _read_node(data, off, v75)
        if ch is None:
            break
        children.append(ch)
    return Node(name, props, children), end

def load(path):
    data = open(path, 'rb').read()
    assert data[:20] == b'Kaydara FBX Binary  '
    version = struct.unpack_from('<I', data, 23)[0]
    v75 = version >= 7500
    off = 27
    top = []
    while off < len(data):
        n, off = _read_node(data, off, v75)
        if n is None:
            break
        top.append(n)
    return Node('root', [], top), version

def props70(node):
    """Return dict of Properties70 values for an object node."""
    out = {}
    p = node.find('Properties70')
    if p:
        for P in p.children:
            out[P.props[0]] = P.props[4:]
    return out
