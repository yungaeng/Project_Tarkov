// Inspect binary FBX metadata without executing downloaded content.
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const zlib = require('zlib');
const root = path.resolve(__dirname, '../../Assets/_Archive/Animations/ThirdParty');
const walk = p => fs.readdirSync(p,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(path.join(p,e.name)):[path.join(p,e.name)]);
function inspect(file) {
  const b=fs.readFileSync(file);
  if (!b.subarray(0,23).equals(Buffer.from('Kaydara FBX Binary  \0\x1a\0','binary'))) throw Error('Invalid FBX: '+file);
  const version=b.readUInt32LE(23), wide=version>=7500, header=wide?25:13;
  let stacks=[], meshes=0, curves=0, keyArrays=0, keyCount=0;
  function node(start) {
    if(start+header>b.length) throw Error('Truncated FBX node');
    const end=wide?Number(b.readBigUInt64LE(start)):b.readUInt32LE(start);
    if(!end) return null;
    const count=wide?Number(b.readBigUInt64LE(start+8)):b.readUInt32LE(start+4);
    const propBytes=wide?Number(b.readBigUInt64LE(start+16)):b.readUInt32LE(start+8);
    const nameLength=b[start+header-1];
    let p=start+header+nameLength;
    if(end>b.length||end<=p||p+propBytes>end) throw Error('Invalid node boundary');
    const name=b.toString('utf8',start+header,p), values=[];
    for(let i=0;i<count;i++) {
      const type=String.fromCharCode(b[p++]);
      if(type==='S'||type==='R') {const n=b.readUInt32LE(p);p+=4;values.push(type==='S'?b.toString('utf8',p,p+n):null);p+=n;}
      else if('YCFDIL'.includes(type)) {const size={Y:2,C:1,F:4,D:8,I:4,L:8}[type]; values.push(type==='I'?b.readInt32LE(p):null);p+=size;}
      else if('fdilbc'.includes(type)) {
        const n=b.readUInt32LE(p), encoding=b.readUInt32LE(p+4), bytes=b.readUInt32LE(p+8); p+=12;
        const data=b.subarray(p,p+bytes), expected=n*({f:4,d:8,i:4,l:8,b:1,c:1}[type]);
        const raw=encoding===1?zlib.inflateSync(data):data;
        if(raw.length!==expected) throw Error('Invalid array length');
        if(name==='KeyTime') { keyArrays++; keyCount+=n; }
        values.push({arrayType:type,count:n});p+=bytes;
      } else throw Error('Unknown FBX property type '+type);
    }
    if(p!==start+header+nameLength+propBytes) throw Error('Property size mismatch');
    if(name==='AnimationStack') stacks.push(values.filter(v=>typeof v==='string').map(s=>s.split('\0')[0])[0]||'(unnamed)');
    if(name==='AnimationCurve') curves++;
    if(name==='Geometry'&&values.includes('Mesh')) meshes++;
    while(p+header<=end) { const next=node(p);if(!next) break;p=next; }
    return end;
  }
  let p=27;while(p+header<b.length){const next=node(p);if(!next)break;p=next;}
  if(!stacks.length||!curves||!keyCount) throw Error('No animation data: '+file);
  return {file:path.relative(root,file).split(path.sep).join('/'),bytes:b.length,sha256:crypto.createHash('sha256').update(b).digest('hex'),fbxVersion:version,stacks,meshes,curves,keyArrays,keyCount};
}
const records=walk(root).filter(p=>/\.fbx$/i.test(p)&&/Mixamo_Combat|Animmade_RifleCrouchShooting/.test(p)).map(inspect);
const required = ['Rifle_Kneel_Idle','Rifle_Kneel_To_Aim','Prone_Idle','Prone_Firing_Rifle','Prone_Reloading','Rifle_Pull_Out','Rifle_Put_Away','Rifle_Kneel_To_Prone','Rifle_Prone_To_Kneel'];
for (const name of required) {
  const record=records.find(r=>r.file==='Mixamo_Combat/'+name+'.fbx');
  if(!record||record.meshes) throw Error('Missing required animation or unexpected mesh: '+name);
}
const mixamoRecords=records.filter(r=>r.file.startsWith('Mixamo_Combat/'));
fs.writeFileSync(path.join(root,'Mixamo_Combat','provenance.json'),JSON.stringify({
  retrieved:'2026-11-19',source:'https://www.mixamo.com/',
  acquired:'Official website; authenticated by user; browser download',
  license:'Adobe/Mixamo terms; commercial game use permitted; not CC0; no standalone raw asset redistribution',
  faq:'https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html',
  settings:{format:'FBX Binary',fps:30,keyframeReduction:'none',singleClipSkin:'Without Skin',packPose:'No Character'},
  requestedFiles:required.map(n=>n+'.fbx'),records:mixamoRecords
},null,2)+'\n');
fs.writeFileSync(path.join(root,'Animmade_RifleCrouchShooting','provenance.json'),JSON.stringify({
  retrieved:'2026-11-19',source:'https://animmade.com/animation/58',
  officialDownload:'https://drive.google.com/uc?export=download&id=1W37H_rGQ6Bta1fpCWExw1eo13tXbN3jF',
  license:'Product page grants royalty-free commercial and non-commercial use; not identified as CC0; linked EULA returned 404',
  records:records.filter(r=>r.file.startsWith('Animmade_RifleCrouchShooting/'))
},null,2)+'\n');
fs.writeFileSync(path.join(root,'combat-fbx-inventory.json'),JSON.stringify({retrieved:'2026-11-19',note:'Binary structure, decompressed key-array lengths, animation presence, and mesh counts checked. Playback/retargeting not tested.',records},null,2)+'\n');
console.log(JSON.stringify({files:records.length,meshFiles:records.filter(r=>r.meshes).map(r=>r.file),records:records.filter(r=>!r.file.includes('Pro_Rifle_Pack/'))},null,2));
