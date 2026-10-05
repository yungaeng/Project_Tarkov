"""Create a skeleton-only looping idle from the project's existing binary FBX assets.

Preserves FBX transforms, pre-rotations, skeleton names and animation connections.
Only animation curves are authored; meshes, materials and embedded media are omitted.
No Blender or Autodesk SDK installation is required.
"""
from pathlib import Path
from dataclasses import dataclass
import argparse
import math
import struct
import zlib

FBX_SECOND = 46186158000


@dataclass
class Property:
    kind: str
    raw: bytes

    def value(self):
        if self.kind in "SR":
            return self.raw[4:]
        return struct.unpack("<" + {"Y": "h", "C": "?", "I": "i", "L": "q", "F": "f", "D": "d"}[self.kind], self.raw)[0]

    def array(self):
        count, encoding, size = struct.unpack("<III", self.raw[:12])
        payload = self.raw[12:12 + size]
        if encoding == 1:
            payload = zlib.decompress(payload)
        return struct.unpack("<" + str(count) + {"f": "f", "d": "d", "l": "q", "i": "i", "b": "?", "c": "b"}[self.kind], payload)


@dataclass
class Node:
    name: str
    props: list
    children: list
    terminated: bool = False

    def child(self, name):
        return next((node for node in self.children if node.name == name), None)


class FBX:
    def __init__(self, path):
        self.raw = Path(path).read_bytes()
        if not self.raw.startswith(b"Kaydara FBX Binary"):
            raise ValueError("Expected binary FBX")
        version = struct.unpack_from("<I", self.raw, 23)[0]
        self.header_format = "<QQQB" if version >= 7500 else "<IIIB"
        self.header_size = struct.calcsize(self.header_format)
        self.nodes = []
        offset = 27
        while any(self.raw[offset:offset + self.header_size]):
            node, offset = self.read_node(offset)
            self.nodes.append(node)
        self.footer = self.raw[offset + self.header_size:]

    def read_node(self, offset):
        end, count, size, name_size = struct.unpack_from(self.header_format, self.raw, offset)
        offset += self.header_size
        name = self.raw[offset:offset + name_size].decode("utf-8")
        offset += name_size
        props = []
        for _ in range(count):
            kind = chr(self.raw[offset])
            offset += 1
            if kind in "SR":
                length = 4 + struct.unpack_from("<I", self.raw, offset)[0]
            elif kind in "fdlibc":
                length = 12 + struct.unpack_from("<I", self.raw, offset + 8)[0]
            else:
                length = {"Y": 2, "C": 1, "I": 4, "L": 8, "F": 4, "D": 8}[kind]
            props.append(Property(kind, self.raw[offset:offset + length]))
            offset += length
        children = []
        terminated = False
        while offset < end:
            if not any(self.raw[offset:offset + self.header_size]):
                offset += self.header_size
                terminated = True
                break
            node, offset = self.read_node(offset)
            children.append(node)
        if offset != end:
            raise ValueError("Invalid FBX node boundary")
        return Node(name, props, children, terminated), end

    def encode_node(self, node, offset):
        name = node.name.encode("utf-8")
        props = b"".join(p.kind.encode("ascii") + p.raw for p in node.props)
        cursor = offset + self.header_size + len(name) + len(props)
        children = []
        for child in node.children:
            encoded = self.encode_node(child, cursor)
            children.append(encoded)
            cursor += len(encoded)
        if node.terminated or node.children:
            children.append(bytes(self.header_size))
            cursor += self.header_size
        return struct.pack(self.header_format, cursor, len(node.props), len(props), len(name)) + name + props + b"".join(children)

    def write(self, path):
        output = bytearray(self.raw[:27])
        for node in self.nodes:
            output.extend(self.encode_node(node, len(output)))
        output.extend(bytes(self.header_size))
        output.extend(self.footer)
        Path(path).write_bytes(output)

    def node(self, name):
        return next(node for node in self.nodes if node.name == name)


def scalar(kind, value):
    if kind == "S":
        value = value.encode("utf-8") if isinstance(value, str) else value
        return Property(kind, struct.pack("<I", len(value)) + value)
    return Property(kind, struct.pack("<" + {"L": "q", "I": "i", "D": "d"}[kind], value))


def array(kind, values):
    packed = struct.pack("<" + str(len(values)) + {"l": "q", "f": "f", "i": "i"}[kind], *values)
    compressed = zlib.compress(packed)
    return Property(kind, struct.pack("<III", len(values), 1, len(compressed)) + compressed)


def short_name(node):
    return node.props[1].value().split(b"\x00")[0].split(b":")[-1].decode("utf-8")


def transform_properties(model):
    props = model.child("Properties70")
    if not props:
        return {}
    return {p.props[0].value().decode(): [v.value() for v in p.props[4:]]
            for p in props.children if p.name == "P"}


