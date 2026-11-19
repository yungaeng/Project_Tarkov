# 게임에 연결한 외부 모션

2026-11-19 코드 연결. 실행·시각 검증은 미완료다.

- `Mixamo/`: 실제 로딩하는 FBX 51개. 8방향 걷기·달리기·스프린트·웅크린 이동, 서기·웅크림 대기/조준, 점프·착지, 서기/웅크림 사망, 무릎 대기·조준, 엎드림 대기·발사·장전, 꺼내기·수납, 무릎↔엎드림 전환.
- `Quaternius/UAL1_Standard.glb`: CC0 원본 중 비무장 대기·걷기·달리기, 주먹 공격, 가슴 피격 클립을 이름으로 선택한다. 소스와 게임 리그의 바인드 회전을 대조해 게임의 뼈 이름/로컬 회전에 맞춘다. 한 GLB 안의 나머지 클립은 개별 런타임 상태에 연결하지 않았다.
- 실제 사용 파일과 원본 해시: [runtime-manifest.json](runtime-manifest.json).
- 게임 파일에 포함되는 출처·조건: [ANIMATION_NOTICES.txt](../../../Licenses/ANIMATION_NOTICES.txt).
- 취득 원본·미사용 자료: [Assets/_Archive](../../../_Archive/README.md).

## PC 이동 시 필요한 별도 파일

Mixamo FBX는 `.gitignore` 대상이므로 Git으로 소스만 가져오면 이 폴더의 `Mixamo/`가 비어 있을 수 있다. 보관 원본의 FBX와 ZIP도 Git에서 제외된다. 이때 `idle.fbx` 로딩부터 실패하며, 셰이더나 캐릭터 메시 오류가 아니다.

허가된 프로젝트 작업용 모션 원본을 별도로 가져와 `Assets/_Archive/Animations/ThirdParty/Mixamo_Combat/Pro_Rifle_Pack/`에 팩 FBX 49개, `Mixamo_Combat/`에 개별 모션 9개를 두고 저장소 루트에서 `./Tools/Animations/StageImportedAnimations.ps1`을 실행한다. 실행용으로는 그중 FBX 51개를 복사한다. 원본이 없으면 공식 Mixamo에서 `CH22_NONPBR` 리그, FBX Binary / 30fps / Keyframe Reduction none, 팩 No Character / 개별 Without Skin으로 다시 받는다.

2026-11-19 이 PC에서 모션 전체가 누락되어 시작이 중단된 문제를 위 방식으로 복구했다. `idle.fbx`를 포함한 51개를 소스 Assets와 `Project_Tarkov/x64/Debug/Assets/`에 복사하고 원본과 해시 일치를 확인했다. 새 다운로드는 이전 해시와 달라 출처 목록을 갱신했으며, 원본 FBX 58개의 크기·버전·곡선·키 배열/키 수는 이전 기록과 일치했다. 이전 해시 기록은 `Assets/_Archive/Animations/MixamoRecovery-2026-11-19/`에 보존한다. 빌드·게임 실행 검증은 하지 않았다.

소총 기준 전투 표현이다. 권총 아이템은 아직 소총 모델과 표현을 공유한다. 별도 권총 손/무기 모델이나 전투용 칼/개머리판 공격으로 분류하지 않는다.

정지 웅크림은 무릎 대기/조준, 웅크린 이동은 8방향 이동을 사용한다. 엎드린 장전은 게임의 2초 장전 시간에 맞춰 샘플링한다. 꺼내기·수납은 1.2초에 맞추며 해당 구간에는 조준·발사·장전을 막는다. 가져온 팔 동작을 IK가 덮어쓰지 않도록 동작 중 총기를 손 소켓에 연결한다.

X키 근접공격은 무기 수납 상태에서 재생한다. 현재 프레임의 몸통(Spine2)→오른손 주먹 관절(RightHandMiddle1) 좌표와 손목/주먹 간격에서 구한 반경으로 가림·명중을 판정한다. 1.6m 같은 고정 사정거리를 사용하지 않는다. 공격 한 번에 피해 판정을 한 번만 요청하며, 피해는 25다.

Mixamo는 CC0가 아닌 상업용 게임 이용 라이선스다. 원본/게임용 FBX의 독립 에셋 배포를 피하도록 Git에서 제외한다. 화면 크레딧은 추가하지 않았고, 게임 안의 라이선스 파일에 안내를 보존한다. Quaternius는 CC0로 표기 의무가 없지만 동일 파일에 출처를 남긴다. 캐릭터/의상/UI의 기존 권리 확인 문제까지 해결한 것은 아니다.
