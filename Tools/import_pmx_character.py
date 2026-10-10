"""Convert a PMX export to the project's existing animated character rig (no build)."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import struct
import numpy as np
from PIL import Image


class AiString(C.Structure):
    _fields_ = [('length', C.c_uint), ('data', C.c_char * 1024)]


class AiNode(C.Structure):
    pass


AiNode._fields_ = [('name', AiString), ('matrix', C.c_float * 16),
    ('parent', C.POINTER(AiNode)), ('count', C.c_uint), ('children', C.POINTER(C.POINTER(AiNode)))]


class AiScene(C.Structure):
    _fields_ = [('flags', C.c_uint), ('root', C.POINTER(AiNode))]


def reference_rig(path, dll):
    directory = os.add_dll_directory(str(dll.parent.resolve()))
    lib = C.CDLL(str(dll.resolve()))
    lib.aiCreatePropertyStore.restype = C.c_void_p
    lib.aiSetImportPropertyInteger.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
    lib.aiReleasePropertyStore.argtypes = [C.c_void_p]
    lib.aiImportFileFromMemoryWithProperties.argtypes = [C.c_void_p, C.c_uint, C.c_uint, C.c_char_p, C.c_void_p]
    lib.aiImportFileFromMemoryWithProperties.restype = C.POINTER(AiScene)
    lib.aiReleaseImport.argtypes = [C.POINTER(AiScene)]
    content = path.read_bytes()
    properties = lib.aiCreatePropertyStore()
    lib.aiSetImportPropertyInteger(properties, b'IMPORT_FBX_PRESERVE_PIVOTS', 0)
    scene = lib.aiImportFileFromMemoryWithProperties(content, len(content), 0, b'fbx', properties)
    lib.aiReleasePropertyStore(properties)
    if not scene: raise ValueError('Cannot load reference skeleton')
    nodes = {}
    def visit(ptr, parent, world):
        node = ptr.contents
        name = node.name.data.decode('utf-8').split(':')[-1]
        local = np.array(node.matrix).reshape(4, 4)
        world = world @ local
        nodes[name] = dict(parent=parent, local=local, world=world)
        for i in range(node.count): visit(node.children[i], name, world)
    visit(scene.contents.root, None, np.eye(4))
    lib.aiReleaseImport(scene)
    directory.close()
    return nodes


class PMX:
    def __init__(self, path):
        self.path = Path(path)
        self.f = self.path.open('rb')
        assert self.f.read(4) == b'PMX '
        self.version = self.read('f')
        config = self.f.read(self.read('B'))
        self.encoding = 'utf-16-le' if config[0] == 0 else 'utf-8'
        self.extra = config[1]
        self.sizes = dict(zip(('vertex', 'texture', 'material', 'bone', 'morph', 'rigid'), config[2:]))
        self.names = [self.string() for _ in range(4)]
        self.vertices = []
        for _ in range(self.read('i')):
            pos, normal, uv = self.read('3f'), self.read('3f'), self.read('2f')
            self.f.read(self.extra * 16)
            kind = self.read('B')
            if kind == 0:
                bones, weights = [self.index('bone')], [1.0]
            elif kind in (1, 3):
                bones = [self.index('bone') for _ in range(2)]
                w = self.read('f')
                weights = [w, 1-w]
                if kind == 3: self.f.read(36)
            elif kind in (2, 4):
                bones = [self.index('bone') for _ in range(4)]
                weights = self.read('4f')
            else: raise ValueError(f'Unsupported PMX skinning {kind}')
            self.read('f')
            self.vertices.append((pos, normal, uv, bones, weights))
        self.indices = [self.index('vertex') for _ in range(self.read('i'))]
        self.textures = [self.string() for _ in range(self.read('i'))]
        self.materials = []
        for _ in range(self.read('i')):
            name, english = self.string(), self.string()
            diffuse = self.read('4f')
            self.f.read(12 + 4 + 12)
            flags = self.read('B')
            self.f.read(16 + 4)
            texture = self.index('texture')
            self.index('texture'); self.read('B')
            if self.read('B') == 0: self.index('texture')
            else: self.read('B')
            self.string()
            self.materials.append(dict(name=name, diffuse=diffuse, texture=texture, count=self.read('i')))
        self.bones = []
        for _ in range(self.read('i')):
            name, english = self.string(), self.string()
            position, parent, layer, flags = self.read('3f'), self.index('bone'), self.read('i'), self.read('H')
            tail = self.index('bone') if flags & 1 else self.read('3f')
            if flags & 0x300: self.index('bone'); self.read('f')
            if flags & 0x400: self.f.read(12)
            if flags & 0x800: self.f.read(24)
            if flags & 0x2000: self.read('i')
            if flags & 0x20:
                self.index('bone'); self.read('i'); self.read('f')
                for _ in range(self.read('i')):
                    self.index('bone')
                    if self.read('B'): self.f.read(24)
            self.bones.append(dict(name=name, english=english, position=position, parent=parent, tail=tail))
        self.f.close()

    def read(self, fmt):
        values = struct.unpack('<' + fmt, self.f.read(struct.calcsize('<' + fmt)))
        return values[0] if len(values) == 1 else values

    def string(self):
        return self.f.read(self.read('i')).decode(self.encoding)

    def index(self, kind):
        size = self.sizes[kind]
        return self.read({1: 'B', 2: 'H', 4: 'I'}[size] if kind == 'vertex' else {1: 'b', 2: 'h', 4: 'i'}[size])


def convert(source, output, rig):
    root = source.parent
    models = [PMX(source), PMX(root / 'Outfit 00/model.pmx')]
    mapping = dict(cf_j_hips='Hips', cf_j_spine01='Spine', cf_j_spine02='Spine1',
        cf_j_spine03='Spine2', cf_j_neck='Neck', cf_j_head='Head')
    for suffix, side in [('L', 'Left'), ('R', 'Right')]:
        for src, dst in [('shoulder', 'Shoulder'), ('arm00', 'Arm'), ('forearm01', 'ForeArm'),
                         ('hand', 'Hand'), ('thigh00', 'UpLeg'), ('leg01', 'Leg'), ('leg03', 'Foot'), ('toes', 'ToeBase')]:
            mapping[f'cf_j_{src}_{suffix}'] = side + dst
        for src, dst in [('thumb', 'Thumb'), ('index', 'Index'), ('middle', 'Middle'), ('ring', 'Ring'), ('little', 'Pinky')]:
            for i in range(1, 4): mapping[f'cf_j_{src}{i:02d}_{suffix}'] = side + 'Hand' + dst + str(i)
    by_name = {b['name']: b for b in models[0].bones}
    mapping = {k: v for k, v in mapping.items() if k in by_name and v in rig}
    names = list(mapping.values())
    # PMX faces -Z; game faces +Z. Its left/right X convention is already correct.
    positions = np.array([v[0] for m in models for v in m.vertices])
    floor = positions[:, 1].min()
    scale = 180.0 / (positions[:, 1].max() - floor)
    def point(p): return (np.array(p) - [0, floor, 0]) * [scale, scale, -scale]
    bindings = [point(by_name[k]['position']) for k in mapping]
    metadata = {m['MaterialName']: m for m in json.loads((root / 'KK_MaterialData.json').read_text(encoding='utf-8-sig'))}
    pngs = {p.name: p for p in root.rglob('*.png')}
    overrides = {'cf_m_face_00': 'cf_m_face_00_MT_CT.png', 'cf_m_body': 'cf_m_body_MT_CT.png',
        'cf_m_hitomi_00_cf_Ohitomi_L02': 'cf_m_hitomi_00_cf_Ohitomi_L02_MT_00.png',
        'cf_m_hitomi_00_cf_Ohitomi_R02': 'cf_m_hitomi_00_cf_Ohitomi_R02_MT_00.png'}
    slots, images, report = {}, [], []
    def material_image(mat):
        name = mat['name']
        choices = [overrides.get(name, ''), name + '_MT_CT.png', name + '_MT_00.png', name + '_MT.png']
        path = next((pngs[n] for n in choices if n in pngs), None)
        if path:
            img = Image.open(path).convert('RGBA')
            if name == 'cf_m_body' and (root / 'cf_m_body_AM.png').is_file():
                # Exported clothing mask: red is visible skin in the fully dressed state.
                mask = Image.open(root / 'cf_m_body_AM.png').convert('RGB').getchannel('R')
                img.putalpha(mask.resize(img.size, Image.Resampling.NEAREST))
        else:
            color = mat['diffuse']
            meta = metadata.get(name, {})
            if 'hair' in name and meta.get('ShaderPropColorValues'):
                c = meta['ShaderPropColorValues'][0]
                color = [c['r'], c['g'], c['b'], 1]
            img = Image.new('RGBA', (8, 8), tuple(int(np.clip(v, 0, 1) * 255) for v in color))
        # Baked color textures retain the supplied colors; don't tint them again.
        key = str(path) if path else name
        if key not in slots:
            slots[key] = len(images)
            images.append(img.resize((508, 508), Image.Resampling.LANCZOS))
            report.append(dict(material=name, texture=str(path.relative_to(root)) if path else 'material color'))
        return slots[key]
    def mapped_weights(model, index):
        visited = set()
        while 0 <= index < len(model.bones) and index not in visited:
            visited.add(index)
            bone = model.bones[index]
            if bone['name'] in mapping: return names.index(mapping[bone['name']])
            index = bone['parent']
        return names.index('Hips')

    # Transfer skinning from the actual body surface: PMX skirt/helper bones do
    # not exist on the gameplay rig and otherwise collapse to a rigid hip weight.
    body = models[0]
    body_ids, start = [], 0
    for material in body.materials:
        if material['name'] == 'cf_m_body':
            body_ids.extend(body.indices[start:start + material['count']])
        start += material['count']
    body_ids = np.unique(body_ids)
    if not len(body_ids): raise ValueError('Body surface is required for clothing skinning')
    body_positions = np.array([body.vertices[i][0] for i in body_ids])
    body_weights = np.zeros((len(body_ids), len(names)))
    for row, idx in enumerate(body_ids):
        for bone, weight in zip(body.vertices[idx][3], body.vertices[idx][4]):
            if bone >= 0 and weight > 0:
                body_weights[row, mapped_weights(body, bone)] += weight
    body_weights /= body_weights.sum(axis=1, keepdims=True)

    def clothing_weights(model, ids):
        result = {}
        # Four nearby body vertices smooth seams without requiring extra packages.
        for begin in range(0, len(ids), 128):
            batch = ids[begin:begin + 128]
            positions = np.array([model.vertices[i][0] for i in batch])
            distances = ((positions[:, None, :] - body_positions[None, :, :]) ** 2).sum(axis=2)
            nearby = np.argpartition(distances, 3, axis=1)[:, :4]
            d = np.take_along_axis(distances, nearby, axis=1)
            blend = 1 / np.maximum(d, 1e-8)
            blend /= blend.sum(axis=1, keepdims=True)
            skin = (body_weights[nearby] * blend[:, :, None]).sum(axis=1)
            for idx, weights in zip(batch, skin):
                result[int(idx)] = {j: float(w) for j, w in enumerate(weights) if w > 1e-6}
        return result

    vertices, indices, outfit_report = [], [], []
    for model in models:
        def joint(index):
            visited = set()
            while 0 <= index < len(model.bones) and index not in visited:
                visited.add(index)
                bone = model.bones[index]
                if bone['name'] in mapping: return names.index(mapping[bone['name']])
                index = bone['parent']
            return names.index('Hips')
        joints = [joint(i) for i in range(len(model.bones))]
        offset = 0
        for mat in model.materials:
            faces = np.array(model.indices[offset:offset + mat['count']], dtype=np.uint32).reshape(-1, 3)
            offset += mat['count']
            name = mat['name']
            # Export includes hidden alternate clothing states; keep the fully dressed outfit.
            if any(s in name for s in ('Bonelyfans', 'shadowcast', 'namida', 'gageye', 'm_Mask', 'shoes_uwabaki')): continue
            if name in ('cf_m_top_tsyatu01 8910', 'cf_m_bot_Heteacher_d 9000',
                        'cf_m_shorts_03 9200', 'cf_m_shorts_03 9220', 'bra_sports2_mat 9100'): continue
            slot = material_image(mat)
            remap = {}
            used = np.unique(faces)
            is_clothing = model is models[1] and 'hair' not in name
            # Footwear stays permanent. 1..4 are upper/lower outerwear and underwear.
            garment = 5 if name == 'cf_m_body' else 1 if 'top_' in name else 2 if 'bot_' in name else 3 if 'bra_sports2' in name else 4 if 'shorts_' in name else 0
            transferred = clothing_weights(model, used) if is_clothing else {}
            first_vertex = len(vertices)
            linked_joints = set()
            for idx in used:
                pos, normal, uv, bones, weights = model.vertices[idx]
                combined = {}
                for bone, weight in zip(bones, weights):
                    if bone >= 0 and weight > 0:
                        j = joints[bone]
                        combined[j] = combined.get(j, 0) + weight
                if is_clothing: combined = transferred[int(idx)]
                if not combined: combined = {names.index('Hips'): 1.0}
                pairs = sorted(combined.items(), key=lambda p: -p[1])[:4]
                linked_joints.update(names[j] for j, _ in pairs)
                total = sum(w for _, w in pairs)
                js = [j for j, _ in pairs] + [0] * (4 - len(pairs))
                ws = [w / total for _, w in pairs] + [0.0] * (4 - len(pairs))
                remap[int(idx)] = len(vertices)
                # PMX UVs have a top-left origin, matching the atlas PNG rows.
                tex = ((slot % 8 * 512 + 2 + np.clip(uv[0], 0, 1) * 508) / 4096,
                       (slot // 8 * 512 + 2 + np.clip(uv[1], 0, 1) * 508) / 4096)
                vertices.append((*point(pos), *(np.array(normal) * [1, 1, -1]), *tex, *js, *ws, garment))
            # Reflection reverses winding.
            indices.extend(remap[int(i)] for face in faces for i in face[[0, 2, 1]])
            if is_clothing:
                outfit_report.append(dict(material=name, first_vertex=first_vertex,
                    vertex_count=len(vertices)-first_vertex, joints=sorted(linked_joints),
                    binding='body-surface skin weights', garment=garment))
    assert len(images) <= 64
    atlas = Image.new('RGBA', (4096, 4096), (0, 0, 0, 0))
    for i, img in enumerate(images):
        x, y = i % 8 * 512, i // 8 * 512
        # Two-pixel edge padding prevents neighboring tiles bleeding into each other.
        padded = np.pad(np.array(img), ((2, 2), (2, 2), (0, 0)), mode='edge')
        atlas.paste(Image.fromarray(padded), (x, y))
    output.mkdir(parents=True, exist_ok=True)
    atlas.save(output / 'character.png')
    with (output / 'character.mesh').open('wb') as f:
        f.write(b'TKCHAR03')
        f.write(struct.pack('<III', len(names), len(vertices), len(indices)))
        for name, pos in zip(names, bindings):
            raw = name.encode('ascii')
            f.write(struct.pack('<I', len(raw))); f.write(raw); f.write(struct.pack('<3f', *pos))
        for vertex in vertices: f.write(struct.pack('<8f4I4fI', *vertex))
        f.write(np.array(indices, dtype='<u4').tobytes())
    summary = dict(source=str(source), height_m=1.8, vertices=len(vertices), triangles=len(indices)//3,
        joints=len(names), materials=report, clothing=outfit_report, bounds_min=np.array(vertices)[:, :3].min(0).tolist(),
        bounds_max=np.array(vertices)[:, :3].max(0).tolist())
    (output / 'import.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in summary.items() if k not in ('materials', 'clothing')}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('--rig', type=Path)
    parser.add_argument('--assimp', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    model = PMX(args.source)
    if args.rig:
        rig = reference_rig(args.rig, args.assimp)
        if args.output:
            convert(args.source, args.output, rig)
            raise SystemExit(0)
        print({k: v['world'][:3, 3].tolist() for k, v in rig.items() if k in ('Hips','Head','LeftArm','RightArm','LeftForeArm','LeftHand','LeftFoot')})
        print([b['name'] for b in model.bones if b['name'].startswith('cf_j_')])
    else:
        print(json.dumps(dict(vertices=len(model.vertices), materials=model.materials,
            textures=model.textures, bones=model.bones), ensure_ascii=False, indent=2))
