# Project_Tarkov

Windows / C++17 / OpenGL 3.3 기반의 싱글플레이 타르코프형 게임 프로젝트입니다.

**은신처 장비 준비 → 레이드 → 탐색·루팅·AI 전투 → 탈출 또는 사망/MIA → 결과·저장 → 재출격** 흐름을 구현하고 있습니다.

데모는 **2026년 12월 중순~말 다운로드·플레이 가능한 형태**를 목표로 합니다. [합의된 범위와 진행 방식](docs/PLANNED.md#demo-scope)을 기준으로 개발하며, 정확한 배포일은 미정입니다.

## 개발 문서

현재 사양과 계획은 다음 두 문서를 기준으로 관리합니다. 문서·폴더 정리일은 2026-11-12이며, 기능과 검증 날짜는 각 문서에 구분해 기록합니다.

- [개발된 내용](docs/IMPLEMENTED.md): 현재 코드 기준 전체 기능, 조작, 수치, 구조, 에셋·도구, 빌드·저장 방법과 검증 이력.
- [개발할 내용](docs/PLANNED.md): 실제 Escape from Tarkov 자료와의 비교, 현재 한계, 상세 요구사항, 우선순위·의존성·완료 기준과 조사 출처.

구조와 편집 방법은 [프로젝트 구조](docs/ARCHITECTURE.md), [맵 데이터](Assets/Maps/README.md), [제작 도구](Tools/README.md)에 정리했습니다. [과거 변경 이력](docs/IMPLEMENTED.md#change-history)은 현재 사양과 구분해 보존합니다. 구현 여부와 실행 검증 여부도 구분합니다.

## 라이선스

- [라이선스 및 상업 배포 준비](docs/licenses/README.md): 캐릭터·모션 교체 계획과 UI 아이콘 수정 대상, 미완료 항목.
- [제3자 라이선스 원문](Open_Source_Licenses.txt): 사용 라이브러리의 고지와 배포 조건.

## 빠른 시작

1. Windows에 Visual Studio 2022 C++ 데스크톱 개발 도구, MSVC v143, Windows SDK와 vcpkg를 준비합니다.
2. 필요하면 `VCPKG_ROOT`를 설정합니다. 루트 `vcpkg.json`으로 의존성을 복원합니다.
3. 저장소 루트의 `Project_Tarkov.sln`을 열어 **Debug | x64**로 빌드합니다.
4. 실행 파일은 `Project_Tarkov/x64/Debug/`에 생성됩니다. 개발 시 루트 `Assets/`를 사용하고, 런타임 자원은 실행 파일 옆에도 복사됩니다.

자세한 환경·실행·저장 경로·문제 해결은 [개발된 내용의 빌드와 실행](docs/IMPLEMENTED.md#2-빌드와-실행)을 참고하세요. 캐릭터 제작 도구는 [Tools 사용 안내](Tools/README.md)에 정리했습니다.

## 현재 범위

- 500×500m 산업단지, 주요 건물 5개, 적 5명, 월드 루팅 20곳.
- 25분 레이드, 무작위 스폰 2곳, 기본 탈출구 2곳.
- 이동·점프·웅크리기·포복, 1/3인칭, 소총 전투, 시체 수색.
- 부위 체력·수분·스태미나, 장비 착탈, 10×8 창고와 방별 가구 보관함, v19 프로필 저장.

원작의 탄도·방탄·의료·경제·퀘스트·은신처 제작 및 온라인 시스템과는 구현 범위가 다릅니다. 상세 차이와 후속 작업은 [개발할 내용](docs/PLANNED.md)에 정리했습니다.

현재 소스는 `Project_Tarkov/src/`, 런타임 자원은 `Assets/`, 제작 도구는 `Tools/`에 있습니다. 기존 `ArtSource/`는 캐릭터 교체 예정에 따라 삭제했습니다. [폴더 관계도](docs/ARCHITECTURE.md#파일폴더-관계도)와 [정리 기록](docs/IMPLEMENTED.md#cleanup-history)을 참고하세요.