# Project_Tarkov

# 타르코프 게임 제작

## 빌드 환경

Windows / Visual Studio 2022 / MSVC v143 기반 C++ 프로젝트입니다.
기본 확인 대상 구성은 **Debug | x64**입니다.

설치할 도구와 외부 라이브러리, Visual Studio 설정, 빌드·실행 방법,
오류 해결 방법은 [빌드 환경 가이드](docs/BUILD.md)에 정리되어 있습니다.
GLAD를 포함한 의존성은 vcpkg manifest와 NuGet으로 복원합니다.
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

