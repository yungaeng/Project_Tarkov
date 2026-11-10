"""Attach the exported KoreanSurvivor head to the existing male game rig."""
import argparse
import io
import json
from pathlib import Path
from paths import source_workspace
from paths import repository_path
import struct

import numpy as np
from PIL import Image

from create_tactical_outfit import write_mesh
from validate_character import load, preview


def clip_neck(positions, normals, uv, indices):
    records = np.column_stack((positions, normals, uv))
    clipped = []
    for triangle in indices:
        polygon = list(records[triangle])
        for axis, sign, limit in ((1, 1, 149.5), (0, 1, -7), (0, -1, -7)):
            result = []
            for i, end in enumerate(polygon):
                start = polygon[i - 1]
                a, b = sign * start[axis] - limit, sign * end[axis] - limit
                if (a >= 0) != (b >= 0):
                    result.append(start + (end - start) * a / (a - b))
                if b >= 0:
                    result.append(end)
            polygon = result
        for i in range(1, len(polygon) - 1):
            clipped.extend((polygon[0], polygon[i], polygon[i + 1]))
    records = np.array(clipped)
    return records[:, :3], records[:, 3:6], records[:, 6:8], np.arange(len(records)).reshape(-1, 3)


class VRM:
    def __init__(self, path):
        raw = path.read_bytes()
        assert raw[:4] == b'glTF'
        length, kind = struct.unpack_from('<II', raw, 12)
        assert kind == 0x4E4F534A
        self.data = json.loads(raw[20:20 + length])
        size, kind = struct.unpack_from('<II', raw, 20 + length)
        assert kind == 0x004E4942
        self.buffer = raw[28 + length:28 + length + size]

    def accessor(self, index):
        a = self.data['accessors'][index]
        assert 'sparse' not in a
        view = self.data['bufferViews'][a['bufferView']]
        dtype = np.dtype({5121: 'u1', 5123: '<u2', 5125: '<u4', 5126: '<f4'}[a['componentType']])
        width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[a['type']]
        return np.ndarray((a['count'], width), dtype, self.buffer,
                          offset=view.get('byteOffset', 0) + a.get('byteOffset', 0),
                          strides=(view.get('byteStride', width * dtype.itemsize), dtype.itemsize)).copy()

    def texture(self, material):
        pbr = material['pbrMetallicRoughness']
        source = self.data['textures'][pbr['baseColorTexture']['index']]['source']
        view = self.data['bufferViews'][self.data['images'][source]['bufferView']]
        start = view.get('byteOffset', 0)
        image = Image.open(io.BytesIO(self.buffer[start:start + view['byteLength']])).convert('RGBA')
        pixels = np.array(image).astype(float)
        pixels *= pbr.get('baseColorFactor', [1, 1, 1, 1])
        return Image.fromarray(np.clip(pixels, 0, 255).astype('uint8'))


