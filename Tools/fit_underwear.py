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
    parser.add_argument('--thong-only', action='store_true', help='Preserve the bra and all outer equipment')
    args = parser.parse_args()
    directory = args.directory
    base, triangles, joints = load(directory / 'character.mesh')
    report = json.loads((directory / 'import.json').read_text(encoding='utf-8'))
    # Filter by garment instead of truncating: later assets (boots) must survive.
    removed = [4] if args.thong_only else [3, 4]
    keep = ~np.isin(base['garment'], removed)
    remap = np.cumsum(keep) - 1
    triangles = remap[triangles[np.all(keep[triangles], axis=1)]]
    base = base[keep].copy()
    builder = Outfit(base, joints)
    # Model units are centimeters: keep a small skin clearance to avoid z-fighting.
    allowance_cm = {3: .30, 4: .06}

    def panel(name, garment, low, high, planes=(), trim=False):
        builder.shell(name, triangles, garment,
                      [lambda p: p[1] - low, lambda p: high - p[1], *planes], allowance_cm[garment])
        part = builder.parts[-1]
        assert len(part['vertices']) > 0, name
        # Reserved swatches outside the body and tactical atlas regions.
        part['vertices']['uv'] = [(3200 if trim else 3136) / 4096, 2368 / 4096]

    if not args.thong_only:
        torso = [lambda p: 13.0 - p[0], lambda p: 13.0 + p[0]]
        panel('bra_underband', 3, 126, 129, torso, trim=True)
        panel('bra_front', 3, 129, 142, torso + [lambda p: p[2] + 2])
        panel('bra_back', 3, 129, 139, torso + [lambda p: -2 - p[2]])
        for side in (-1, 1):
            strap = [lambda p, s=side: s*p[0] - 7, lambda p, s=side: 10 - s*p[0]]
            panel(f'bra_front_strap_{side}', 3, 142, 154, strap + [lambda p: p[2] + 2])
            panel(f'bra_back_strap_{side}', 3, 139, 154, strap + [lambda p: -2 - p[2]])

    # Shallow V-shaped hip band and a shorter, contoured front panel.
    # Keep the crotch narrow without extending fabric down the inner thighs.
    def waist_height(p):
        return 103.0 + .10 * abs(p[0])

    panel('thong_waistband', 4, 103, 107,
          [lambda p: p[1] - waist_height(p),
           lambda p: waist_height(p) + 1.0 - p[1]], trim=True)
    panel('thong_front', 4, 92.5, 105,
          [lambda p: p[2] + 3, lambda p: waist_height(p) - p[1],
           lambda p: 1.6 + .30*(p[1]-92.5) + .028*(p[1]-92.5)**2 - p[0],
           lambda p: 1.6 + .30*(p[1]-92.5) + .028*(p[1]-92.5)**2 + p[0]])
    panel('thong_back', 4, 92.5, 105,
          [lambda p: -3 - p[2], lambda p: waist_height(p) - p[1],
           lambda p: max(.55, 1.6 - (p[1]-92.5)*.42) - p[0],
           lambda p: max(.55, 1.6 - (p[1]-92.5)*.42) + p[0]])

    with Image.open(directory / 'character.png') as source:
        atlas = source.convert('RGBA').copy()
    assert atlas.size == (4096, 4096)
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
        if args.thong_only and garment == 3:
            continue
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
    temporary = directory / 'character.underwear.tmp.png'
    atlas.save(temporary)
    temporary.replace(directory / 'character.png')
    report.update(vertices=len(vertices), triangles=len(indices))
    if args.thong_only:
        for old in report.get('fitted_underwear', {}).get('parts', []):
            if old['garment'] == 3:
                entry = dict(old)
                entry['first_vertex'] = int(remap[old['first_vertex']])
                parts.append(entry)
    report['fitted_underwear'] = dict(generator='Tools/fit_underwear.py',
        base_vertices=len(base), base_triangles=len(triangles),
        allowance_cm={str(k): v for k, v in allowance_cm.items()}, parts=parts,
        note='Clipped body surface with interpolated body skin weights; shared character palette.')
    (directory / 'import.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    load(directory / 'character.mesh')
    print(f'Saved fitted underwear: {len(vertices)-len(base)} vertices, {len(indices)-len(triangles)} triangles.')


if __name__ == '__main__':
    main()
