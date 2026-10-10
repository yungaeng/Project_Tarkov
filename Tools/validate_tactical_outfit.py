"""Check fitted tactical assets and render front/side/posed views, without building."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from validate_character import load, preview


def pose(vertices, joints, arm_angle=0, leg_angle=0, head_angle=0):
    names = list(joints)
    transforms = [np.eye(4) for _ in names]
    for side, sign in [('Left', -1), ('Right', 1)]:
        angle = np.radians(arm_angle * sign)
        r = np.array([[np.cos(angle),-np.sin(angle),0],
                      [np.sin(angle),np.cos(angle),0],[0,0,1]])
        m = np.eye(4); m[:3,:3]=r
        pivot=joints[side+'Arm']; m[:3,3]=pivot-r@pivot
        for i,name in enumerate(names):
            if name.startswith(side) and any(s in name for s in ('Arm','Hand')): transforms[i]=m
        angle = np.radians(leg_angle * sign)
        r = np.array([[1,0,0],[0,np.cos(angle),-np.sin(angle)],[0,np.sin(angle),np.cos(angle)]])
        m = np.eye(4); m[:3,:3]=r
        pivot=joints[side+'UpLeg']; m[:3,3]=pivot-r@pivot
        for i,name in enumerate(names):
            if name.startswith(side) and any(s in name for s in ('Leg','Foot','Toe')): transforms[i]=m
    angle=np.radians(head_angle)
    r=np.array([[np.cos(angle),0,np.sin(angle)],[0,1,0],[-np.sin(angle),0,np.cos(angle)]])
    m=np.eye(4);m[:3,:3]=r;m[:3,3]=joints['Head']-r@joints['Head']
    transforms[names.index('Head')]=m
    matrices=np.asarray(transforms)[vertices['joints']]
    p=np.column_stack((vertices['position'],np.ones(len(vertices))))
    result=vertices.copy()
    result['position']=(np.einsum('nkij,nj->nki',matrices,p)[:,:,:3]*vertices['weights'][:,:,None]).sum(1)
    result['normal']=(np.einsum('nkij,nj->nki',matrices[:,:,:3,:3],vertices['normal'])*vertices['weights'][:,:,None]).sum(1)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--preview',type=Path)
    args=parser.parse_args()
    v,t,j=load(args.directory/'character.mesh')
    report=json.loads((args.directory/'import.json').read_text(encoding='utf-8'))['tactical_outfit']
    for group,name in [(1,'shirt'),(2,'trousers'),(6,'vest'),(7,'helmet')]:
        part,tri,joints=load(args.directory/'Tactical'/f'{name}.mesh')
        assert set(part['garment'])=={group} and list(joints)==list(j)
        assert np.array_equal(part, v[v['garment']==group])
        assert len(tri)==sum(p['triangles'] for p in report['parts'] if p['garment']==group)
        lengths=np.linalg.norm(part['normal'],axis=1)
        assert np.allclose(lengths,1,atol=1e-4)
        area=np.linalg.norm(np.cross(part['position'][tri[:,1]]-part['position'][tri[:,0]],
                                    part['position'][tri[:,2]]-part['position'][tri[:,0]]),axis=1)
        assert (area>1e-6).all()
    # The carrier must stop above the waist/abdomen, including its pouches.
    vest=v[v['garment']==6]
    assert vest['position'][:,1].min()>127
    assert vest['position'][:,1].min()-117>10
    head=list(j).index('Head')
    helmet=v[v['garment']==7]
    assert np.all(helmet['joints'][:,0]==head) and np.allclose(helmet['weights'][:,0],1)
    for arm,leg,head_angle in [(75,0,0),(45,35,40),(95,-40,-45)]:
        posed=pose(v,j,arm,leg,head_angle)
        assert np.isfinite(posed['position']).all()
        # Rigid helmet and carrier preserve every edge length through the pose.
        for group in (6,7):
            tri=t[v['garment'][t[:,0]]==group]
            a=np.linalg.norm(v['position'][tri[:,1]]-v['position'][tri[:,0]],axis=1)
            b=np.linalg.norm(posed['position'][tri[:,1]]-posed['position'][tri[:,0]],axis=1)
            assert np.allclose(a,b,atol=3e-5)
        for group in (1,2):
            selected=v['garment']==group
            assert np.linalg.norm(posed['position'][selected]-v['position'][selected],axis=1).max()>1 or (group==2 and leg==0)
    print('PASS: 4 separate meshes, atlas coordinates, normals, nondegenerate triangles, skinning poses, rigid helmet/carrier, short vest clearance.')
    if args.preview:
        atlas=np.asarray(Image.open(args.directory/'character.png').convert('RGBA'))
        relaxed=pose(v,j,75)
        moving=pose(v,j,65,25,15)
        settings=[(relaxed,0,63,'FRONT / ALL EQUIPPED'),(relaxed,55,63,'3/4 / SHORT CARRIER'),
                  (relaxed,180,63,'BACK / FITTED CLOTHING'),(moving,-20,15,'SHIRT + TROUSERS / POSE')]
        result=Image.new('RGB',(1600,850),(35,39,44))
        for i,(model,yaw,mask,label) in enumerate(settings):
            view=preview(model,t,atlas,width=400,height=800,yaw=yaw,clothing_mask=mask)
            result.paste(view,(i*400,45))
            ImageDraw.Draw(result).text((i*400+20,20),label,fill='#d5d0b7')
        args.preview.parent.mkdir(parents=True,exist_ok=True)
        result.save(args.preview)


if __name__=='__main__': main()
