"""Validate the runtime character asset and render a software preview without a build."""
import argparse
from pathlib import Path
import struct
import json
import numpy as np
from PIL import Image


def load(path):
    with path.open('rb') as f:
        magic = f.read(8)
        assert magic in (b'TKCHAR01', b'TKCHAR02', b'TKCHAR03')
        nj, nv, ni = struct.unpack('<III', f.read(12))
        joints = {}
        for _ in range(nj):
            size, = struct.unpack('<I', f.read(4))
            name = f.read(size).decode('ascii')
            assert name not in joints
            joints[name] = np.array(struct.unpack('<3f', f.read(12)))
        fields = [('position', '<f4', 3), ('normal', '<f4', 3), ('uv', '<f4', 2),
                  ('joints', '<u4', 4), ('weights', '<f4', 4)]
        if magic != b'TKCHAR01': fields.append(('garment', '<u4'))
        dtype = np.dtype(fields)
        vertices = np.frombuffer(f.read(nv * dtype.itemsize), dtype=dtype)
        indices = np.frombuffer(f.read(ni * 4), dtype='<u4').reshape(-1, 3)
        assert not f.read()
    assert len(vertices) == nv and indices.max() < nv and vertices['joints'].max() < nj
    assert np.allclose(vertices['weights'].sum(1), 1) and (vertices['weights'] >= 0).all()
    for field in ('position', 'normal', 'uv', 'weights'): assert np.isfinite(vertices[field]).all()
    assert (vertices['uv'] >= 0).all() and (vertices['uv'] <= 1).all()
    if 'garment' in vertices.dtype.names:
        assert {0, 5} <= set(vertices['garment']) <= {0, 1, 2, 3, 4, 5}
        assert np.all(vertices['garment'][indices] == vertices['garment'][indices[:, :1]])
    for side in ('Left', 'Right'):
        assert all(side + suffix in joints for suffix in ('Arm', 'ForeArm', 'Hand'))
    return vertices, indices, joints


