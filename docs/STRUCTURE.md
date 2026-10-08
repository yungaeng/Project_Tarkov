# 소스 구조

소스 루트는 `Project_Tarkov/Project_Tarkov`이며 Visual Studio 필터도 실제 폴더와 일치합니다.
솔루션 위치와 `Assets` 경로는 기존과 같습니다.

| 폴더 | 역할 |
| --- | --- |
| Core | Application, Window, Input, Time, Logger |
| Scene | Scene, SceneManager |
| Game | RaidScene, Entity, EntityManager, Character, Player, PlayerController, 설정 |
| Physics | CollisionWorld |
| Graphics | Renderer, Mesh, Shader, Texture, Camera, ModelLoader, ThirdPersonCameraController |
| Animation | SkeletalAnimation, CharacterVisual |
| Assets | 모델, 애니메이션, 셰이더 및 텍스처 |

## 한 프레임의 처리 순서

1. Application이 창 이벤트와 Input을 갱신합니다.
2. RaidScene이 PlayerController를 통해 인벤토리 입력을 처리합니다.
3. 게임 입력이 허용되면 ThirdPersonCameraController가 카메라 방향을 갱신합니다.
4. PlayerController가 이전 이동 명령을 지우고 이동·웅크림·달리기 명령을 설정합니다.
5. Character가 충돌·중력을 적용하고 실제 이동 여부와 방향을 갱신합니다.
6. CharacterVisual이 캐릭터 상태에 맞춰 애니메이션과 메시를 갱신합니다.
7. 카메라가 플레이어 위치를 추적하고 RaidScene이 월드와 캐릭터를 렌더링합니다.

## 소유권과 의존성

- RaidScene이 충돌 월드, 플레이어, 캐릭터 표현을 소유합니다. 충돌 월드는 플레이어보다 오래 살아 있습니다.
- Character는 충돌 월드를 비소유 포인터로 참조하며 키보드·카메라·OpenGL에 의존하지 않습니다.
- EntityManager에 객체를 추가할 때 unique_ptr로 소유권을 이전합니다.
- Application, SceneManager, RaidScene, Character, EntityManager의 주요 구현은 cpp에 있습니다.
- 헤더는 필요한 라이브러리 헤더를 직접 포함하며 공통 헤더의 using namespace에 의존하지 않습니다.
- 그래픽 자원은 Application::Shutdown에서 창의 OpenGL 컨텍스트를 제거하기 전에 해제합니다.

## 변경 범위와 확인

GLFW를 포함한 의존성은 루트 vcpkg.json으로 복원합니다. 기존 NuGet 패키지 캐시는 더 이상 프로젝트에서 참조하지 않습니다.
걷기 5, 달리기 9, 웅크리기 2.5를 사용합니다. 몸체 높이는 서 있을 때 1.8, 웅크릴 때 1.1이며 일어서기 전에 공간을 검사합니다.
점프와 카메라 충돌은 별도 구현 과제입니다.

2026-10-08 구조 정리 후 파일 경로, 프로젝트·필터 등록, 로컬 include와 에셋 참조를 정적으로 확인합니다.
사용자 요청에 따라 이번 변경의 빌드·실행 검증은 수행하지 않습니다.
