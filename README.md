# Project_Tarkov

# 타르코프 게임 제작

## 빌드 환경

Windows / Visual Studio 2022 / MSVC v143 기반 C++ 프로젝트입니다.
기본 확인 대상 구성은 **Debug | x64**입니다.

설치할 도구와 외부 라이브러리, Visual Studio 설정, 빌드·실행 방법,
오류 해결 방법은 [빌드 환경 가이드](docs/BUILD.md)에 정리되어 있습니다.
GLAD를 포함한 의존성은 vcpkg manifest로 복원합니다.
Visual Studio에서 `Project_Tarkov/Project_Tarkov.sln`을 열고 Debug | x64로 빌드합니다.

## 캐릭터 애니메이션

대기(Idle), 걷기, 달리기, 웅크리기 이동·대기를 지원합니다.
상태 전환과 Idle.fbx 구성은 [애니메이션 가이드](docs/ANIMATION.md)를 참고하세요.

## 26/5/3
### Core와 Graphics 제작
<img width="1600" height="1042" alt="image" src="https://github.com/user-attachments/assets/337c3567-76b4-4e29-87e7-b79e0361629f" />
<img width="1688" height="798" alt="image" src="https://github.com/user-attachments/assets/6f018011-7e68-4eda-a663-51f05cfe695f" />

## 26/5/4
### Entity와 Game안의 RaidScene 제작
### RaidScene : 키입력을 받아 움직이는 Cube와 바닥 floor 생성
### 마우스 회전으로 화면 회전, 화면이 회전하면 키입력도 변화
<img width="1883" height="1091" alt="image" src="https://github.com/user-attachments/assets/a3b1ddc8-6796-4060-b706-54454e1a981d" />
<img width="357" height="779" alt="image" src="https://github.com/user-attachments/assets/c5534a14-5c35-4d45-b9c4-2a5bc8e32fd3" />

### player 사람모양의 모델로 변경
### F11 전체화면, 전체화면 변경 시, 화면 재조정
### I키 인벤토리, 마우스 포커스 해제
### Shift, Ctrl 달리기 웅크리기 속도 제한
<img width="1707" height="1075" alt="image" src="https://github.com/user-attachments/assets/ef96186c-9d08-4bec-801e-5c69821cacc9" />


## 26/10/8
### Entity → Character → Player 구조로 캐릭터 이동 처리 분리
### 중력과 지면 판정, 벽과 상자 충돌 추가
### 벽에 닿으면 멈추거나 벽을 따라 이동, 바닥 끝에서는 낙하

### 캐릭터 대기, 걷기, 달리기, 웅크리기 애니메이션 적용
### Idle.fbx에 대기 호흡 동작 추가, 웅크린 상태에서도 호흡 표현
### 이동 방향으로 캐릭터 회전, 애니메이션 전환 시 0.15초 보간
### 벽에 막혀 실제 이동이 없으면 대기 동작으로 전환

### 창 포커스를 잃으면 입력 차단, 커서 재포착 시 시점 튐 방지
### 셰이더 파일 변경 시 다시 로드, 오류 발생 시 기존 셰이더 유지

### 불필요한 .build, tests, tools 폴더와 build.ps1 정리
### Visual Studio 솔루션에서 빌드하도록 안내 수정
### 라이브러리 복원에 필요한 vcpkg.json 유지


## 소스 구조

폴더별 역할과 업데이트 순서는 [구조 가이드](docs/STRUCTURE.md)를 참고하세요.
