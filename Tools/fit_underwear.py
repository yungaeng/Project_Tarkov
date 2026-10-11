"""Fit the underwear to the character's body triangles and skin weights, without a build."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from create_tactical_outfit import Outfit, write_mesh
from validate_character import load


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    directory = args.directory
    base, triangles, joints = load(directory / 'character.mesh')
    report = json.loads((directory / 'import.json').read_text(encoding='utf-8'))
    # Generated parts are appended, preserving all existing tactical asset ranges.
    previous = report.get('fitted_underwear')
    if previous:
        base = base[:previous['base_vertices']]
        triangles = triangles[:previous['base_triangles']]
    assert not np.isin(base['garment'], [3, 4]).any(), 'Remove old imported underwear before fitting'
    builder = Outfit(base, joints)
    # Model units are centimeters: keep a small skin clearance to avoid z-fighting.
    allowance_cm = {3: .30, 4: .08}

    def panel(name, garment, low, high, planes=(), trim=False):
        builder.shell(name, triangles, garment,
                      [lambda p: p[1] - low, lambda p: high - p[1], *planes], allowance_cm[garment])
        part = builder.parts[-1]
        assert len(part['vertices']) > 0, name
        # Reserved swatches outside the body and tactical atlas regions.
        part['vertices']['uv'] = [(3200 if trim else 3136) / 4096, 2368 / 4096]

    torso = [lambda p: 13.0 - p[0], lambda p: 13.0 + p[0]]
    panel('bra_underband', 3, 126, 129, torso, trim=True)
    panel('bra_front', 3, 129, 142, torso + [lambda p: p[2] + 2])
    panel('bra_back', 3, 129, 139, torso + [lambda p: -2 - p[2]])
    for side in (-1, 1):
        strap = [lambda p, s=side: s*p[0] - 7, lambda p, s=side: 10 - s*p[0]]
        panel(f'bra_front_strap_{side}', 3, 142, 154, strap + [lambda p: p[2] + 2])
        panel(f'bra_back_strap_{side}', 3, 139, 154, strap + [lambda p: -2 - p[2]])

    panel('thong_waistband', 4, 98, 101, trim=True)
    # Front and rear share the crotch width and meet the continuous hip band.
    panel('thong_front', 4, 86, 98,
          [lambda p: p[2] + 3, lambda p: 1.8 + (p[1]-86)*.85 - p[0],
           lambda p: 1.8 + (p[1]-86)*.85 + p[0]])
    panel('thong_back', 4, 86, 98,
          [lambda p: -3 - p[2], lambda p: 1.8 + (p[1]-86)*.10 - p[0],
           lambda p: 1.8 + (p[1]-86)*.10 + p[0]])

    atlas = Image.open(directory / 'character.png').convert('RGBA')
    assert atlas.size == (4096, 4096)
    assert not ((base['uv'][:, 0] >= 3104/4096) & (base['uv'][:, 0] <= 3232/4096)
                & (base['uv'][:, 1] >= 2336/4096) & (base['uv'][:, 1] <= 2400/4096)).any()
    draw = ImageDraw.Draw(atlas)
    draw.rectangle((3104, 2336, 3167, 2399), fill='#18191d')
    draw.rectangle((3168, 2336, 3231, 2399), fill='#45464a')
    vv, tt, offset, parts = [base], [triangles], len(base), []
    output = directory / 'Underwear'
    output.mkdir(exist_ok=True)
    for part in builder.parts:
        v, t = part['vertices'], part['triangles']
        vv.append(v)
        tt.append(t + offset)
        parts.append(dict(name=part['name'], garment=part['garment'], first_vertex=offset,
                          vertex_count=len(v), triangles=len(t)))
        offset += len(v)
    for garment, name in [(3, 'bra'), (4, 'thong')]:
        pv, pt, offset = [], [], 0
        for part in builder.parts:
            if part['garment'] != garment:
                continue
            pv.append(part['vertices'])
            pt.append(part['triangles'] + offset)
            offset += len(part['vertices'])
        write_mesh(output / (name + '.mesh'), np.concatenate(pv), np.concatenate(pt), joints)
    vertices, indices = np.concatenate(vv), np.concatenate(tt)
    write_mesh(directory / 'character.mesh', vertices, indices, joints)
    atlas.save(directory / 'character.png')
    report.update(vertices=len(vertices), triangles=len(indices))
    report['fitted_underwear'] = dict(generator='Tools/fit_underwear.py',
        base_vertices=len(base), base_triangles=len(triangles),
        allowance_cm={str(k): v for k, v in allowance_cm.items()}, parts=parts,
        note='Clipped body surface with interpolated body skin weights; shared character palette.')
    (directory / 'import.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    load(directory / 'character.mesh')
    print(f'Saved fitted underwear: {len(vertices)-len(base)} vertices, {len(indices)-len(triangles)} triangles.')


if __name__ == '__main__':
    main()
