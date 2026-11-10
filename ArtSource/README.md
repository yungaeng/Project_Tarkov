# 캐릭터 제작 원본

이 폴더는 배포용 Assets와 분리한 편집·제작 자료다.

- `Characters/SurvivorHead/`: VRoid·VRM·Blender 원본과 머리 연결 메타데이터·미리보기.
- `Characters/여캐/Derived/`, `Characters/Male/Derived/`: import.json, Tactical·Underwear의 중간 메시와 재질.
- 실행 결과인 character.mesh와 character.png는 `Assets/Models/Player/<캐릭터>/`에 둔다.

`Tools/Characters/paths.py`의 `source_workspace()`는 저장소의 표준 캐릭터 폴더를 대상으로 할 때 이 Derived 경로를 사용한다. 사용자가 별도의 출력 폴더를 지정하면 메타데이터와 중간 자료도 그 폴더 안에 유지한다. [도구 사용법](../Tools/README.md)을 참고한다.

원본과 파생 메시를 불필요한 빌드 생성물로 간주해 삭제하지 않는다. 게임 배포에는 vcxproj에서 선택한 런타임 자원만 복사한다. Git LFS는 현재 적용하지 않았다.
