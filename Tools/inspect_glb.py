import json, struct, numpy as np, io
from PIL import Image, ImageDraw
P='/root/.claude/uploads/8f8f58f4-a454-521c-8e8c-db8593c883ac/12631da0-rudeus_greyrat_-_mushoku_tensei.glb'
data=open(P,'rb').read(); off=12; ch=[]
while off<len(data):
    l,t=struct.unpack('<I4s',data[off:off+8]); ch.append(data[off+8:off+8+l]); off+=8+l
j=json.loads(ch[0]); bin_=ch[1]
CT={5126:np.float32,5123:np.uint16,5121:np.uint8,5125:np.uint32}
NC={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT4':16}
def acc(i):
    a=j['accessors'][i]; bv=j['bufferViews'][a['bufferView']]
    dt=CT[a['componentType']]; n=NC[a['type']]; o=bv.get('byteOffset',0)+a.get('byteOffset',0)
    stride=bv.get('byteStride')
    if stride and stride!=np.dtype(dt).itemsize*n:
        raw=np.frombuffer(bin_,dtype=np.uint8,count=a['count']*stride,offset=o).reshape(a['count'],stride)
        arr=raw[:,:np.dtype(dt).itemsize*n].copy().view(dt).reshape(a['count'],n)
    else:
        arr=np.frombuffer(bin_,dtype=dt,count=a['count']*n,offset=o).reshape(a['count'],n)
    if a.get('normalized'): arr=arr.astype(np.float32)/np.iinfo(dt).max
    return arr
def q2m(q):
    x,y,z,w=q
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
def local(n):
    if 'matrix' in n: return np.array(n['matrix']).reshape(4,4).T
    M=np.eye(4); t=n.get('translation',[0,0,0]); r=n.get('rotation',[0,0,0,1]); s=n.get('scale',[1,1,1])
    M[:3,:3]=q2m(r)@np.diag(s); M[:3,3]=t; return M
nodes=j['nodes']; parent={}
for i,n in enumerate(nodes):
    for c in n.get('children',[]): parent[c]=i
W={}
def world(i):
    if i in W: return W[i]
    M=local(nodes[i]); 
    if i in parent: M=world(parent[i])@M
    W[i]=M; return M
prim=j['meshes'][0]['primitives'][0]
pos=acc(prim['attributes']['POSITION']).astype(np.float64); uv=acc(prim['attributes']['TEXCOORD_0'])
J=acc(prim['attributes']['JOINTS_0']).astype(int); Wt=acc(prim['attributes']['WEIGHTS_0']).astype(np.float64)
idx=acc(prim['indices']).reshape(-1,3)
print('verts',len(pos),'tris',len(idx))
ws=Wt.sum(1); print('weight sum min/max',ws.min(),ws.max(),' unnormalized(>1e-3):',int((abs(ws-1)>1e-3).sum()))
print('influences/vert histogram',np.bincount((Wt>1e-4).sum(1)))
skin=j['skins'][0]; joints=skin['joints']; IBM=acc(skin['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
mesh_node=[i for i,n in enumerate(nodes) if 'mesh' in n][0]
print('mesh node',mesh_node,nodes[mesh_node].get('name'),'skin on node:',nodes[mesh_node].get('skin'))
# skinned bind pose: joint world * IBM
Mskin=np.array([world(jn)@IBM[k] for k,jn in enumerate(joints)])
ph=np.c_[pos,np.ones(len(pos))]
sk=np.zeros((len(pos),4))
for k in range(4):
    sk+=Wt[:,k:k+1]*np.einsum('nij,nj->ni',Mskin[J[:,k]],ph)
V=sk[:,:3]
mn,mx=V.min(0),V.max(0); print('skinned bbox min',mn.round(3),'max',mx.round(3),'size',(mx-mn).round(3))
# glTF is Y-up
height=mx[1]-mn[1]; print('HEIGHT (gltf units, meters by spec):',round(height,4))
jw={nodes[jn]['name']:world(jn)[:3,3] for jn in joints}
for nm in ['_rootJoint','top_C0_0_jnt_02','spine_C0_0_jnt_05','spine_C0_3_jnt_063','neck_C0_0_jnt_066','head_C0_0_jnt_067','leg_L0_0_jnt_08','leg_L0_1_jnt_09','leg_L0_2_jnt_010','leg_L0_3_jnt_011','arm_L0_0_jnt_092','arm_L0_1_jnt_093','arm_L0_2_jnt_094','shoulder_L0_0_jnt_091','hair_C0_0_jnt_075']:
    print(f'{nm:24s}',jw[nm].round(4))
# unused joints (no weights)
used=set(np.unique(J[Wt>1e-4])); unused=[nodes[joints[k]]['name'] for k in range(len(joints)) if k not in used]
print('joints with zero weights:',len(unused),unused[:40])
np.save('V.npy',V); np.save('idx.npy',idx); np.save('uv.npy',uv)
img=j['images'][0]; bv=j['bufferViews'][img['bufferView']]
tex=Image.open(io.BytesIO(bin_[bv.get('byteOffset',0):bv.get('byteOffset',0)+bv['byteLength']])); tex.save('rudeus_atlas.png'); print('texture',tex.size,tex.mode)
json.dump({k:[float(x) for x in v] for k,v in jw.items()},open('joint_world.json','w'),indent=0)
