"""Fit a short black trunk to the male character using its body skin weights."""
import argparse
import json
from pathlib import Path
from paths import source_workspace
from paths import repository_path

import numpy as np
from PIL import Image, ImageDraw

from create_tactical_outfit import Outfit, write_mesh
from validate_character import load


def fit(directory):
    mesh = directory / 'character.mesh'
    vertices, triangles, joints = load(mesh)
    keep = vertices['garment'] != 4
    triangles = (np.cumsum(keep) - 1)[triangles[keep[triangles].all(axis=1)]]
    vertices = vertices[keep]
    outfit = Outfit(vertices, joints)
    hip = joints['LeftUpLeg'][1]
    low, high = hip - 12, hip + 10
    outfit.shell('black_trunks', triangles, 4,
                 [lambda p: p[1] - low, lambda p: high - p[1]], .35)
    part = outfit.parts[-1]
    cloth, faces = part['vertices'], part['triangles']
    with Image.open(directory / 'character.png') as source:
        atlas = source.convert('RGBA')
    # Front and back panels share a dark fabric swatch with stitched hems.
    for front in (False, True):
        tile = Image.new('RGB', (512, 256), '#262629')
        draw = ImageDraw.Draw(tile)
        draw.rectangle((0, 0, 511, 39), fill='#1e1e20')
        for y in (4, 36, 242, 247):
            draw.line((0, y, 511, y), fill='#444447')
        if front:
            for sign in (-1, 1):
                draw.line([(256 + sign * x, y) for x, y in
                           [(77, 40), (75, 90), (65, 141), (44, 182), (13, 211)]],
                          fill='#19191c', width=3)
        atlas.paste(tile, (2048 + 512 * int(front), 2560))
    pos = cloth['position']
    front = pos[:, 2] > joints['Hips'][2]
    cloth['uv'][:, 0] = (2049 + front * 512 + np.clip(.5 + pos[:, 0]/42, 0, 1)*509)/4096
    cloth['uv'][:, 1] = (2561 + np.clip((high-pos[:, 1])/(high-low), 0, 1)*253)/4096
    dest = source_workspace(directory) / 'Underwear'
    dest.mkdir(exist_ok=True)
    write_mesh(dest/'black_trunks.mesh', cloth, faces, joints)
    write_mesh(mesh, np.concatenate([vertices, cloth]), np.concatenate([triangles, faces+len(vertices)]), joints)
    atlas.save(directory/'character.png')
    validated, faces, _ = load(mesh)
    report_path = source_workspace(directory) / 'import.json'
    report = json.loads(report_path.read_text(encoding='utf-8'))
    report.update(vertices=len(validated), triangles=len(faces), underpants='Underwear/black_trunks.mesh')
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=repository_path)
    fit(parser.parse_args().directory)
