# 프로젝트 구조와 작업 경계

기준일: 2026-11-10. 현재 소스와 프로젝트 설정을 설명한다. 빌드 출력은 기존 `Project_Tarkov/x64/Debug/`를 유지한다.

## 폴더 역할

- `Project_Tarkov.sln`: 저장소 루트의 Visual Studio 진입점.
- `src/Project_Tarkov.vcxproj`, `.filters`: 컴파일 대상과 솔루션 탐색기 표시 목록.
- `src/Core/`: 실행 루프·입력·로그·경로 탐색·에셋 카탈로그.
- `src/Game/Character/`, `Combat/`: 캐릭터와 생존 상태, 전투·AI·루팅.
- `src/Game/Raid/`: 레이드 진행, 결과 장면, 상태와 결과 데이터.
- `src/Game/Hideout/`: 저장 프로필, 은신처 상호작용, 가구 명령의 저장·실패 롤백.
- `src/Game/Inventory/`: 아이템 데이터와 UI에 의존하지 않는 인벤토리 명령.
- `src/Game/UI/`: RaidHud·HideoutHud·InventoryHud·ResultHud와 공통 HudWidgets·InventoryUi.
- `src/Game/World/`: 맵 로딩과 절차적 배치 구현. 기본 도형과 배칭은 `src/Graphics/WorldGeometry.*`를 사용한다.
- `src/Animation/`, `Graphics/`, `Physics/`, `Scene/`: 애니메이션·표현·충돌·장면 기반 코드.
- `Assets/`: 런타임에 읽는 메시·텍스처·모션·셰이더·맵 데이터. [맵 데이터 형식](../Assets/Maps/README.md).
- `ArtSource/`: 편집 원본과 파생 제작 자료. [원본 관리 안내](../ArtSource/README.md).
- `Tools/`: 캐릭터 제작 및 별도 검증 소스. [사용 안내](../Tools/README.md).
- `docs/IMPLEMENTED.md`: 현재 사양. `docs/PLANNED.md`: 후속 계획과 완료 상태. `docs/history/`: 당시 변경 기록.
- `Project_Tarkov/x64/Debug/`: EXE·DLL·복사된 Assets, 중간 파일 `obj/Project_Tarkov/`, 기본 `Saves/`, 별도 검증 결과 `qa/`.
- `.build/`, `vcpkg_installed/`: 의존성 캐시 및 기존 작업 자료. 게임 빌드·테스트 출력 위치를 이쪽으로 옮기지 않는다.

이전 프로젝트 위치의 세이브·IDE 개인 설정·생성물은 호환과 데이터 보존을 위해 남아 있을 수 있다. 이 폴더들을 새 소스의 중복 사본으로 보고 일괄 삭제하지 않는다. `vc140.pdb`는 Git 추적에서 제외되었고 `.pdb`는 ignore한다. Wavefront 모델인 `Assets/Models/Weapons/*.obj`는 컴파일 중간 파일과 다르게 관리한다.

## 책임과 데이터 흐름

장면 수명과 플레이어·충돌 월드·렌더 자원의 소유자는 여전히 `RaidScene`이다. `HideoutScene.cpp`는 동일 클래스의 은신처 메서드를 별도 구현 파일로 분리한 것이며, 별도의 독립 HideoutScene 클래스를 도입한 것은 아니다. `Hideout`은 장면과 구분되는 저장 프로필이다.

화면은 `InventoryCommands::Command`를 생성하고 일반 조작은 `InventoryCommands::Execute`, 레이드 조작은 `ApplyRaid`, 은신처 조작은 `Hideout::ApplyInventoryCommand`로 전달한다. 은신처 명령은 기존 가구 종류·수납 검증과 저장 실패 롤백을 유지한다. UI의 포커스·생존 여부 같은 입력 조건은 화면/장면에서 검사한다.

