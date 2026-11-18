# Mixamo 전투 모션

취득일: 2026-11-19. 공식 Mixamo에 사용자 계정으로 로그인한 뒤 다운로드했다.

## 이용 조건

CC0가 아닌 Adobe/Mixamo 콘텐츠다. 무료이며 상업용 게임에 사용할 수 있다. 원본 캐릭터/애니메이션 파일을 독립 에셋, 에셋 팩, 템플릿 등으로 공개 재배포하거나 판매하는 용도로 사용하지 않는다. 프로젝트 팀 내부 이용과 게임에 포함한 사용은 원본 에셋 배포와 구분한다.

- 공식 FAQ: https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html
- Adobe 커뮤니티의 배포 및 크레딧 안내: https://community.adobe.com/questions-696/mixamo-faq-licensing-royalties-ownership-eula-and-tos-589400?lang=en
- 추가 약관 원문: `Mixamo-Additional-Terms-20210623.pdf`

FAQ는 웹 검색 도구로 읽었다. 파일 보관용 직접 다운로드는 Adobe CDN의 접근 거부로 저장하지 못했다. 위 링크와 실제 내려받은 추가 약관을 함께 보존한다.

## 요청한 동작

- `Rifle_Kneel_Idle.fbx`: 소총을 든 무릎앉기 대기.
- `Rifle_Kneel_To_Aim.fbx`: 무릎앉기에서 소총 조준으로 전환. 정지 조준은 마지막 자세를 유지하는 방식으로 활용할 수 있으나 적용/재생 검증 전이다.
- `Prone_Idle.fbx`: 소총을 든 엎드림 대기. 별도의 상하 조준 공간/조준 오프셋 팩은 아니다.
- `Prone_Firing_Rifle.fbx`: 엎드린 소총 발사. 검색 결과의 두 동명 클립 중 설명이 `Firing A Rifle While Prone`인 전신 버전을 선택했다.
- `Prone_Reloading.fbx`: 엎드린 소총 장전.
- `Rifle_Pull_Out.fbx`: 소총 꺼내기.
- `Rifle_Put_Away.fbx`: 소총 수납.
- `Rifle_Kneel_To_Prone.fbx`: 소총을 든 무릎앉기 → 엎드림 전환.
- `Rifle_Prone_To_Kneel.fbx`: 소총을 든 엎드림 → 무릎앉기 전환.

## 추가 확보 팩

`Pro_Rifle_Pack.zip` 및 `Pro_Rifle_Pack/`에는 49개의 FBX가 있다. 공식 검색 화면의 팩 이름을 보고 취득했으나, 실제 압축파일은 이동·웅크리기·조준 대기·점프·사망 중심이다. 엎드린 사격/장전이나 무기 꺼내기/수납을 이 팩에서 확보했다고 계산하지 않는다. 상세 실제 파일 목록과 SHA-256은 상위 `combat-fbx-inventory.json`에 기록한다.

## 내보내기와 검사

- FBX Binary, 30fps, Keyframe Reduction: none.
- 단일 클립은 Without Skin, 팩은 No Character로 내보냈다.
- 사용자의 기존 Mixamo 캐릭터 `Ch22_nonPBR`의 스켈레톤을 기준으로 서비스에서 리타기팅되었다. 캐릭터 메시와 텍스처는 다운로드 대상에 포함하지 않았다.
- `Tools/Animations/InspectFbxAnimations.cjs`로 바이너리 구조, 애니메이션 스택/곡선, 압축 키 배열, 메시 제외 여부를 정적으로 확인한다. 원본 출처와 해시는 `provenance.json` 및 상위 인벤토리에 보관한다.
- 취득 이후 런타임 사용본을 `Assets/Animations/Characters/Imported/`로 분리하고 상태·팔 IK·총기 손 소켓을 코드에 연결했다. 이 폴더는 취득 원본 보관용이다. 게임 재생/시각 검증은 아직 수행하지 않았다.
