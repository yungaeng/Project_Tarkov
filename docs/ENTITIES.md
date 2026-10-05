# 게임 객체 구조

현재 구조는 Entity → Character → Player입니다.

- Entity는 위치·회전·크기와 활성 상태를 가집니다.
- Character는 월드 기준 이동 방향과 속도를 받아 수평 이동을 처리합니다.
- Player는 Character의 이동과 Entity의 위치를 그대로 사용합니다.
- RaidScene이 Player를 unique_ptr로 소유하고, 입력·카메라·렌더링을 처리합니다.

매 프레임 장면은 먼저 StopMovement를 호출합니다. 게임 입력이 허용될 때만
WASD와 카메라 방향으로 SetMovement를 설정하고, Update(dt)를 한 번 호출합니다.
따라서 인벤토리가 열려 있거나 창이 비활성일 때 이전 이동 명령이 남지 않습니다.
속도는 기존 값인 걷기 5, 달리기 9, 웅크리기 2.5를 유지합니다.
Character 자체는 입력 장치나 카메라를 참조하지 않습니다.

SetMovement의 명령은 StopMovement 또는 다음 SetMovement까지 유지됩니다.
다른 제어 코드에서도 이동을 중단할 때 반드시 명령을 갱신해야 합니다.
Entity 참조를 통해 Update(dt)를 호출해도 Character의 이동 처리가 실행됩니다.
EntityManager는 현재 실행 경로에 사용하지 않습니다.

## 이후 확장

- AICharacter는 Character를 상속하고, AI 판단 결과를 이동 명령으로 전달합니다.
  RaidScene이 vector<unique_ptr<AICharacter>>로 소유할 수 있습니다.
- 월드 아이템은 Entity를 상속하여 위치와 표시 모델을 가집니다.
  캐릭터 이동·체력은 필요하지 않습니다.
- 인벤토리 아이템은 월드 위치가 없는 별도 데이터로 둡니다.
  줍기·버리기는 아이템 데이터의 소유권을 옮기고 월드 표현을 제거·생성합니다.

정적 상자 충돌·중력·지면 판정은 [물리 처리 설명](PHYSICS.md)에 정리했습니다.
AI 판단, 체력, 아이템 데이터 및 줍기는 아직 구현하지 않았습니다.

## 검증

Developer PowerShell for VS 2022에서 저장소 루트 기준:

```powershell
.\tests\run-character-tests.ps1
.\tests\run-input-tests.ps1
```

캐릭터 테스트는 위치 공유, Entity를 통한 업데이트, 대각선 속도,
수평 이동, 정지, 비활성 상태, 잘못된 시간·속도·방향 입력,
서로 다른 프레임 간격과 캐릭터 유형의 공통 이동 처리를 확인합니다.
로컬 GLM 설치가 있으면 실제 헤더를 사용하고, 없으면 테스트용 벡터 대역을 사용합니다.
대역 사용 결과는 실제 GLM이나 게임 전체의 빌드 검증을 의미하지 않습니다.
