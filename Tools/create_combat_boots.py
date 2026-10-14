"""Generate original black combat boots, skinned mesh and atlas material swatches."""
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from validate_character import load
from create_tactical_outfit import Outfit, write_mesh


def main():
    directory = Path('Project_Tarkov/Project_Tarkov/Assets/Models/Player/KimGawon')
    v, t, joints = load(directory / 'character.mesh')
    # Re-running replaces only the boots and preserves all other equipment.
    keep = v['garment'] != 8
    remap = np.cumsum(keep) - 1
    t = remap[t[np.all(keep[t], axis=1)]]
    v = v[keep].copy()
    # Tuck both trouser cuffs inside the shaft, including the rear calf.
    # A radial cap makes repeated generation stable rather than shrinking again.
    for side in ('Left', 'Right'):
        x = float(joints[side+'Foot'][0])
        ids = np.flatnonzero((v['garment'] == 2) &
                            (v['position'][:, 1] < 32) & (v['position'][:, 0] * x > 0))
        p = v['position'][ids].copy()
        blend = np.clip((p[:, 1]-24)/8, 0, 1)
        rx, rz = 3.8 + 3.0*blend, 4.4 + 3.0*blend
        delta = p[:, [0, 2]] - [x, -3]
        radius = np.sqrt((delta[:, 0]/rx)**2 + (delta[:, 1]/rz)**2)
        p[:, [0, 2]] = [x, -3] + delta / np.maximum(radius[:, None], 1)
        v['position'][ids] = p

    def bind_ankles(vertices):
        # Cuff and boot shaft share exactly the same ankle blend. Nearest-body
        # weights differed at the rear and pulled the cloth through the leather.
        for side in ('Left', 'Right'):
            x = float(joints[side+'Foot'][0])
            ids = np.flatnonzero((vertices['position'][:, 0]*x > 0) &
                (((vertices['garment'] == 2) & (vertices['position'][:, 1] < 32)) |
                 (vertices['garment'] == 8)))
            leg = np.clip((vertices['position'][ids, 1]-10)/14, 0, 1)
            vertices['joints'][ids] = [list(joints).index(side+'Foot'), list(joints).index(side+'Leg'), 0, 0]
            vertices['weights'][ids] = np.column_stack([1-leg, leg, np.zeros(len(ids)), np.zeros(len(ids))])

    bind_ankles(v)
    # Recalculate normals for the reshaped trousers only.
    faces = t[np.all(v['garment'][t] == 2, axis=1)]
    normals = np.zeros((len(v), 3))
    p = v['position']
    face_normals = np.cross(p[faces[:, 1]]-p[faces[:, 0]], p[faces[:, 2]]-p[faces[:, 0]])
    for corner in range(3): np.add.at(normals, faces[:, corner], face_normals)
    ids = np.flatnonzero((v['garment'] == 2) & (np.linalg.norm(normals, axis=1) > 1e-8))
    v['normal'][ids] = normals[ids] / np.linalg.norm(normals[ids], axis=1)[:, None]
    outfit = Outfit(v, joints)
    colors = ['#17191c', '#090a0c', '#373a40', '#23262b']

    def material(index):
        outfit.parts[-1]['vertices']['uv'] = [(3328 + index * 64 + 32)/4096, 2368/4096]

    def rings(name, x, profile, shade, side):
        points, faces = [], []
        count = 32
        for y, rx, rz, z in profile:
            for a in np.linspace(0, 2*np.pi, count, endpoint=False):
                points.append([x + rx*np.sin(a), y, z + rz*np.cos(a)])
        for row in range(len(profile)-1):
            for i in range(count):
                a, b = row*count+i, row*count+(i+1)%count
                faces.extend([[a, b, b+count], [a, b+count, a+count]])
        points.extend([[x, profile[0][0], profile[0][3]], [x, profile[-1][0], profile[-1][3]]])
        for i in range(count):
            n = (i+1)%count
            faces.extend([[len(points)-2, n, i],
                          [len(points)-1, (len(profile)-1)*count+i, (len(profile)-1)*count+n]])
        outfit.add(name, points, faces, 8, 'rubber', rigid=side+'Foot' if shade == 1 else None)
        material(shade)

    for side in ('Left', 'Right'):
        x = float(joints[side+'Foot'][0])
        rings(side+' sole', x, [(2.0,4.8,11.7,1.5),(3.8,4.9,11.8,1.5)], 1, side)
        rings(side+' leather upper', x, [(3.8,4.75,11.6,1.5),(6.5,4.8,11.5,1.5),
              (9,4.5,9.5,0),(12,4.8,6.8,-2),(17,5.1,6.2,-3),(25,5.3,6.3,-3)], 0, side)
        rings(side+' padded collar', x, [(23.5,5.45,6.45,-3),(25.2,5.45,6.45,-3)], 3, side)
        for y in (12,14,16,18,20,22):
            # Crossed flat lace strips on the front of the shaft.
            for sign in (-1,1):
                z = float(np.interp(y, [12,17,25], [4.8,3.2,3.3])) + .12
                points = [[x-2.4,y-sign*.6,z],[x+2.4,y+sign*.6,z],
                          [x+2.4,y+sign*.6+.3,z+.05],[x-2.4,y-sign*.6+.3,z+.05]]
                outfit.add(side+' lace', points, [[0,1,2],[0,2,3]], 8, 'rubber')
                material(2)
        for z in (-7,-3,1,5,9):
            outfit.box(side+' tread', [x,2.0,z], [8.4,.7,1.8], 8, 'rubber', side+'Foot')
            material(1)
    vv, tt, offset = [], [], 0
    for part in outfit.parts:
        vv.append(part['vertices']); tt.append(part['triangles']+offset)
        offset += len(part['vertices'])
    boots, triangles = np.concatenate(vv), np.concatenate(tt)
    bind_ankles(boots)
    output = directory / 'Tactical'
    selected = v['garment'] == 2
    mapping = np.cumsum(selected)-1
    trouser_triangles = mapping[t[np.all(selected[t], axis=1)]]
    write_mesh(output/'trousers.mesh', v[selected], trouser_triangles, joints)
    write_mesh(output/'black_boots.mesh', boots, triangles, joints)
    write_mesh(directory/'character.mesh', np.concatenate([v,boots]), np.concatenate([t,triangles+len(v)]), joints)
    with Image.open(directory/'character.png') as source:
        atlas = source.convert('RGBA').copy()
    draw = ImageDraw.Draw(atlas)
    for i, color in enumerate(colors):
        draw.rectangle((3328+i*64,2336,3391+i*64,2399),fill=color)
    atlas_path = directory/'character.png'
    temporary = directory/'character.boots.tmp.png'
    atlas.save(temporary)
    temporary.replace(atlas_path)
    atlas.crop((3328,2336,3584,2400)).save(output/'black_boots_material.png')
    report = json.loads((directory/'import.json').read_text(encoding='utf-8'))
    report.update(vertices=len(v)+len(boots),triangles=len(t)+len(triangles))
    report['combat_boots'] = dict(generator='Tools/create_combat_boots.py',garment=8,
        slot=7,mesh='Tactical/black_boots.mesh',material='Tactical/black_boots_material.png',
        vertices=len(boots),triangles=len(triangles),atlas_region=[3328,2336,256,64])
    (directory/'import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'Saved black boots: {len(boots)} vertices, {len(triangles)} triangles.')


if __name__ == '__main__':
    main()
