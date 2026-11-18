// Static filesystem/provenance inspection. Does not build or launch the game.
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const repo=path.resolve(__dirname,'../..'),assets=path.join(repo,'Assets');
const runtime=path.join(assets,'Animations/Characters/Imported');
const archive=path.join(assets,'_Archive/Animations/ThirdParty');
const walk=p=>fs.readdirSync(p,{withFileTypes:true}).flatMap(e=>e.isDirectory()?walk(path.join(p,e.name)):[path.join(p,e.name)]);
const hash=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const records=[];
for(const file of walk(runtime).filter(p=>/\.(fbx|glb)$/i.test(p))) {
  const name=path.basename(file);let source,license;
  if(file.includes(path.sep+'Mixamo'+path.sep)) {
    source=path.join(archive,'Mixamo_Combat',name);
    if(!fs.existsSync(source)) source=path.join(archive,'Mixamo_Combat/Pro_Rifle_Pack',name);
    license='Adobe/Mixamo terms; commercial game use; standalone asset redistribution restricted';
  } else {
    source=path.join(archive,'Quaternius_UniversalAnimationLibrary/Extracted/Universal Animation Library[Standard]/Unreal-Godot',name);
    license='CC0-1.0';
  }
  if(!fs.existsSync(source)||hash(file)!==hash(source)) throw Error('Missing/different acquisition source: '+file);
  records.push({file:path.relative(assets,file).split(path.sep).join('/'),bytes:fs.statSync(file).size,
    sha256:hash(file),source:path.relative(assets,source).split(path.sep).join('/'),license});
}
if(records.filter(r=>r.file.includes('/Mixamo/')).length!==51) throw Error('Unexpected Mixamo runtime count');
const json=JSON.stringify({date:'2026-11-19',verification:'Static paths, bytes and SHA-256 only; game loading/playback not verified',records},null,2)+'\n';
fs.writeFileSync(path.join(runtime,'runtime-manifest.json'),json);
fs.writeFileSync(path.join(assets,'Licenses/AnimationSources.json'),json);
const catalog=fs.readFileSync(path.join(repo,'Project_Tarkov/src/Core/AssetCatalog.h'),'utf8');
for(const m of catalog.matchAll(/=\s*"([^"\n]+)"/g))
  if(!fs.existsSync(path.join(assets,m[1]))) throw Error('Missing catalog asset: '+m[1]);
const glb=fs.readFileSync(path.join(runtime,'Quaternius/UAL1_Standard.glb'));
const doc=JSON.parse(glb.subarray(20,20+glb.readUInt32LE(12)).toString('utf8'));
const takes=new Set(doc.animations.map(a=>a.name));
for(const name of ['Idle_Loop','Walk_Loop','Sprint_Loop','Punch_Cross','Hit_Chest'])
  if(!takes.has(name)) throw Error('Missing selected CC0 take: '+name);
console.log(JSON.stringify({runtimeAnimationFiles:records.length,mixamo:51,cc0LibraryFiles:1,catalogPaths:'present',selectedCC0Takes:'present',sourceHashes:'match',gameExecuted:false},null,2));
