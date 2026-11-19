# 성인 남녀 신체·외부 생식기 메시 후보

취득·확인: 2026-11-19 (한국 시간). 무료 상업용 게임에 사용할 신체 메시를 찾는 작업이다. 게임 캐릭터 파일은 수정하지 않았다.

## 원본과 확인 방법

- 공식 파일별 목록: https://files.makehumancommunity.org/sites/default/files/assets.json
- 목록 접근 안내: https://github.com/makehumancommunity/mpfb2/issues/462
- CC0 설명: https://creativecommons.org/publicdomain/zero/1.0/
- 다운로드한 `assets.json` 원문과 각 폴더의 `source-metadata.json`을 보존했다. `mesh-inventory.json`은 OBJ의 정점/면 수, 경계, SHA-256, 라이선스 헤더를 기록한다.
- CC0로 일치하는 아래 파일은 상업 이용·수정·재배포 및 출처 표기 의무가 없는 조건으로 게시되어 있다. 소프트웨어 코드의 GPL/AGPL과 개별 그래픽 자산의 라이선스를 구분했다.

## 추천 후보: 목록과 메시/설정 파일의 CC0 표기가 일치

| 폴더 | 제작자 / 제목 | 포함 범위와 한계 |
| --- | --- | --- |
| `3396/` | spamrakuen, 원형 wolgade / Female generic with simplified genitals (fixed) | 여성 전신 OBJ, 13,916 정점 / 13,914 면. 외음부가 단순화된 몸체. 상세 해부학 모델로 간주하지 않는다. OBJ와 proxy 헤더 모두 CC0. |
| `3167/` | beebo123 / RealisticLabiaMedium | 위 계열 여성 몸체용 음순 세부 부속 OBJ, 1,260 정점 / 1,156 면. 전신 몸체가 아니며 결합·피팅이 필요하다. OBJ/mhclo 헤더 CC0. 피부 텍스처·재질 경로는 아직 구성하지 않았다. |
| `3223/` | ukiyoe / man genital | 남성 외부 생식기 부속 OBJ, 607 정점 / 588 면. 전신 몸체가 아니며 CC0 남성 몸체에 결합해야 한다. OBJ/mhclo/mhmat 모두 CC0. 목록의 제작자는 ukiyoe이나 파일 author 필드는 unknown이다. |

여성 몸체 3396은 단순화된 외음부이며, 3167 부속을 사용해 세부를 보완하는 후보 조합이다. 남성 3223은 성기 부속만이므로, 이미 받은 Blender Studio 남성 몸체나 MakeHuman CC0 기본 몸체와 연결하는 작업이 별도로 필요하다. 완성된 남녀 게임 캐릭터나 즉시 적용 가능한 리그라고 보고하지 않는다.

## 추천에서 제외: 라이선스 표기 불일치

- `735/` Adult Male Genitalia: 목록은 CC0이나 OBJ와 proxy 내부는 MHteam / AGPL3. 같은 이름의 다른 에셋과 혼동하지 않는다.
- `880/` Adult female genitalia remapped: 목록과 OBJ는 CC0이나 proxy 내부는 MHteam / AGPL3이며 설명에도 기존 프록시를 수정했다고 기록되어 있다.

위 두 항목은 조사 증거로만 보관한다. CC0 자산으로 확정하거나 런타임에 포함하지 않는다. AGPL의 상업 이용 자체가 금지라는 뜻이 아니며, 해당 파일별 배포 조건이 이번 확인으로 정리되지 않았다는 뜻이다.

## 실제 확인 범위

원본 다운로드, 메타데이터·라이선스 헤더 대조, OBJ 정점/면과 경계·해시의 정적 확인을 수행했다. 기능·해부학 범위는 제작자 설명에 근거하며, 세부 형상의 Blender 시각 검사와 리깅·변형·접합 검증은 아직 하지 않았다. 수술용 정확도나 내부 생식기·항문까지 모두 포함된다는 보장은 없다. 시트 얼굴 유지·의상 탈착 구현과 게임 실행은 별도 작업이다.