`HudWidgets`의 장비 카드 테두리·인체 표시·캐릭터 미리보기를 여러 화면에서 재사용한다. ImGui 렌더 콜백에 넘기는 미리보기 객체는 프레임 렌더 종료까지 살아 있어야 한다. 화면을 더 분리할 때 함수 내부 임시 객체의 주소를 콜백에 넘겨 수명을 단축하지 않는다.

## 에셋 경로와 배포

`AssetCatalog.h`는 논리 파일 이름, `RuntimePaths`는 파일 시스템 경로를 담당한다. 로더는 UTF-8 경로를 처리하고 현재 작업 디렉터리를 바꾸지 않는다.

에셋은 `TARKOV_ASSET_ROOT` 절대 경로가 최우선이다. 미지정 시 실행 파일의 상위에서 `vcpkg.json`과 `src/`가 있는 저장소를 찾으면 그 루트의 Assets를 사용하고, 저장소 밖 패키지는 EXE 옆 Assets를 사용한다. 개발 중 출력 폴더의 에셋을 직접 확인하려면 `TARKOV_ASSET_ROOT`를 그 폴더로 지정한다.

`CopyRuntimeAssets`는 셰이더·애니메이션·맵 파일, UI PNG, 무기 OBJ, 공통 리그 FBX, 각 캐릭터의 character.mesh/png를 복사한다. 제작 원본·의상 중간 메시·import.json은 배포 대상이 아니다. 기존 출력 폴더의 오래된 파일을 삭제하는 타깃은 없으므로 배포본은 새 패키지 폴더에 필요한 결과만 구성한다. 실제 사용자 세이브를 배포본에 포함하지 않는다.

네 기본 FBX는 `Assets/Animations/Characters/Locomotion/`에 보존되어 있다. Idle·Walking·Fast Run·Crouched Walking은 실제 로딩 대상이므로 제거하지 않는다.

## 세이브 선택

1. `TARKOV_SAVE_ROOT`가 지정되면 그 절대 경로를 사용한다.
2. 미지정 시 EXE 옆 `Saves/hideout.txt`가 있으면 우선 사용한다. 과거 폴더에 다른 프로필이 있어도 이 우선순위는 유지된다.
3. EXE 옆에 프로필이 없으면 저장소 루트 `Saves`, 예전 `Project_Tarkov/Project_Tarkov/Saves`, `Project_Tarkov/Saves`를 탐색한다. 하나면 그대로 사용하고, 둘 이상이면 경로 지정이 필요하다.
4. 기존 파일이 없으면 EXE 옆 Saves를 사용한다.

탐색 과정은 프로필을 이동·병합·변환하지 않는다. 게임이 프로필을 로드하고 저장할 때 기존 버전 변환과 저장 동작이 수행된다. 현재 저장 헤더는 `HIDEOUT 19`다. 이전 프로필을 수동으로 옮기려면 게임을 종료하고 파일을 백업한 뒤 경로를 지정한다. 두 프로필을 자동 병합하는 기능은 없다.

## 추가 파일을 넣을 때

새 cpp/h는 `.vcxproj`와 `.filters` 양쪽에 등록한다. 새 런타임 자원 종류는 `CopyRuntimeAssets`의 선택 범위와 카탈로그를 확인한다. 제작 스크립트의 상대 인자는 저장소 루트 기준이다. 에셋 이동과 게임 규칙 변경은 분리해 기록한다.

`.gitattributes`는 텍스트·바이너리 분류만 설정한다. Git LFS 필터와 기존 이력 변환은 적용하지 않았다. LFS는 원격 저장소 지원과 저장 용량 정책을 확인한 뒤 별도 도입할 항목이다.

## 확인 범위

이전 구조 정리 커밋 `27ad256`의 문서에는 Debug/x64 빌드·시작 스모크 테스트·Python 구문 검사 결과가 기록되어 있다. 이번 후속 작업에서는 소스 include 경로, 프로젝트/필터 목록을 대조했고 연결 누락은 발견하지 못했다. 이 대조는 컴파일이나 플레이 검증이 아니다. 이번에는 빌드·테스트·게임 실행을 하지 않았다.
