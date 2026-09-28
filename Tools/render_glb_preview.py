import numpy as np, json, sys
from PIL import Image, ImageDraw
V=np.load('V.npy'); idx=np.load('idx.npy'); uv=np.load('uv.npy'); tex=np.asarray(Image.open('rudeus_atlas.png').convert('RGB')).astype(np.float32)
TH,TW=tex.shape[:2]
def render(yaw_deg, H=700, extra_pts=None):
    a=np.radians(yaw_deg); R=np.array([[np.cos(a),0,np.sin(a)],[0,1,0],[-np.sin(a),0,np.cos(a)]])
    P=V@R.T
    tri=P[idx]; n=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]); nl=np.linalg.norm(n,axis=1)+1e-9; n/=nl[:,None]
    depth=tri[:,:,2].mean(1)
    cuv=uv[idx].mean(1); tx=np.clip((cuv[:,0]%1)*TW,0,TW-1).astype(int); ty=np.clip((cuv[:,1]%1)*TH,0,TH-1).astype(int)
    col=tex[ty,tx]
    L=np.array([0.3,0.5,0.8]); L/=np.linalg.norm(L); shade=0.55+0.45*np.abs(n@L)
    col=np.clip(col*shade[:,None],0,255).astype(np.uint8)
    mn=V.min(0); mx=V.max(0); s=(H-40)/(mx[1]-mn[1]); W=int(H*0.9)
    img=Image.new('RGB',(W,H),(40,44,52)); d=ImageDraw.Draw(img)
    order=np.argsort(depth)  # far (negative z) first: camera looks along -z from +z
    for t in order:
        pts=[(W/2+p[0]*s, H-20-(p[1]-mn[1])*s) for p in tri[t]]
        d.polygon(pts,fill=tuple(col[t]))
    if extra_pts is not None:
        for p in extra_pts@R.T:
            x,y=W/2+p[0]*s, H-20-(p[1]-mn[1])*s; d.ellipse([x-3,y-3,x+3,y+3],outline=(255,60,60))
    return img
jw=json.load(open('joint_world.json')); J=np.array(list(jw.values()))
imgs=[render(0),render(90),render(180),render(0,extra_pts=J)]
W=sum(i.width for i in imgs); out=Image.new('RGB',(W,imgs[0].height)); x=0
for i in imgs: out.paste(i,(x,0)); x+=i.width
out.save('rudeus_preview.png'); print('saved',out.size)
