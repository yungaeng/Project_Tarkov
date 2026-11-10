# 캐릭터 제작 도구

이 디렉터리의 Python 스크립트는 저장소 루트에서 실행할 수 있으며, 현재 작업 디렉터리와 무관하게 저장소 경로를 계산한다. 인자로 받는 경로는 저장소 루트 기준 상대 경로 또는 절대 경로를 사용할 수 있다.

```powershell
python .\Tools\Characters\<script>.py --help
```

대부분의 스크립트는 NumPy와 Pillow가 필요하다. PMX 변환은 추가로 Assimp 네이티브 라이브러리가 필요하며, 설치 위치는 `--assimp`로 지정할 수 있다. 각 스크립트의 `--help` 출력이 옵션 기본값과 상세 사용법의 기준이다.

## 도구

| 스크립트 | 용도와 입력/출력 |
| --- | --- |
| `Characters/import_pmx_character.py` | PMX `source`를 기존 게임 리그로 변환한다. `--rig`, `--assimp`, `--output`으로 리그·Assimp 라이브러리·결과를 지정한다. |
| `Characters/attach_vrm_head.py` | VRM에서 내보낸 머리 `source`를 게임 캐릭터 `base`에 연결하고 지정한 `output`에 쓴다. |
| `Characters/create_tactical_outfit.py` | 캐릭터 `directory`의 바디 데이터를 사용해 전술복 메시와 텍스처를 생성한다. `--vest-only`는 다른 의상 결과물을 보존하며 조끼만 다시 만든다. |
| `Characters/fit_underwear.py` | 캐릭터 `directory`에 여성 속옷을 피팅한다. `--thong-only`는 브라와 겉옷을 보존한다. |
| `Characters/fit_black_trunks.py` | 캐릭터 `directory`에 남성 검정 트렁크를 피팅한다. |
| `Characters/create_combat_boots.py` | `--directory`의 캐릭터에 군화 메시와 재질 패치를 생성한다. 기본 대상은 `Assets/Models/Player/여캐/`다. |
| `Characters/validate_character.py` | 캐릭터 폴더의 메시·가중치 데이터를 검사한다. `--preview`로 소프트웨어 미리보기 PNG를 지정하고, `--clothing-mask`로 0–255 의상 마스크를 선택한다. |
| `Characters/validate_tactical_outfit.py` | 캐릭터 폴더의 전술복 변형과 포즈를 검사한다. `--preview`로 미리보기 출력 위치를 지정한다. |
| `Validation/validate_tactical_motion.cpp` | 게임 프로젝트에 포함되지 않은 C++/OpenGL 자세 검증 도구 소스다. |

예를 들어 검증 미리보기는 다음처럼 저장할 수 있다.

```powershell
python .\Tools\Characters\validate_character.py .\Assets\Models\Player\여캐 --preview .\.build\여캐-preview.png
```

`Tools/Characters/paths.py`는 기본 입력 루트 `Assets/`와 원본/파생 자료 루트 `ArtSource/`를 정의한다. 게임에 사용하는 결과는 `Assets/Models/Player/`에 두고, 원본과 제작 메타데이터는 `ArtSource/Characters/<캐릭터>/`에 보관한다. 스크립트 실행은 게임 빌드나 런타임 검증을 대신하지 않는다.