def preview(vertices, triangles, atlas, width=640, height=800, yaw=0, low=None, high=None, clothing_mask=15):
    p = vertices['position'].astype(float).copy()
    angle = np.radians(yaw)
    rotation = np.array([[np.cos(angle), 0, np.sin(angle)], [0, 1, 0], [-np.sin(angle), 0, np.cos(angle)]])
    p = p @ rotation.T
    normals = vertices['normal'] @ rotation.T
    if low is None: low = p.min(0)
    if high is None: high = p.max(0)
    scale = min((width-40)/(high[0]-low[0]), (height-40)/(high[1]-low[1]))
    p[:, 0] = (p[:, 0] - (low[0]+high[0])/2) * scale + width/2
    p[:, 1] = height/2 - (p[:, 1] - (low[1]+high[1])/2) * scale
    depth = np.full((height, width), -np.inf)
    pixels = np.full((height, width, 3), (35, 39, 44), dtype=np.uint8)
    for tri in triangles:
        garment = int(vertices['garment'][tri[0]]) if 'garment' in vertices.dtype.names else 0
        if 1 <= garment <= 4 and not (clothing_mask & (1 << (garment-1))): continue
        if garment == 3 and clothing_mask & 1: continue
        if garment == 4 and clothing_mask & 2: continue
        a, b, c = p[tri]
        x0, y0 = np.maximum(np.floor(np.min(p[tri, :2], 0)).astype(int), [0, 0])
        x1, y1 = np.minimum(np.ceil(np.max(p[tri, :2], 0)).astype(int), [width-1, height-1])
        if x0 > x1 or y0 > y1: continue
        denominator = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(denominator) < 1e-8: continue
        yy, xx = np.mgrid[y0:y1+1, x0:x1+1]
        u = ((b[1]-c[1])*(xx+0.5-c[0])+(c[0]-b[0])*(yy+0.5-c[1]))/denominator
        v = ((c[1]-a[1])*(xx+0.5-c[0])+(a[0]-c[0])*(yy+0.5-c[1]))/denominator
        w = 1-u-v
        z = u*a[2]+v*b[2]+w*c[2]
        mask = (u >= 0) & (v >= 0) & (w >= 0) & (z > depth[y0:y1+1, x0:x1+1])
        if not mask.any(): continue
        bary = np.stack((u[mask], v[mask], w[mask]), 1)
        uv = bary @ vertices['uv'][tri]
        tex = atlas[np.clip((uv[:, 1]*atlas.shape[0]).astype(int), 0, atlas.shape[0]-1),
                    np.clip((uv[:, 0]*atlas.shape[1]).astype(int), 0, atlas.shape[1]-1)]
        if garment == 5 and not (clothing_mask & 1):
            tex = tex.copy()
            tex[tex[:, 3] < 90] = (31, 36, 43, 255)
        if garment == 5:
            tex = tex.copy()
            body_y = (bary @ vertices['position'][tri])[:, 1]
            tex[(body_y > 84) & (body_y < 116)] = (31, 36, 43, 255)
        ys, xs = yy[mask], xx[mask]
        opaque = tex[:, 3] >= 90
        light = 0.65 + 0.35*np.abs((bary @ normals[tri])[:, 2])
        pixels[ys[opaque], xs[opaque]] = (tex[opaque, :3]*light[opaque, None]).clip(0, 255)
        depth[ys[opaque], xs[opaque]] = z[mask][opaque]
    return Image.fromarray(pixels)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    parser.add_argument('--preview', type=Path)
    parser.add_argument('--clothing-mask', type=int, default=15, choices=range(16))
    args = parser.parse_args()
    vertices, triangles, joints = load(args.directory/'character.mesh')
    print(f'Valid: {len(vertices)} vertices, {len(triangles)} triangles, {len(joints)} joints; normalized weights and valid UVs.')
    if 'garment' in vertices.dtype.names:
        groups = vertices['garment'][triangles[:, 0]]
        permanent = (groups == 0) | (groups == 5)
        for mask in range(16):
            visible = permanent.copy()
            for group in range(1, 5):
                if mask & (1 << (group-1)): visible |= groups == group
            if mask & 1: visible[groups == 3] = False
            if mask & 2: visible[groups == 4] = False
            assert visible[permanent].all()
            for group in range(1, 5):
                expected = bool(mask & (1 << (group-1))) and not (group == 3 and mask & 1) and not (group == 4 and mask & 2)
                assert (visible[groups == group] == expected).all()
        print('All 16 clothing combinations preserve the base layer and toggle only the corresponding garment.')
    report = json.loads((args.directory/'import.json').read_text(encoding='utf-8'))
    names = list(joints)
    for part in report.get('clothing', []):
        start, count = part['first_vertex'], part['vertex_count']
        clothing = vertices[start:start+count]
        assert len(clothing) == count and count > 0
        material = part['material']
        suffix = 'Spine2' if 'bra_sports2' in material else 'Arm' if 'top_' in material else 'Foot' if ('socks_' in material or 'shoes_' in material) else 'UpLeg'
        for side in ('Left', 'Right'):
            bone = suffix if suffix == 'Spine2' else side + suffix
            influence = np.where(clothing['joints'] == names.index(bone), clothing['weights'], 0).sum(axis=1)
            assert influence.max() > 0.05, (material, bone, 'missing clothing binding')
            angle = np.radians(30)
            rotation = np.array([[1, 0, 0], [0, np.cos(angle), -np.sin(angle)], [0, np.sin(angle), np.cos(angle)]])
            relative = clothing['position'] - joints[bone]
            displacement = ((relative @ rotation.T) - relative) * influence[:, None]
            assert np.isfinite(displacement).all() and np.linalg.norm(displacement, axis=1).max() > 0.01
        print(f'Clothing pose check: {material} follows both {suffix} joints.')
    if args.preview:
        atlas = np.array(Image.open(args.directory/'character.png').convert('RGBA'))
        front = preview(vertices, triangles, atlas, clothing_mask=args.clothing_mask)
        head = preview(vertices, triangles, atlas, low=np.array([-25, 135, -20]), high=np.array([25, 181, 20]), clothing_mask=args.clothing_mask)
        result = Image.new('RGB', (1280, 800))
        result.paste(front, (0, 0)); result.paste(head, (640, 0))
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        result.save(args.preview)
