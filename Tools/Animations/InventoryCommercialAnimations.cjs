// Reads glTF metadata only; does not execute downloaded content.
const fs = require('fs');
const path = require('path');
const root = path.resolve(__dirname, '../../Assets/_Archive/Animations/ThirdParty');
function walk(p) {
  return fs.readdirSync(p, { withFileTypes: true }).flatMap(e =>
    e.isDirectory() ? walk(path.join(p, e.name)) : [path.join(p, e.name)]);
}
const records = [];
let checkedFiles = 0;
for (const file of walk(root).filter(p => /\.(glb|gltf)$/i.test(p))) {
  let doc;
  if (file.endsWith('.glb')) {
    const bytes = fs.readFileSync(file);
    if (bytes.toString('ascii', 0, 4) !== 'glTF' || bytes.readUInt32LE(16) !== 0x4e4f534a) throw Error(file);
    doc = JSON.parse(bytes.subarray(20, 20 + bytes.readUInt32LE(12)).toString('utf8'));
  } else doc = JSON.parse(fs.readFileSync(file, 'utf8'));
  for (const item of [...(doc.buffers || []), ...(doc.images || [])]) {
    if (item.uri && !item.uri.startsWith('data:')) {
      if (/^[a-z]+:/i.test(item.uri)) throw Error('Unexpected remote resource: ' + item.uri);
      const dependency = path.resolve(path.dirname(file), decodeURIComponent(item.uri));
      if (!dependency.startsWith(root + path.sep) || !fs.existsSync(dependency)) throw Error('Missing/unsafe dependency: ' + dependency);
    }
  }
  checkedFiles++;
  for (const anim of doc.animations || []) {
    records.push({ pack: path.relative(root, file).split(path.sep)[0],
      file: path.relative(root, file).split(path.sep).join('/'),
      name: anim.name || '(unnamed)', channels: anim.channels.length,
      rootMotion: /_RM\./.test(file), license: 'CC0-1.0' });
  }
}
fs.writeFileSync(path.join(root, 'animation-inventory.json'), JSON.stringify({
  retrieved: '2026-11-19', note: 'GLB/glTF entries only. FBX equivalents are not counted twice. A_TPose is a reference pose.',
  records
}, null, 2) + '\n');
let text = '# 다운로드한 상업 이용 가능 애니메이션\n\n확인일: 2026-11-19\n\n';
text += '아래 GLB/glTF 목록의 Quaternius 및 KayKit 세 팩은 동봉 License.txt에서 CC0-1.0을 확인했다. 이 세 팩은 게임 판매·수정·재배포가 가능하며 출처 표기는 필수가 아니다. 추가 Mixamo/ANIMMADE FBX는 별도 라이선스이므로 하단 링크로 구분한다. 이 목록은 취득 기록이며 실제 런타임 사용본은 별도 안내한다. 게임 실행 확인은 미완료다.\n\n';
text += '## 요청한 동작과 확보한 클립\n\n';
text += '- 걷기·달리기: Walk_Loop, Jog_Fwd_Loop, Sprint_Loop, Walking_A/B/C, Running_A/B, Walking_Backwards, Running_Strafe_Left/Right\n';
text += '- 웅크리기·포복: Crouch_Idle_Loop, Crouch_Fwd_Loop, Crouching, Crawling\n';
text += '- 무릎앉기 관련: Fixing_Kneeling (수리 동작이며 전투용 무릎 조준 대기는 아님)\n';
text += '- 권총: Pistol_Idle_Loop, Pistol_Aim_Down/Neutral/Up, Pistol_Shoot, Pistol_Reload, Ranged_1H_Aiming/Reload/Shoot/Shooting\n';
text += '- 소총 관련: Running_HoldingRifle, Ranged_2H_Aiming/Reload/Shoot/Shooting (범용 양손 원거리 동작; 실제 소총 그립 정렬 확인 필요)\n';
text += '- 피격: Hit_Chest, Hit_Head, Hit_Knockback, Hit_A, Hit_B\n';
text += '- 사망: Death01, Death_A, Death_B, Death_A_Pose, Death_B_Pose\n';
text += '- 기타: 점프·착지·회피·구르기·슬라이딩·눕기·일어나기·상호작용·아이템 사용·근접공격·도구 작업 등\n\n';
text += '이 폴더는 취득 원본과 미사용 자료를 보관한다. 실제 사용본과 연결 상태는 [게임용 모션 안내](../../../Animations/Characters/Imported/README.md)를 참고한다. 위 내용은 CC0 세 팩의 취득 목록이다. 추가 확보한 전투 모션은 [Mixamo_Combat/README.md](Mixamo_Combat/README.md)를 참고한다. FBX의 데이터 검사는 [combat-fbx-inventory.json](combat-fbx-inventory.json)에 기록한다. Lie_Down/Lie_Idle/Lie_StandUp은 이름만으로 엎드림 자세라고 단정하지 않는다.\n\n';
for (const pack of [...new Set(records.map(r => r.pack))]) {
  const all = records.filter(r => r.pack === pack);
  const names = [...new Set(all.map(r => r.name))];
  const source = JSON.parse(fs.readFileSync(path.join(root, pack, 'provenance.json'), 'utf8').replace(/^\uFEFF/, ''));
  text += `## ${pack}\n\n공식 출처: ${source.source}\n\n고유 클립 이름 ${names.length}개 (기준 포즈 포함). FBX와 GLB/glTF의 같은 모션, 루트모션 버전은 별도 동작으로 중복 계산하지 않았다.\n\n`;
  text += names.map(n => '- `' + n + '`').join('\n') + '\n\n';
}
text += '## 추가 FBX 자료와 별도 라이선스\n\n- [Mixamo 전투 모션](Mixamo_Combat/README.md): 사용자가 직접 로그인한 뒤 공식 사이트에서 취득. 상업용 게임 이용 가능, CC0 아님. 원본 에셋 독립 재배포 제한.\n- [ANIMMADE 낮은 자세 소총 사격](Animmade_RifleCrouchShooting/README.md): 상품 페이지에서 상업 이용 허용을 확인한 보조 자료. CC0 아님.\n- Cransh Sketchfab 권총·소총 1인칭 팩은 다운로드하지 않았다.\n- 유료 Pro/Source 에디션은 구매하지 않았다.\n\nGLB/glTF 모션 이름은 파일 메타데이터에서 추출했다. 총기와 손의 정렬, 사망 연출의 방향·품질은 재생 검증 전이다.\n';
fs.writeFileSync(path.join(root, 'README.md'), text);
console.log(JSON.stringify({ checkedFiles, fileEntries: records.length, globalUniqueNames: new Set(records.map(r=>r.name)).size, packs: [...new Set(records.map(r => r.pack))].map(pack => ({pack, uniqueNames: new Set(records.filter(r => r.pack === pack).map(r=>r.name)).size})) }, null, 2));