def generate(source, bind, output):
    fbx, base = FBX(source), FBX(bind)
    objects = fbx.node("Objects")
    base_models = {short_name(n): n for n in base.node("Objects").children if n.name == "Model"}
    lookup = {n.props[0].value(): n for n in objects.children}
    connections = fbx.node("Connections")
    parents = {}
    for connection in connections.children:
        values = [p.value() for p in connection.props]
        if len(values) >= 3:
            parents.setdefault(values[1], []).append((values[2], values[3] if len(values) > 3 else b""))
    frames = 106
    times = [round(i * FBX_SECOND / 30) for i in range(frames)]
    authored = 0
    authored_ids = set()
    curve_nodes = set()
    for curve in objects.children:
        if curve.name != "AnimationCurve":
            continue
        links = parents.get(curve.props[0].value(), [])
        target = None
        for curve_node, axis in links:
            for model_id, property_name in parents.get(curve_node, []):
                model = lookup.get(model_id)
                if model and model.name == "Model" and property_name in (b"Lcl Translation", b"Lcl Rotation", b"Lcl Scaling"):
                    target = (model, property_name.decode(), axis.decode().split("|")[-1], curve_node)
        if not target:
            continue
        model, property_name, axis, curve_node = target
        if axis not in ("X", "Y", "Z"):
            raise ValueError("Unexpected animation axis: " + axis)
        name = short_name(model)
        first = curve.child("KeyValueFloat").props[0].array()[0]
        # Plant the legs in the original bind stance. Keep relaxed upper-body walking pose.
        lower_body = name == "Hips" or any(part in name for part in ("Leg", "Foot", "Toe"))
        if lower_body and name in base_models:
            values = transform_properties(base_models[name]).get(property_name)
            if values and len(values) == 3:
                first = values["XYZ".index(axis)]
        values = []
        for i in range(frames):
            breath = math.sin(2 * math.pi * i / (frames - 1))
            value = first
            if name == "Spine2":
                if property_name == "Lcl Rotation" and axis == "X":
                    value += 0.45 * breath
                if property_name == "Lcl Scaling" and axis in ("Y", "Z"):
                    value *= 1 + (0.004 if axis == "Y" else 0.006) * breath
            values.append(value)
        for child_name, prop in (
            ("KeyTime", array("l", times)), ("KeyValueFloat", array("f", values)),
            ("KeyAttrFlags", array("i", [4])), ("KeyAttrDataFloat", array("f", [0, 0, 0, 0])),
            ("KeyAttrRefCount", array("i", [frames])),
        ):
            child = curve.child(child_name)
            if child: child.props = [prop]
            else: curve.children.append(Node(child_name, [prop], []))
        authored += 1
        authored_ids.add(curve.props[0].value())
        curve_nodes.add(curve_node)

    if not authored:
        raise ValueError("No animation curves found")
    keep = {"Model", "NodeAttribute", "AnimationStack", "AnimationLayer", "AnimationCurveNode", "AnimationCurve"}
    objects.children = [n for n in objects.children if n.name in keep]
    layers = {parent for node_id in curve_nodes for parent, _ in parents.get(node_id, [])
              if parent in lookup and lookup[parent].name == "AnimationLayer"}
    stacks = {parent for node_id in layers for parent, _ in parents.get(node_id, [])
              if parent in lookup and lookup[parent].name == "AnimationStack"}
    valid_animation_ids = authored_ids | curve_nodes | layers | stacks
    objects.children = [n for n in objects.children
                        if not n.name.startswith("Animation") or n.props[0].value() in valid_animation_ids]
    for node in objects.children:
        if node.name == "AnimationStack":
            node.props[1] = scalar("S", b"Idle\x00\x01AnimStack")
    # Mesh node transforms are harmless but exporting them as Null avoids dangling geometry.
    for n in objects.children:
        if n.name == "Model" and n.props[2].value() == b"Mesh":
            n.props[2] = scalar("S", "Null")
    kept_ids = {n.props[0].value() for n in objects.children}
    connections.children = [c for c in connections.children
        if c.props[1].value() in kept_ids and (c.props[2].value() == 0 or c.props[2].value() in kept_ids)]

    def set_times(node):
        if node.name == "P" and node.props:
            name = node.props[0].value()
            if name in (b"LocalStart", b"ReferenceStart", b"TimeSpanStart"):
                node.props[-1] = scalar("L", 0)
            if name in (b"LocalStop", b"ReferenceStop", b"TimeSpanStop"):
                node.props[-1] = scalar("L", times[-1])
        if node.name in ("LocalTime", "ReferenceTime"):
            node.props = [scalar("L", 0), scalar("L", times[-1])]
        for child in node.children:
            set_times(child)
    for node in fbx.nodes:
        set_times(node)
    definitions = fbx.node("Definitions")
    counts = {}
    for node in objects.children:
        counts[node.name] = counts.get(node.name, 0) + 1
    definitions.child("Count").props = [scalar("I", len(objects.children))]
    definitions.children = [n for n in definitions.children if n.name != "ObjectType" or n.props[0].value().decode() in counts]
    for node in definitions.children:
        if node.name == "ObjectType":
            node.child("Count").props = [scalar("I", counts[node.props[0].value().decode()])]
    output.parent.mkdir(parents=True, exist_ok=True)
    fbx.write(output)
    # Structural round trip and exact closed-loop endpoint validation.
    checked = FBX(output)
    curves = [n for n in checked.node("Objects").children if n.name == "AnimationCurve"]
    for curve in curves:
        values = curve.child("KeyValueFloat").props[0].array()
        assert len(values) == frames and abs(values[0] - values[-1]) < 1e-6
    print(f"Generated {output}: {authored} curves, {frames} keys, 3.5 seconds, {output.stat().st_size} bytes")


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1] / "Project_Tarkov/Project_Tarkov/Assets"
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=root / "Walking.fbx")
    parser.add_argument("--bind", type=Path, default=root / "Models/Player/Ch22_nonPBR.fbx")
    parser.add_argument("--output", type=Path, default=root / "Idle.fbx")
    args = parser.parse_args()
    generate(args.source, args.bind, args.output)
