"""Create fitted, skinned tactical clothing from the existing character (no build).

All geometry and the original camouflage approximation are generated locally.
The reference photo is a design reference, not a texture source.
"""
import argparse
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from validate_character import load


def camouflage(size=2048):
    rng = np.random.default_rng(741)
    image = Image.new('RGB', (size, size), '#9e9870')
    draw = ImageDraw.Draw(image)
    # Interlocking organic macro patches and smaller light/dark islands.
    for count, radius, color in [(240, 54, '#73794e'), (200, 44, '#b1a47a'),
                                  (190, 33, '#68734a'), (150, 22, '#746044'),
                                  (190, 12, '#d1c4a0'), (160, 9, '#454d35')]:
        for _ in range(count):
            center = rng.uniform(0, size, 2)
            stretch = rng.uniform(.7, 2.3)
            angles = np.linspace(0, 2*np.pi, 20, endpoint=False)
            radii = radius * rng.uniform(.55, 1.4, len(angles))
            points = center + np.column_stack((np.cos(angles)*radii*stretch, np.sin(angles)*radii))
            for dx in (-size, 0, size):
                for dy in (-size, 0, size):
                    draw.polygon([tuple(p + [dx, dy]) for p in points], fill=color)
    image = image.filter(ImageFilter.GaussianBlur(1.1))
    pixels = np.asarray(image).astype(float)
    yy, xx = np.indices((size, size))
    weave = np.where(xx % 4 == 0, -3, 0) + np.where(yy % 4 == 0, 3, 0)
    pixels += weave[:, :, None] + rng.normal(0, 1.3, (size, size, 1))
    return Image.fromarray(np.uint8(np.clip(pixels, 0, 255))).convert('RGBA')


def write_mesh(path, vertices, triangles, joints):
    with path.open('wb') as f:
        f.write(b'TKCHAR04')
        f.write(struct.pack('<III', len(joints), len(vertices), triangles.size))
        for name, position in joints.items():
            raw = name.encode('ascii')
            f.write(struct.pack('<I', len(raw)) + raw + struct.pack('<3f', *position))
        f.write(vertices.tobytes())
        f.write(np.asarray(triangles, dtype='<u4').tobytes())


