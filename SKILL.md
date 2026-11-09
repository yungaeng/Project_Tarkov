# Project Skill & Workflow Guidelines

## Project: Tarkov

### Core Rule (핵심 지침)
코드를 수정하거나 새로운 기능을 추가하는 작업을 수행할 때, **별도의 빌드(Build) 및 테스트(Test) 과정을 거치지 않고**, 오직 **문서화(Documentation) 작업만 완료한 후 작업을 종료**합니다.

---

### Task Execution Workflow (작업 진행 절차)

1. **코드 수정 및 기능 추가 (Code Modification / Feature Addition)**
   - 요청된 코드 변경사항 적용 또는 새 기능 구현
   - 변경 사항이 최소한의 일관성을 유지하도록 작성

2. **문서화 작업 (Documentation Only)**
   - 변경된 코드 및 추가된 기능에 대해 관련 문서 업데이트 (예: `README.md`, API 문서, 주석, 변경 이력 등)
   - 구현 내용, 파라미터, 사용법 등을 명확히 기재

3. **작업 종료 (Completion)**
   - ⚠️ **빌드 수행 안 함 (No Build)**
   - ⚠️ **테스트 수행 안 함 (No Test)**
   - 문서화 완료 즉시 모든 작업 종료 및 보고

---

### Documentation Checklist (문서화 체크리스트)
- [ ] 추가/수정된 기능의 목적 작성
- [ ] 주요 함수 및 클래스의 파라미터/반환값 설명 추가
- [ ] 필요한 경우 설정 방법 및 연동 가이드 업데이트