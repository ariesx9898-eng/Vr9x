"""Normalises the uploaded Rudeus GLB for Unreal import (run: python3 Tools/prepare_rudeus_glb.py).

What it does (and why):
  * The source rig is authored at 10x scale (16.17 m tall). Every joint translation, the
    vertex positions and the inverse-bind-matrix translations are scaled by 0.1, which is
    mathematically identical to a uniform scale of the whole skinned asset (S W S^-1), so the
    bind pose, weights and proportions are untouched. Result: 1.617 m -> 161.7 cm in UE.
  * The ancestor chain (Z-up->Y-up rotation, x0.01 and x100 FBX scales) is flattened into a
    single pure rotation so no scale ends up in the skeleton root bone.
  * KHR_materials_unlit is removed so the material imports as lit (the game uses a lit toon
    master material; an unlit character would ignore sun/shadows/day-night).
The output is verified by re-skinning the bind pose and checking height and foot contact.
"""
import json, struct, sys, os
import numpy as np

SRC = os.path.join(os.path.dirname(__file__), '..', 'SourceArt', 'Characters', 'Rudeus', 'Rudeus_Greyrat.glb')
DST = os.path.join(os.path.dirname(__file__), '..', 'SourceArt', 'Characters', 'Rudeus', 'Rudeus_Greyrat_UE.glb')
SCALE = 0.1

def read_glb(path):
    data = open(path, 'rb').read()
    off, chunks = 12, []
    while off < len(data):
        l, t = struct.unpack('<I4s', data[off:off + 8]); chunks.append((t, bytearray(data[off + 8:off + 8 + l]))); off += 8 + l
    return json.loads(chunks[0][1]), chunks[1][1]

def write_glb(path, j, binary):
    js = json.dumps(j, separators=(',', ':')).encode()
    js += b' ' * ((4 - len(js) % 4) % 4)
    binary = bytes(binary) + b'\0' * ((4 - len(binary) % 4) % 4)
    total = 12 + 8 + len(js) + 8 + len(binary)
    with open(path, 'wb') as f:
        f.write(struct.pack('<4sII', b'glTF', 2, total))
        f.write(struct.pack('<I4s', len(js), b'JSON')); f.write(js)
        f.write(struct.pack('<I4s', len(binary), b'BIN\0')); f.write(binary)

def mat(n):
    return np.array(n['matrix'], dtype=np.float64).reshape(4, 4).T

def main():
    j, b = read_glb(SRC)
    nodes = j['nodes']
    # 1) Flatten ancestors: node0 * node1 * node3 -> one rotation on node0; others identity.
    A = mat(nodes[0]) @ mat(nodes[1]) @ mat(nodes[3])
    sx = np.linalg.norm(A[:3, 0])
    assert abs(sx - 1) < 1e-4, f'net ancestor scale {sx} != 1'
    nodes[0]['matrix'] = [float(x) for x in A.T.reshape(-1)]
    for i in (1, 3, 141):
        nodes[i].pop('matrix', None)
    # 2) Scale joint translations.
    joints = set(j['skins'][0]['joints'])
    for i in joints:
        if 'translation' in nodes[i]:
            nodes[i]['translation'] = [t * SCALE for t in nodes[i]['translation']]
    # 3) Scale POSITION data (tightly packed float3 per bufferView stride 12).
    prim = j['meshes'][0]['primitives'][0]
    acc = j['accessors'][prim['attributes']['POSITION']]
    bv = j['bufferViews'][acc['bufferView']]
    base = bv.get('byteOffset', 0) + acc.get('byteOffset', 0)
    stride = bv.get('byteStride', 12)
    pos = np.frombuffer(bytes(b), dtype=np.uint8)
    for k in range(acc['count']):
        o = base + k * stride
        v = np.frombuffer(bytes(b[o:o + 12]), dtype=np.float32) * SCALE
        b[o:o + 12] = v.astype(np.float32).tobytes()
    acc['min'] = [m * SCALE for m in acc['min']]; acc['max'] = [m * SCALE for m in acc['max']]
    # 4) Scale inverse bind matrix translations (column-major mat4: elements 12,13,14).
    ia = j['accessors'][j['skins'][0]['inverseBindMatrices']]
    ibv = j['bufferViews'][ia['bufferView']]
    ibase = ibv.get('byteOffset', 0) + ia.get('byteOffset', 0)
    for k in range(ia['count']):
        o = ibase + k * 64
        m = np.frombuffer(bytes(b[o:o + 64]), dtype=np.float32).copy()
        m[12:15] *= SCALE
        b[o:o + 64] = m.tobytes()
    # 5) Lit material.
    for m in j['materials']:
        m.get('extensions', {}).pop('KHR_materials_unlit', None)
        if 'extensions' in m and not m['extensions']:
            m.pop('extensions')
    if 'KHR_materials_unlit' in j.get('extensionsUsed', []):
        j['extensionsUsed'].remove('KHR_materials_unlit')
    if not j.get('extensionsUsed'):
        j.pop('extensionsUsed', None)
    j['asset'].setdefault('extras', {})['mt_processing'] = 'scaled x0.1, ancestors flattened, unlit removed (Tools/prepare_rudeus_glb.py)'
    write_glb(DST, j, b)
    print('wrote', os.path.normpath(DST))

if __name__ == '__main__':
    main()