class Outfit:
    def __init__(self, base, joints):
        self.base = base
        self.joints = joints
        self.names = list(joints)
        self.parts = []
        self.body = base[base['garment'] == 5]

    def skin(self, positions):
        ids, weights = [], []
        for start in range(0, len(positions), 128):
            distances = ((positions[start:start+128, None] - self.body['position'][None])**2).sum(2)
            nearest = np.argpartition(distances, 3, axis=1)[:, :4]
            blend = 1 / np.maximum(np.take_along_axis(distances, nearest, 1), .01)
            blend /= blend.sum(1, keepdims=True)
            for neighbours, factors in zip(nearest, blend):
                skin = np.zeros(len(self.names))
                for idx, factor in zip(neighbours, factors):
                    np.add.at(skin, self.body['joints'][idx], self.body['weights'][idx]*factor)
                best = np.argsort(skin)[-4:][::-1]
                ids.append(best)
                weights.append(skin[best]/skin[best].sum())
        return np.asarray(ids), np.asarray(weights)

    def add(self, name, positions, triangles, garment, material='camo', rigid=None, skin=None):
        p = np.asarray(positions, float)
        t = np.asarray(triangles, int).reshape(-1, 3)
        normals = np.zeros_like(p)
        face = np.cross(p[t[:, 1]]-p[t[:, 0]], p[t[:, 2]]-p[t[:, 0]])
        keep = np.linalg.norm(face, axis=1) > 1e-6
        t, face = t[keep], face[keep]
        for i in range(3): np.add.at(normals, t[:, i], face)
        normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-9)
        v = np.zeros(len(p), dtype=self.base.dtype)
        v['position'], v['normal'], v['garment'] = p, normals, garment
        if garment == 6 and rigid is None: rigid = 'Spine2'
        if rigid:
            v['joints'][:, 0], v['weights'][:, 0] = self.names.index(rigid), 1
        elif skin is not None:
            v['joints'], v['weights'] = skin
        else:
            v['joints'], v['weights'] = self.skin(p)
        # Duplicate UV seams per triangle for axis-projected cloth, not the body UVs.
        v = v[t.flatten()].copy()
        if material == 'camo':
            for i, tri in enumerate(t):
                axes = [0, 1] if abs(face[i, 2]) >= abs(face[i, 1]) else [0, 2]
                uv = (p[tri][:, axes] + [85, 0 if axes[1] == 1 else 85]) / [180, 190 if axes[1] == 1 else 180]
                assert (uv >= 0).all() and (uv <= 1).all()
                v['uv'][i*3:i*3+3] = (uv * [2044, 2044] + [2, 2050]) / 4096
        else:
            swatch = {'webbing': 0, 'rubber': 1, 'metal': 2, 'stitch': 3}[material]
            v['uv'] = [(2176 + swatch*256)/4096, 2176/4096]
        t = np.arange(len(v), dtype=np.uint32).reshape(-1, 3)
        self.parts.append(dict(name=name, garment=garment, vertices=v, triangles=t))

    def shell(self, name, triangles, garment, planes, allowance):
        body_tri = triangles[np.all(self.base['garment'][triangles] == 5, axis=1)]
        positions, normals = self.base['position'], self.base['normal']
        # Clip precisely at the hems rather than deleting crossing triangles.
        points, faces, weights, ids = [], [], [], []
        for tri in body_tri:
            poly = []
            for idx in tri:
                skin = np.zeros(len(self.names))
                np.add.at(skin, self.base['joints'][idx], self.base['weights'][idx])
                poly.append((positions[idx].astype(float), normals[idx].astype(float), skin))
            for plane in planes:
                clipped = []
                for a, b in zip(poly, poly[1:]+poly[:1]):
                    da, db = plane(a[0]), plane(b[0])
                    if da >= 0: clipped.append(a)
                    if (da >= 0) != (db >= 0):
                        s = da/(da-db)
                        clipped.append(tuple(x+(y-x)*s for x, y in zip(a, b)))
                poly = clipped
                if not poly: break
            if len(poly) < 3: continue
            first = len(points)
            for position, normal, skin in poly:
                normal /= max(np.linalg.norm(normal), 1e-8)
                points.append(position + normal * allowance)
                best = np.argsort(skin)[-4:][::-1]
                ids.append(best); weights.append(skin[best]/skin[best].sum())
            faces.extend((first, first+i, first+i+1) for i in range(1, len(poly)-1))
        self.add(name, points, faces, garment, skin=(np.asarray(ids), np.asarray(weights)))

    def box(self, name, center, size, garment, material='camo', rigid=None):
        p = np.array([[-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],
                      [-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]])*np.asarray(size)/2 + center
        faces = [[0,3,2,1],[4,5,6,7],[0,1,5,4],[3,7,6,2],[0,4,7,3],[1,2,6,5]]
        # Separate face normals give crisp cloth panels and hard hardware edges.
        positions, triangles = [], []
        for face in faces:
            start = len(positions); positions.extend(p[face])
            triangles.extend([[start,start+1,start+2],[start,start+2,start+3]])
        self.add(name, positions, triangles, garment, material, rigid)

    def band(self, name, center, radii, height, garment, material='webbing', rigid=None):
        p, t = [], []
        for y in (-height/2, height/2):
            for a in np.linspace(0, 2*np.pi, 49):
                p.append(np.array(center) + [radii[0]*np.sin(a), y, radii[1]*np.cos(a)])
        for i in range(48): t.extend([[i,i+1,i+50],[i,i+50,i+49]])
        self.add(name, p, t, garment, material, rigid)

    def helmet(self):
        p, t = [], []
        # High-cut shell: front brow stays above the eyes, sides clear the ears.
        for ring in range(17):
            for i in range(65):
                a = i*2*np.pi/64
                bottom = 1.57 - .20*max(np.cos(a), 0) - .12*abs(np.sin(a))
                phi = .025 + (bottom-.025)*ring/16
                p.append([12.9*np.sin(phi)*np.sin(a), 168+13.8*np.cos(phi),
                          -2.2+15.2*np.sin(phi)*np.cos(a)])
        for r in range(16):
            for i in range(64):
                a=r*65+i; t.extend([[a,a+65,a+1],[a+1,a+65,a+66]])
        self.add('helmet_shell', p, t, 7, rigid='Head')
        # Cover the tiny pole opening with a fan.
        self.add('helmet_crown', [[0,181.8,-2.2]]+p[:65],
                 [[0,i+1,i+2] for i in range(64)], 7, rigid='Head')
        for side in (-1, 1):
            self.box('helmet_side_rail', [side*12.5,171,-1], [1.3,2.2,11], 7, 'rubber', 'Head')
            self.box('helmet_velcro', [side*9.7,177,-2], [2,1.4,6], 7, 'webbing', 'Head')
        self.box('helmet_nvg_mount', [0,173,13.0], [4,4,.9], 7, 'rubber', 'Head')
        self.box('helmet_mount_insert', [0,173,13.55], [1.9,2.2,.25], 7, 'metal', 'Head')
        # Rear retention strap and an under-chin strap, both rigid to the head.
        for side in (-1, 1):
            self.box('helmet_retention', [side*8.2,159,-.8], [.8,18,.55], 7, 'webbing', 'Head')
        self.box('helmet_chin_cup', [0,150,2], [15.5,1.2,2], 7, 'rubber', 'Head')

    def build(self, triangles):
        self.shell('combat_shirt', triangles, 1,
                   [lambda p:p[1]-113.5, lambda p:58-abs(p[0]),
                    lambda p:max(148.5-p[1],abs(p[0])-5.5)], 1.05)
        self.shell('combat_trousers', triangles, 2,
                   [lambda p:117-p[1],lambda p:p[1]-12.5], 1.35)
        self.band('trouser_belt', [0,115,-1], [12,9], 2.3, 2)
        self.box('belt_buckle', [0,115,8.6], [3,2.5,.8], 2, 'metal')
        self.band('shirt_collar', [0,149,-2], [5.3,5], 3, 1)
        self.box('shirt_zip', [0,143,10.7], [.5,6,.4], 1, 'webbing')
        for side in (-1, 1):
            self.box('trouser_cargo_pocket', [side*14.5,87,-1.6], [2.7,12,9], 2)
            self.box('cargo_flap', [side*15.9,91,-1.6], [.5,3.2,9.6], 2, 'webbing')
            self.box('knee_reinforcement', [side*8.4,55,3.0], [7,9,1.5], 2, 'webbing')
            self.box('sleeve_pocket', [side*22,146,2.1], [7,3.7,1.6], 1)
        # A short chest carrier: its entire geometry ends above y=127, leaving
        # the abdomen between the shirt hem and the lower rib cage uncovered.
        self.box('carrier_front', [0,136.5,12.8], [22,18,3.2], 6)
        self.box('carrier_back', [0,137,-9.8], [22,18,3], 6)
        for side in (-1, 1):
            self.box('carrier_side', [side*11,136,1], [2,7,21], 6)
            self.box('shoulder_strap', [side*7.3,149,1], [3.1,2.2,24], 6, 'webbing')
            self.box('front_strap', [side*7.3,146,12.8], [3,7,1.7], 6, 'webbing')
            self.box('strap_buckle', [side*7.3,147,14], [3.4,2.1,.7], 6, 'rubber')
        for x in (-6.6, 0, 6.6):
            self.box('magazine_pouch', [x,132.5,16.3], [5.7,9.5,3.8], 6)
            self.box('pouch_flap', [x,136.5,18.5], [5.9,2.1,.55], 6, 'webbing')
        for y in (141.1,143.4):
            self.box('molle_webbing', [0,y,14.5], [18,.85,.3], 6, 'webbing')
            for x in (-7.5,-4.5,-1.5,1.5,4.5,7.5):
                self.box('molle_stitch', [x,y,14.7], [.13,.85,.12], 6, 'stitch')
        self.helmet()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    directory = args.directory
    base, triangles, joints = load(directory/'character.mesh')
    report = json.loads((directory/'import.json').read_text(encoding='utf-8'))
    previous = report.get('tactical_outfit')
    if previous:
        base = base[:previous['base_vertices']]
        triangles = triangles[:previous['base_triangles']]
    builder = Outfit(base, joints)
    builder.build(triangles)
    atlas = Image.open(directory/'character.png').convert('RGBA')
    assert atlas.size == (4096,4096)
    assert base['uv'][:,1].max() < .5, 'Reserved atlas region is already occupied'
    tile = camouflage()
    atlas.paste(tile,(0,2048))
    draw = ImageDraw.Draw(atlas)
    for i,color in enumerate(['#666448','#30352d','#8c8972','#b4a580']):
        draw.rectangle((2048+i*256,2048,2303+i*256,2303),fill=color)
    output = directory/'Tactical'
    output.mkdir(exist_ok=True)
    tile.save(output/'multicam.png')
    all_vertices, all_triangles = [base], [triangles]
    part_report = []
    offset = len(base)
    for part in builder.parts:
        v,t=part['vertices'],part['triangles']
        all_vertices.append(v); all_triangles.append(t+offset)
        part_report.append(dict(name=part['name'],garment=part['garment'],first_vertex=offset,
                                vertex_count=len(v),triangles=len(t)))
        offset += len(v)
    combined = np.concatenate(all_vertices)
    combined_triangles = np.concatenate(all_triangles)
    # Separate editable skinned assets use the same atlas and rig as the character.
    for garment,name in [(1,'shirt'),(2,'trousers'),(6,'vest'),(7,'helmet')]:
        parts=[p for p in builder.parts if p['garment']==garment]
        vv,tt,offset=[],[],0
        for part in parts:
            vv.append(part['vertices']); tt.append(part['triangles']+offset)
            offset += len(part['vertices'])
        write_mesh(output/(name+'.mesh'),np.concatenate(vv),np.concatenate(tt),joints)
    write_mesh(directory/'character.mesh',combined,combined_triangles,joints)
    atlas.save(directory/'character.png')
    report.update(vertices=len(combined),triangles=len(combined_triangles),
                  bounds_min=combined['position'].min(0).tolist(),bounds_max=combined['position'].max(0).tolist())
    report['tactical_outfit']=dict(generator='Tools/create_tactical_outfit.py',base_vertices=len(base),
        base_triangles=len(triangles),garments={'1':'shirt','2':'trousers','6':'vest','7':'helmet'},
        texture='Tactical/multicam.png',atlas_region=[0,2048,2048,2048],parts=part_report,
        note='Original procedural MultiCam-style approximation; no ballistic gameplay effect.')
    (directory/'import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'Created tactical outfit: {len(combined)-len(base)} added vertices; {len(combined_triangles)-len(triangles)} added triangles.')


if __name__ == '__main__': main()
