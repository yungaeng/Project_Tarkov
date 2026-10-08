# 빌드 환경 가이드

## 의존성과 도구

- Windows, Visual Studio 2022의 C++ 데스크톱 개발 도구(MSVC v143), Windows SDK가 필요합니다.
- `vcpkg.json`은 GLAD 1(OpenGL 3.3 + loader), GLM, Assimp, stb, GLFW를 선언하고 vcpkg baseline을 고정합니다.
- GLFW도 vcpkg의 `glfw3` 패키지로 복원합니다. NuGet 복원은 필요하지 않습니다.
- vcpkg 설치 경로는 `VCPKG_ROOT` 환경변수로 지정합니다. 기본값은 `%USERPROFILE%\vcpkg`입니다. 해당 경로에 부트스트랩된 `vcpkg.exe`가 있어야 합니다.
- 최초 빌드는 인터넷 연결이 필요하며 의존성 빌드 때문에 시간이 걸립니다.

## 빌드

Visual Studio에서 `Project_Tarkov/Project_Tarkov.sln`을 엽니다.
Debug 또는 Release / x64를 선택해 솔루션을 빌드합니다. vcpkg가 필요한 라이브러리를 복원합니다.
다른 vcpkg 설치 경로를 사용하려면 Visual Studio 실행 전에 `VCPKG_ROOT` 환경변수를 설정합니다.
프로젝트의 vcpkg manifest 통합이 헤더, 라이브러리 연결 및 실행에 필요한 DLL 복사를 처리합니다.
GLAD는 라이브러리로 연결하므로 별도의 `glad.c`를 추가하지 않습니다.
`Graphics/Texture.cpp`가 stb_image 구현을 제공하므로 다른 파일에서 구현 매크로를 중복 정의하지 않습니다.

의존성은 `vcpkg_installed`, 다운로드 및 중간 파일은 `.build`에 생성됩니다.
두 폴더는 Git에서 제외됩니다. Visual Studio에서도 솔루션을 열어 Debug 또는 Release / x64를 선택해 빌드할 수 있습니다.
Win32 구성은 이번 검증 대상에 포함하지 않습니다.

## 실행

빌드 시 `Assets` 폴더가 EXE 옆에 자동으로 복사됩니다. Debug 폴더의 EXE를 직접 실행할 수 있습니다. 현재 작업 폴더에 `Assets`가 없으면 EXE 폴더를 기준으로 에셋을 읽습니다. Debug 실행 예시:

```powershell
Push-Location .\Project_Tarkov\Project_Tarkov
try {
    & ..\x64\Debug\Project_Tarkov.exe
}
finally {
    Pop-Location
}
```

Visual Studio 디버깅 작업 디렉터리는 `$(ProjectDir)`로 설정합니다.
OpenGL 3.3을 지원하는 그래픽 드라이버가 필요합니다.
`Assets/Shaders/cube.vs`, `cube.fs`, `Assets/Models/Player/Ch22_nonPBR.fbx`가 있어야 합니다.

창과 모델 표시, WASD 이동, 마우스 회전, Shift/Ctrl 속도 변경, I 커서 전환, F11 전체 화면, Q 종료를 확인합니다.
입력·자원 검증 절차는 [VALIDATION.md](VALIDATION.md)를 참고하세요.

## 오류 해결

- `vcpkg.exe` 또는 MSBuild가 없으면 위 도구 설치와 경로 설정을 확인하세요.
- 패키지 다운로드 실패 시 인터넷 및 프록시 설정을 확인하세요.
- 오래된 vcpkg에서 MSYS 다운로드가 실패하면 vcpkg 도구/스크립트를 갱신하거나 독립 실행형 `pkg-config`의 경로를 `PKG_CONFIG`로 지정하고 `VCPKG_KEEP_ENV_VARS`에 `PKG_CONFIG`를 포함하세요.
- `glad/glad.h` 또는 GLAD 심볼 오류 시 vcpkg 설치 단계가 성공했는지 확인하세요. GLAD 2 헤더로 교체하면 기존 API와 호환되지 않습니다.
- 에셋 로딩 오류는 EXE 옆의 `Assets` 폴더를 확인하고 프로젝트를 다시 빌드하세요. 다른 위치로 배포할 때는 EXE, DLL, `Assets` 폴더를 함께 복사하세요.

## 인벤토리 UI

ImGui와 glfw-binding/opengl3-binding도 vcpkg manifest로 복원합니다. 별도 UI 소스를 프로젝트에 복사할 필요는 없습니다.