def attach(source, base, output):
    vrm = VRM(source)
    vertices, triangles, joints = load(base / 'character.mesh')
    assert Image.open(base / 'character.png').size == (4096, 4096), 'Use the original male asset as the base'
    # In this male asset garment zero contains only the previously fitted head.
    old_head = vertices['garment'] == 0
    assert old_head.any() and vertices['position'][old_head, 1].min() > 150
    neck_faces = ((vertices['garment'][triangles[:, 0]] == 5)
                  & (vertices['position'][triangles, 1].min(axis=1) > 150)
                  & (vertices['position'][triangles, 1].max(axis=1) > 153)
                  & (np.abs(vertices['position'][triangles, 0]).max(axis=1) < 8))
    keep_faces = ~(old_head[triangles].any(axis=1) | neck_faces)
    retained_ids, remapped = np.unique(triangles[keep_faces], return_inverse=True)
    retained = vertices[retained_ids].copy()
    retained['uv'][:, 0] *= 0.5
    parts = [retained]
    faces = [remapped.reshape(-1, 3)]
    atlas = Image.new('RGBA', (8192, 4096))
    atlas.paste(Image.open(base / 'character.png').convert('RGBA'), (0, 0))
    head_index = list(joints).index('Head')
    primitives = [p for m in vrm.data['meshes'] for p in m['primitives']]
    hair = next(p for p in primitives if p['material'] == 10)
    hair_positions = vrm.accessor(hair['attributes']['POSITION'])
    hair_ids = vrm.accessor(hair['indices']).ravel()
    scale = 85.0
    offset = np.array([0, vertices['position'][old_head, 1].max() - scale * hair_positions[hair_ids, 1].max(),
                       joints['Head'][2]])
    report = []
    total = len(retained)
    for slot, primitive in enumerate(p for p in primitives if p['material'] != 9):
        material = vrm.data['materials'][primitive['material']]
        attributes = primitive['attributes']
        positions = vrm.accessor(attributes['POSITION']) * scale + offset
        normals = vrm.accessor(attributes['NORMAL'])
        texcoords = vrm.accessor(attributes['TEXCOORD_0'])
        indices = vrm.accessor(primitive['indices']).reshape(-1, 3)
        if primitive['material'] == 7:
            # The body material also contains the scalp and neck. Retain only
            # triangles above the old head's neck seam.
            positions, normals, texcoords, indices = clip_neck(positions, normals, texcoords, indices)
            # Fit the lower neck over the existing neck, ending inside the collar.
            blend = np.clip((158.5 - positions[:, 1]) / 5.0, 0, 1)
            positions[:, 0] *= 1 + blend * 0.18
            positions[:, 2] = offset[2] + (positions[:, 2] - offset[2]) * (1 + blend * 0.18)
            positions[:, 2] += blend * 4.2
            radial = positions[:, [0, 2]] - [0, offset[2]]
            radius = np.linalg.norm(radial / [4.4, 3.6], axis=1)
            lower = np.clip((155.5 - positions[:, 1]) / 1.5, 0, 1)
            contraction = 1 - lower * (1 - 1 / np.maximum(radius, 1))
            positions[:, [0, 2]] = radial * contraction[:, None] + [0, offset[2]]
            # Use the original neck's skin sample on the collar insertion, avoiding
            # the tank-top pixels carried by the source body's lower UV region.
            sample = texcoords[np.argmin(np.abs(positions[:, 1] - 156))].copy()
            texcoords[positions[:, 1] < 154] = sample
        ids, local = np.unique(indices, return_inverse=True)
        part = np.zeros(len(ids), dtype=vertices.dtype)
        part['position'] = positions[ids]
        part['normal'] = normals[ids]
        part['normal'] /= np.maximum(np.linalg.norm(part['normal'], axis=1, keepdims=True), 1e-8)
        uv = texcoords[ids]
        repeats = np.maximum(1, np.ceil(uv.max(axis=0))).astype(int)
        assert uv.min() >= 0
        uv = uv / repeats
        x, y = 4096 + slot % 4 * 1024, slot // 4 * 1024
        part['uv'] = (np.clip(uv, 0, 1) * 1020 + [x + 2, y + 2]) / [8192, 4096]
        part['joints'][:, 0] = head_index
        part['weights'][:, 0] = 1
        if primitive['material'] == 7:
            head_weight = np.clip((part['position'][:, 1] - 151) / 7, 0, 1)
            part['joints'][:, 1] = list(joints).index('Neck')
            part['weights'][:, 0] = head_weight
            part['weights'][:, 1] = 1 - head_weight
        texture = vrm.texture(material)
        texture = Image.fromarray(np.tile(np.array(texture), (repeats[1], repeats[0], 1)))
        texture = texture.resize((1020, 1020), Image.Resampling.LANCZOS)
        atlas.paste(Image.fromarray(np.pad(np.array(texture), ((2, 2), (2, 2), (0, 0)), mode='edge')), (x, y))
        parts.append(part)
        faces.append(local.reshape(-1, 3) + total)
        total += len(part)
        report.append({'material': material['name'], 'vertices': len(part), 'triangles': len(indices)})
    result = np.concatenate(parts)
    result_faces = np.concatenate(faces).astype('<u4')
    output.mkdir(parents=True, exist_ok=True)
    write_mesh(output / 'character.mesh', result, result_faces, joints)
    atlas.save(output / 'character.png')
    load(output / 'character.mesh')
    # Verify all retained positions, normals and skinning byte-for-byte.
    for field in ('position', 'normal', 'joints', 'weights', 'garment'):
        assert np.array_equal(result[field][:len(retained)], vertices[field][retained_ids])
    assert np.array_equal(np.array(atlas)[:, :4096], np.array(Image.open(base / 'character.png').convert('RGBA')))
    (source_workspace(output) / 'head_attachment.json').write_text(json.dumps({
        'source': str(source), 'scale': scale, 'offset': offset.tolist(),
        'removed_head_vertices': int(old_head.sum()), 'retained_vertices': len(retained),
        'replaced_neck_triangles': int(neck_faces.sum()),
        'head_joint': 'Head', 'materials': report,
    }, indent=2), encoding='utf-8')
    preview(result, result_faces, np.array(atlas), width=600, height=700,
            low=np.array([-27, 139, -25]), high=np.array([27, 185, 25]),
            clothing_mask=31).save(source_workspace(output) / 'head_preview.png')
    print(json.dumps({'vertices': len(result), 'triangles': len(result_faces), 'materials': report}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=repository_path)
    parser.add_argument('base', type=repository_path)
    parser.add_argument('output', type=repository_path)
    args = parser.parse_args()
    attach(args.source, args.base, args.output)
