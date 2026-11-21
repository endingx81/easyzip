# AI 에이전트 개발 행동 강령 및 실수 방지 지침 (Core Rules & Anti-Patterns)

**[경고] 본 문서는 단순 기능 설명서나 커밋 로그가 아닙니다.**  
AI 에이전트(당신)가 개발 중 **실제로 저질렀던 치명적인 실수들을 박제하고 영구히 재발을 방지**하기 위한 강령입니다.  
다른 프로젝트에서도 공통으로 적용되는 **공통 규칙 및 빌드 환경**이 최상단에 배치되어 있으므로, 새로운 작업을 진행하기 전 반드시 숙지하세요.

### 🚨 최우선 원칙 1: 코딩/개발 지시 수신 시 구현 전 계획(Plan) 사전 공유 필수 (선 계획, 후 구현)
- **철칙**: 사용자가 기능 추가, 버그 수정, 리팩토링 등 **코드베이스를 수정하는 코딩 작업을 지시할 때**, 임의로 코드를 곧바로 작성하거나 수정하는 구현 작업을 시작하기 전에 반드시 무엇을 어떻게 진행할 것인지 명확한 실행 계획(Plan)을 먼저 상세히 설명하고 진행할 것.
- **예외(실행/운영 명령 즉시 수행)**: 빌드, 테스트, 패키징, 릴리스 등록 등 **이미 동작과 목표가 명확한 실행/운영 명령(Operational Execution)**은 계획 확인 질문으로 사용자를 번거롭게 되묻지 말고 즉시 원스톱으로 실행할 것.
- **방지 목적**: 사전 계획 공유 없이 성급하게 코드를 수정하여 엉뚱한 방향으로 구현되거나 불필요한 시행착오와 번복이 발생하는 것을 원천 차단함.

### 🚨 최우선 원칙 2: 실수 발생 즉시 자발적 문서 박제 (사용자 요청 대기 절대 금지)
- **실제 안일한 태도**: 빌드 실패, UI 깨짐, 텍스트 짤림 등의 치명적인 실수를 저지르고 수정 코드를 반영했음에도, "수정 완료되었습니다"라고만 보고하고 사용자가 "GEMINI.md에 추가할 건 없냐?"고 직접 지적할 때까지 가만히 있는 수동적인 태도를 보임.
- **철칙**: 버그 수정이나 실패 수습이 완료되면, **사용자가 묻기 전에 무조건 작업 완료 단계에서 본 문서(GEMINI.md)에 '실제 실수'와 '방지 원칙'을 자동으로 기록**하고 그 사실을 함께 보고할 것. 사용자가 먼저 문서를 챙기게 만드는 것은 에이전트의 심각한 직무유기임.
- **지식 자동 업데이트**: 향후 개발을 진행하면서 특정 프로젝트에 국한되지 않는 **일반적인 Win32 C++ 개발, 성능 최적화, 혹은 UI/UX 개선 패턴**을 새롭게 발견하거나 구현하게 되면, 스스로 판단하여 '개발 규칙' 항목에 해당 내용을 자동으로 요약 및 추가할 것 (단, 너무 지엽적인 예외 사항은 억지로 추가하지 않음).

---

# 1. 모든 C++/Win32 프로젝트 공통 필수 규칙 (Global & Common Rules)

다른 프로젝트에서도 동일하게 준수해야 하는 최우선 공통 규칙들입니다.

### ⚙️ 빌드 및 CLI 명령어 규칙 (Build & Environment)
- **절대 경로 MSBuild 사용**: Windows 환경 변수(PATH)에 `msbuild`나 `cmake`가 등록되어 있지 않은 경우가 많으므로 터미널에서 직접 실행하지 말 것.  
  반드시 Visual Studio 2022 Community MSBuild 절대 경로를 사용:
  `& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" easyzip.sln /p:Configuration=Release /p:Platform=x64`
- **인클루드 디렉터리 표준화**: 하위 폴더 소스들이 루트 헤더를 참조할 때 상대 경로 지옥(`../../`)이나 `C1083` 오류가 발생하지 않도록, `.vcxproj`의 `AdditionalIncludeDirectories`에 `$(ProjectDir);%(AdditionalIncludeDirectories)`를 필수로 등록할 것.
- **Git 명령어 임의 실행 절대 금지**: 사용자의 명시적인 지시가 없는 한 `git diff`, `git log`, `git checkout`, `git reset` 등 Git 명령어를 절대 임의로 실행하지 말 것.
- **바이너리 파일 잠금(`LNK1104`) 간과 및 미검증 완료 보고 절대 금지**:
  - **실제 실수**: 사용자가 테스트 중 프로그램을 켜둔 상태에서 빌드를 돌리면 `LINK : fatal error LNK1104: '...exe' 파일을 열 수 없습니다`로 링크가 실패함. 이 에러를 대충 넘기고 "수정 완료"라고 보고하여, 사용자가 버그가 전혀 고쳐지지 않은 구버전 바이너리를 보고 극도로 분노하게 만듦.
  - **방지 원칙**: 코드를 수정한 뒤에는 반드시 MSBuild 출력에서 **`빌드했습니다. 오류 0개, 경고 0개`와 `.exe` 생성 성공**을 100% 확인한 후에만 보고할 것. 실행 중인 프로세스로 인한 `LNK1104` 발생 시 프로세스 종료 후 재빌드를 완결할 것.
- **신규 설치 CLI 도구 환경 설정 사용자 떠넘기기 및 세션 PATH 미반영 금지**:
  - **실제 실수**: 도구 설치 후 에이전트가 직접 터미널 환경 변수를 갱신하거나 절대 경로로 후속 작업을 자동화할 수 있는 권한(`run_command`)이 있음에도 불구하고, 사용자에게 수동 작업을 떠넘기거나 부모 IDE 환경 변수 미반영으로 인한 오류를 유발함.
  - **방지 원칙**:
    1. 도구 설치 후 필요한 환경 변수 갱신, 바이너리 검증, 후속 작업은 **에이전트가 직접 실행(`run_command`)하여 사용자가 곧바로 쓸 수 있는 상태로 완결**해 둘 것.
    2. 사용자에게 터미널 명령어를 안내할 때는 IDE 재시작 전까지 환경 변수가 상속되지 않는 문제를 감안하여 **무조건 검증된 절대 경로**를 기본으로 제공할 것.
- **배포 산출물 중복 생성(`EasyZipSetup.exe` vs `EasyZip_Setup.exe`) 및 버전 미갱신 절대 금지**:
  - **실제 실수**: 과거 릴리스 파일명에 언더스코어(`EasyZip_Setup.exe`)가 섞여 있었다는 이유로, 표준 명칭인 `EasyZipSetup.exe` 외에 `EasyZip_Setup.exe`를 임의로 복사 생성하여 동일한 설치 파일 2개가 중복으로 배포 폴더와 릴리스에 올라가게 만듦.
  - **방지 원칙**:
    1. 설치 파일은 변형 파일명(언더스코어 등)을 일절 허용하지 않으며, 무조건 공식 표준 명칭인 **`EasyZipSetup.exe` 딱 1개**만 단일 생성 및 업로드할 것.
    2. 압축 프로그램(EasyZip)은 셸 메뉴 및 확장자 연결이 핵심이므로 비실용적인 무설치 포터블 대신, 브라우저의 `.exe` 직접 다운로드 차단 경고를 회피할 수 있는 **`EasyZipSetup.zip`**(`EasyZipSetup.exe` 압축본)을 표준 압축 배포본으로 제공할 것.
    3. 배포 폴더와 릴리스에는 항상 표준 바이너리 2개(**`EasyZipSetup.exe`** 1개, **`EasyZipSetup.zip`** 1개)만 정갈하게 유지할 것.
    4. 코드/리소스 수정 및 배포가 발생하면 반드시 시맨틱 버저닝(SemVer) 규칙에 따라 버전 번호를 명확히 상향(`v1.0.0` ➡️ `v1.0.1`)하여 릴리스할 것.
- **설치 프로그램(Setup.exe) 생성 시 외부 도구 탐색 낭비 금지 및 EasyInstall 표준 파이프라인 준수**:
  - **실제 실수**: 설치 프로그램(Setup.exe) 생성 지시를 받았을 때, 프로젝트 환경에 구축된 자체 빌더를 확인하지 않고 Inno Setup(`iscc`), NSIS(`makensis`) 등 엉뚱한 외부 도구를 찾으며 헤매고 시간을 지체함. 동일 워크스페이스 그룹(`D:\coding\visualstudio\easyinstall`)에 전용 인스톨러 빌더가 이미 완성되어 있음에도 이를 즉시 활용하지 못함.
  - **방지 원칙 (표준 배포 패키징 파이프라인)**:
    1. **단일 설치 파일 빌더 절대 경로**:  
       `& "D:\coding\visualstudio\easyinstall\bin\Release\easyinstall.exe" -i "<입력_바이너리_또는_폴더>" -o "<출력경로>\EasyZipSetup.exe" -n "EasyZip" -v "<버전>" -p "ai-open.kr" -m "EasyZip.exe"`
       > [!CAUTION]
       > **단일 파일 vs 폴더 패키징 맹점 방지**: DLL(`EasyZipShell.dll` 등) 동적 의존 파일이 있는 프로젝트에서 `-i`에 단일 `exe` 경로만 전달하면 부속 파일들이 패키징에서 전부 누락됨. 따라서 반드시 **바이너리 전체가 빌드된 폴더(`bin\Release`)를 `-i`로 전달**하여 모든 종속 파일이 100% 동봉되도록 할 것!
    2. **설치 파일 압축 아카이브(EasyZipSetup.zip) 생성**:  
       `powershell -Command "Compress-Archive -Path '<출력경로>\EasyZipSetup.exe' -DestinationPath '<출력경로>\EasyZipSetup.zip' -Force"`
    3. 설치 파일 생성 요구 시 타사 도구(`iscc`, `makensis` 등)를 일절 찾지 말고, 위 `easyinstall.exe` 절대 경로를 이용하여 즉시 1초 만에 원스톱으로 빌드할 것.
    4. **인스톨러(Setup.exe)의 기존 프로세스 감지 시 대상 디렉터리 종속 맹점 방지 원칙**:
       - `findRunningProcesses`에서 프로세스를 감지할 때 단순히 `targetDir` 내부 경로만 검사하면, 사용자가 포터블 폴더, 개발/빌드 폴더, 혹은 이전 경로에서 실행 중인 동일 프로그램을 감지하지 못하고 그냥 지나쳐 창이 닫히지 않고 설치되는 결함이 발생함.
       - 반드시 `curExe == mainExeName`을 최우선 검사하여 **어느 경로에서 실행 중이든 동일한 실행 파일명이면 무조건 프로세스 종료 대상(PID)에 포함**시켜, 100% 안전하게 창을 닫고 교체 설치를 완결할 것.

### 🛠️ 도구 사용 실수 방지: 줄 번호 포함 금지
- **방지 원칙**: 파일 수정 도구를 호출할 때는 파일의 **순수한 실제 코드(공백, 들여쓰기 포함, 줄 번호 절대 제외)**만을 정확하게 지정할 것.

### 🖥️ UI 컨트롤 라이프사이클 4단계 및 너비·줄바꿈·동적 리사이징 원칙 ('반쪽짜리 구현' 금지)
- **동적 레이아웃 및 리사이징 (WM_SIZE 처리)**:
  - `.rc` 파일에 정의된 컨트롤 크기 및 위치는 픽셀이 아닌 **DLU(Dialog Units)** 기반이므로, 시스템 DPI나 시스템 폰트에 따라 렌더링되는 크기가 달라집니다.
  - 따라서 `WM_SIZE` 핸들러에서 하드코딩된 픽셀 값(예: `width - 90`)을 사용한 위치/크기 계산을 피해야 합니다.
  - 항상 `GetWindowRect`로 컨트롤의 실제 렌더링 크기(width/height)를 구하거나, `MapDialogRect`를 통해 DLU를 픽셀로 변환한 후, 부모 클라이언트 영역(`GetClientRect`)에 비례하여 동적으로 배치(우측/하단 정렬 등)하세요.
- **UI 구현 4단계 완결**: 리소스 선언(`.rc`) ➡️ 컨트롤 초기화(`WM_INITDIALOG`) ➡️ 조작/렌더링(`WM_COMMAND`, `WM_DRAWITEM` 등) ➡️ 반응형 재배치(`WM_SIZE`).
- **1줄 라벨 줄바꿈 방지**: 1줄 라벨에는 반드시 `SS_LEFTNOWORDWRAP`을 사용하고, 한글 기준 글자당 최소 14~15px 이상의 넉넉한 너비를 할당할 것.
- **체크박스/라디오 너비 계산**: `텍스트 글자 폭 + 체크마크(24px) + 안전 마진(20~30%)`을 무조건 합산하여 할당할 것.
- **최소 창 크기 방어**: 반응형 대화상자는 `WM_GETMINMAXINFO`를 필수로 구현하여 사용자가 창을 줄이더라도 컨트롤이 겹치거나 깨지지 않도록 최소 너비/높이를 제한할 것.

### 📑 Win32 단일 다이얼로그 탭 컨트롤(`SysTabControl32`) 구현 시 정적 컨트롤(`IDC_STATIC`) 중복 충돌 방지 및 그룹핑 원칙
- **방지 원칙**:
  1. 탭 전환 대상이 되는 모든 정적 라벨과 그룹박스는 `resource.h`에 명시적인 고유 ID(`IDC_GRP_TAB0_1`, `IDC_LBL_TAB0_DESC` 등)를 반드시 부여할 것.
  2. 대화상자 소스코드에서 각 탭 인덱스별 컨트롤 ID 정수 배열(`s_tab0Controls[]`, `s_tab1Controls[]` 등)을 선언하고, `TCN_SELCHANGE` 발생 시 해당 탭의 컨트롤만 `SW_SHOW`, 타 탭의 컨트롤은 `SW_HIDE`로 완벽하게 토글할 것.

### 🪟 Win32 탭 대화상자 내 정적 텍스트/체크박스 사각형 배경 얼룩 방지 (`WM_CTLCOLORSTATIC` 투명화)
- **방지 원칙**:
  1. `WM_INITDIALOG`에서 `EnableThemeDialogTexture(hDlg, ETDT_ENABLETAB)`을 호출하여 부모 대화상자의 테마 텍스처를 활성화할 것.
  2. 다이얼로그 프로시저에서 `WM_CTLCOLORSTATIC` 메시지를 가로채어 반드시 `SetBkMode((HDC)wParam, TRANSPARENT);`를 설정하고 `return (INT_PTR)GetStockObject(NULL_BRUSH);`를 반환할 것.

### 🎨 깜빡임 없는 UI 업데이트 (In-place Update & Notification Suppression)
- 리스트뷰(`ListView`) 등의 아이템 상태가 일부 변경되었을 때, 전체 리스트를 지우고 새로고침(`DeleteAllItems` 후 재로드)하지 말 것.
- 화면 깜빡임과 스크롤 초기화를 방지하기 위해 `ListView_SetItemText` 등을 사용하여 **변경된 특정 행(Row)만 부분 업데이트(In-place Update)** 해야 함.
- 대량의 아이템 갱신 시 불리언 가드(`g_bUpdating = true`)와 `WM_SETREDRAW`를 걸어 `LVN_ITEMCHANGED` 연쇄 이벤트 폭주 및 화면 깜빡임을 원천 차단할 것.

### 🖌️ Win32 Dialog 내 Custom Draw 처리 (`DWLP_MSGRESULT`)
- DialogBox 기반의 윈도우(DialogProc)에서 `WM_NOTIFY` 메시지의 `NM_CUSTOMDRAW`를 처리할 때, 직접 return 값을 반환하면 시스템이 인식하지 못하고 무시하는 경우가 발생함.
- 반드시 `SetWindowLongPtr(hWnd, DWLP_MSGRESULT, 반환값)`을 호출하여 결과를 명시한 후, `return (INT_PTR)TRUE;`를 반환해야 정상적으로 렌더링을 가로채거나(`CDRF_NOTIFYPOSTPAINT`) 넘길 수 있음.

### 🌲 TreeView 폴더 확장 시 Auto-Scroll(화면 튐) 방지
- 노드 삽입 직전 `GetScrollPos(hTree, SB_VERT)`로 스크롤 위치를 기억하고, `PostMessage` 등을 이용해 비동기로(즉시) 해당 스크롤 위치를 복구(`WM_VSCROLL`)하여 화면이 그 상태 그대로 유지되도록 처리해야 함.

### 🖥️ 고DPI 및 다중 모니터 환경에서의 폰트/창 비트맵 스트레칭 방지 (Per-Monitor V2 DPI & MulDiv 스케일링)
- 구형 API인 `SetProcessDPIAware()`는 프로세스를 단일 시스템 DPI에 고정시킴. 반드시 런타임에 `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`를 최우선 적용하여 DWM 가상화 확대를 원천 차단해야 함.
- 모든 폰트 높이(`CreateFontW`) 및 윈도우 배치 좌표/패딩은 `-MulDiv(기본픽셀, dpi, 96)`로 현재 모니터의 실제 DPI(`GetDpiForWindow`)에 비례하여 동적으로 계산하고, `WM_DPICHANGED`와 `WM_SETTINGCHANGE` / `WM_THEMECHANGED` 메시지를 핸들링하여 배율 및 테마 변화 시 레이아웃과 폰트를 즉시 갱신해야 함.

### 🖼️ 고DPI 및 다중 환경 대응을 위한 아이콘(.ico) 멀티 해상도 패키징 원칙
- 반드시 256×256 (PNG 압축), 128×128, 64×64, 48×48, 32×32, 24×24, 16×16 (32-bit RGBA) 전체 표준 규격 프레임을 단일 `.ico` 파일에 통합 패키징하여 배포할 것.

### 🔤 C++ 트레일링 역슬래시(`\`) 줄바꿈(Line-Splicing) 버그 방지
- C/C++ 소스 및 헤더의 한 줄 주석(`//`) 끝에는 절대로 단독 역슬래시(`\`)를 두지 말고, `\\` 또는 `(예: C:\\)` 등으로 처리할 것.

### 🔒 멀티스레드 동기화 뮤텍스(Lock) 내부 동기식 디스크/네트워크 I/O 배제
- 워커 스레드 동기화 임계 영역(Mutex Lock) 내부에서는 디스크 시스템 콜을 일절 배제하고 **순수 RAM 메모리 해시셋/큐 조회 및 삽입**만 수행할 것.

### 📜 PowerShell 텍스트 파일 조작 시 UTF-8 인코딩 강제
- 파일 I/O를 수행하는 셸 스크립트를 작성할 때는 반드시 `-Encoding UTF8` 파라미터를 명시하여 인코딩 깨짐을 원천 차단할 것.

### 🖥️ Windows 배치 스크립트(.bat) 생성/수정 시 CP949(EUC-KR) 인코딩 강제
- 배치 파일(`.bat`)에 한글 안내 메시지(`echo ...`)나 주석이 포함되는 경우 반드시 한국어 Windows ANSI 규격인 **CP949(EUC-KR)**로 명시적으로 인코딩하여 저장할 것.

### 📝 C++ 파일/스크립트 I/O 시 std::wofstream 사용 절대 금지
- 스크립트/배치 파일이나 설정 파일 생성 시 `std::wofstream`을 절대 쓰지 말 것. 반드시 명시적인 UTF-8 변환(`WideCharToMultiByte(CP_UTF8, ...)` 또는 `toUtf8` 헬퍼)을 거쳐 `std::ofstream` + 바이너리(`ios::binary`)로 출력할 것.

### ⚠️ Windows 헤더 min/max 매크로와 C++ <algorithm> 충돌 방지
- `#define NOMINMAX`를 Windows 헤더 인클루드 전에 선언하여 매크로 오염을 사전에 차단할 것.

---

# 2. EasyZip 프로젝트 특화 실수 및 개발 규칙 (Project-Specific Traps)

본 프로젝트(고속 압축/해제 아카이버, 윈도우 쉘 탐색기 연동, 64비트 COM 쉘 익스텐션) 특성상 준수해야 할 필수 규칙입니다. (개발 진행 중 발견되는 실수 및 원칙을 지속적으로 박제 및 갱신)

### 📦 Windows 매니페스트(app.manifest) 중복 선언 충돌 방지 (`CVT1100`, `LNK1123`)
- **실제 실수**: `.rc` 리소스 스크립트에 `1 24 "app.manifest"`를 기재한 상태에서 `.vcxproj`에 `<Manifest Include="res\app.manifest" />`를 중복 등록하여, 링크 단계에서 `CVTRES : fatal error CVT1100: 리소스가 중복되었습니다. 형식:MANIFEST, 이름:1` 및 `LNK1123` 발생.
- **방지 원칙**: Visual Studio 2022 C++ 프로젝트에서는 `.vcxproj`의 매니페스트 도구(`/manifestinput`)로 XML 매니페스트를 주입하고, `.rc` 파일 내부에는 매니페스트 리소스를 중복 선언하지 말 것.

### ⚡ Named Pipe 단일 인스턴스 취합(`BatchCoordinator`) 시 비동기 OVERLAPPED 대기 필수
- **실제 실수**: 탐색기 다중 선택 우클릭 취합을 위해 Named Pipe 서버를 구성할 때, 동기식 `ConnectNamedPipe`를 사용하면 보조 클라이언트가 없는 단독 실행 시 최대 타임아웃까지 불필요하게 1.5초 이상 블로킹되는 딜레이가 발생함.
- **방지 원칙**: 파이프 생성 시 `FILE_FLAG_OVERLAPPED`를 필수로 지정하고, `CreateEventW`와 `WaitForSingleObject`를 조합하여 비동기로 대기(디바운스 타임아웃 150ms)한 뒤 클라이언트가 없으면 지체 없이 즉시 작업을 시작하도록 구현할 것.

### 🧹 배포 패키징 시 순수 런타임 스테이징 폴더 분리 원칙
- **방지 원칙**: `bin\Release\`에는 컴파일 결과물인 `.pdb`, `.lib`, `.exp` 등 대용량 개발 파일들이 섞여 있으므로, 배포용 `EasyZipSetup.exe` 및 포터블 `.zip`을 패키징할 때는 반드시 `dist\EasyZip\`과 같은 독립된 스테이징 디렉터리를 생성하여 필수 실행 파일(`EasyZip.exe`, `EasyZipShell.dll`, `libarchive.dll`, `EasyZip.ico`) 4개만 정갈하게 동봉할 것.

### 🔗 Windows 11 파일 확장자 연결 및 UserChoice 맹점 방지 원칙
- **실제 실수**: Windows 11은 `UserChoice` 해시 보호 정책이 적용되어 단순히 `HKCU\Software\Classes\.zip`의 기본값만 수정해서는 기본 앱으로 동작하지 않음. 또한 `Capabilities\FileAssociations`나 기존 `UserChoice`가 `EasyZip.Archive`를 가리키고 있는데 `EasyZip.Archive\shell\open\command`를 등록하지 않아, 사용자가 탐색기에서 더블클릭해도 "연결 프로그램을 찾을 수 없습니다" 오류가 발생함.
- **방지 원칙**:
  1. 범용 ProgID(`EasyZip.Archive`)와 개별 ProgID(`EasyZip.<ext>`) 모두에 반드시 `shell\open\command`(`"<exePath>" "%1"`), `DefaultIcon`, `FriendlyTypeName`을 100% 등록할 것.
  2. `HKCU\Software\Classes\Applications\EasyZip.exe\SupportedTypes`, `HKCU\Software\EasyZip\Capabilities\FileAssociations`, `HKCU\Software\RegisteredApplications` 및 `<.ext>\OpenWithProgids`를 완벽히 등록하여 윈도우 11 제어판 및 기본 앱 목록에 정식 등재시킬 것.
  3. 구형 COM API인 `IApplicationAssociationRegistrationUI::LaunchAdvancedAssociationUI`를 호출하면 Windows 11에서 "기본 앱을 변경하려면 [설정] > [앱] > [기본 앱]으로 이동하십시오"라는 무용한 파란색 팝업만 뜨고 이동하지 않음. 반드시 Windows 11 공식 딥링크 URI인 `ms-settings:defaultapps?registeredAppUser=EasyZip` (fallback: `ms-settings:defaultapps`)을 `ShellExecuteW`로 직접 호출하여 즉시 기본 앱 설정 화면으로 진입시킬 것.
  4. 설치 후 최초 실행 시(`.zip` 미연결 감지 시) 자동으로 환경설정 [파일 연결] 탭을 팝업하여 사용자가 누락 없이 연결하도록 안내할 것.

### 🛑 Windows 11 탐색기 더블클릭 시 "열 앱 선택 (신규)" 팝업 및 ProgID 불일치 방지 원칙
- **실제 실수**: 설정 앱에서 기본 앱으로 지정했음에도 불구하고, 탐색기에서 `.zip` 더블클릭 시 "이 .zip 파일을 열 앱 선택"과 함께 `EasyZip (신규)` 플라이아웃이 계속 나타남.
- **원인**:
  1. `Capabilities\FileAssociations`(`EasyZip.Archive`)와 `Software\Classes\.zip`(`EasyZip.zip`) 간 ProgID 명칭 불일치로 Windows Shell이 두 개의 서로 다른 핸들러가 경합하는 것으로 오인함.
  2. Windows Shell의 정식 데스크톱 실행 파일 등록 키인 `HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\EasyZip.exe` 누락으로 미검증 앱(`신규`)으로 분류됨.
- **방지 원칙**:
  1. `Capabilities\FileAssociations`와 `Software\Classes\<.ext>` 기본값 ProgID를 모두 `EasyZip.Archive`로 100% 동일하게 일원화할 것.
  2. `HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\EasyZip.exe`에 `(Default)=exePath` 및 `Path=exeDir`을 필수로 등록하여 Windows 11 셸이 정식 데스크톱 앱으로 즉시 인식하도록 할 것.

### 🌐 libarchive 다국어 파일명 깨짐(모지바케) 방지 및 코드페이지 엔진 규칙
- **실제 실수**: 압축파일 내부 한글 파일명이 외계어(`¿ì¸®³ª¶ó` 등)로 깨져서 목록에 표시되거나 추출됨.
- **원인**:
  1. `wWinMain` 시작 시 `setlocale(LC_ALL, "")`을 호출하지 않아 CRT가 영문 `"C"` 로케일로 동작.
  2. `libarchive` 리더 초기화 시 `hdrcharset` 옵션을 미지정하여, UTF-8 플래그가 없는 국내 압축파일(ZIP/RAR/TAR 등, 99% CP949)을 OEM CP437/Latin-1로 오인 해석함.
  3. ZIP 압축 생성(`Compress`) 시 `zip:hdrcharset=UTF-8` 미지정으로 타 압축 프로그램에서도 언어가 깨질 수 있었음.
- **방지 원칙**:
  1. 프로그램 진입점 최상단에서 `std::setlocale(LC_ALL, "");`를 필수로 호출할 것.
  2. 아카이브 읽기 시 `archive_read_set_options(a, "hdrcharset=CP949")` (또는 사용자 지정 코드페이지)를 반드시 주입하고, `archive_entry_pathname_w` 및 `archive_entry_pathname_utf8`, fallback 멀티바이트 스마트 디코더(`EncodingHelper::DecodeSmart`) 3중 방어 체계를 가동할 것.
  3. 신규 ZIP 생성 시 `archive_write_set_format_option(a, "zip", "hdrcharset", "UTF-8")`를 필수로 지정하여 국제 표준 UTF-8 플래그를 보장할 것.
  4. 일본어(Shift-JIS / CP932), 중국어(GBK / CP936) 등 해외 아카이브에 대응할 수 있도록 메인 윈도우 상단에 `코드페이지(&C)` 선택 메뉴를 제공하여 즉각적인 재해석이 가능하도록 할 것.

### 🖼️ Win32 툴바 이미지 리스트 기본값(Index 0) 빈 문서 아이콘 렌더링 방지 원칙
- **실제 실수**: 툴바 컨트롤에 시스템 공유 이미지 리스트(`hSysIcons`)를 연결한 후 모든 버튼의 `iBitmap`을 `0`으로 지정하여, 모든 버튼에 윈도우 기본 미인식 문서 아이콘(하얀색 빈 페이지)이 출력됨.
- **방지 원칙**: 툴바 버튼에는 시스템 공유 이미지 리스트의 0번 인덱스를 재활용하지 말고, 툴바 전용 `HIMAGELIST`를 생성하여 각 기능에 맞는 명확한 시스템 아이콘(열기, 압축, 풀기, 검사, 삭제, 설정 등)을 개별 추출 및 매핑(`iBitmap = 0, 1, 2, ...`)하여 직관적인 UX를 제공할 것.

### 🖱️ 윈도우 탐색기 우클릭 셸 확장(COM DLL & 정적 Verbs) 누락 방지 및 정적 Verbs 오염 금지 원칙
- **실제 실수**:
  1. `EasyZipShell.dll`을 빌드해두고도 `FileAssociation::RegisterAll()` 및 설정 저장 단계에서 `DllRegisterServer`를 호출하지 않아 탐색기 우클릭 시 메뉴가 전혀 나타나지 않음.
  2. Windows 11 노출을 핑계로 `EasyZip.Archive\shell\Extract*`에 정적 동사(Static Shell Verbs)를 등록하여, 단일/다중 파일 개수 감지 및 환경설정 On/Off 체크박스가 전혀 먹히지 않고 정적 텍스트("각각의 폴더에 압축 풀기")가 강제 알파벳순으로 노출되는 대참사 발생.
- **방지 원칙**:
  1. ProgID(`EasyZip.Archive\shell`)에는 오직 더블클릭 실행용 `shell\open\command`만 등록하고, 압축 풀기/압축하기 등 모든 컨텍스트 메뉴는 **`shellex\ContextMenuHandlers`를 통해 동적 DLL(`EasyZipShell.dll`)로 100% 일원화**할 것.
  2. 프로그램 시작 및 설정 저장 시 `LoadLibrary` + `DllRegisterServer`를 통해 COM CLSID 및 `ContextMenuHandlers`(`*`, `Directory`, `Folder`, `EasyZip.Archive`)를 필수로 등록하고, 과거 등록된 정적 동사(`Extract*`, `EasyZip.Compress`, `EasyZip.FastZip`)는 `RegDeleteTreeW`로 흔적도 없이 삭제할 것.

### 🛑 설치 프로그램(EasyInstall)의 실행 중 잠긴 DLL(`ERROR_SHARING_VIOLATION`) 갱신 맹점 방지 원칙
- **실제 실수**: 윈도우 탐색기(`explorer.exe`)가 셸 확장 DLL(`EasyZipShell.dll`)을 로드하고 있을 때, 인스톨러(Setup.exe)가 단순히 파일 쓰기 실패(`32`) 시 파일을 건너뛰어버려(기존 파일 재사용) 코드와 바이너리를 수정했음에도 구버전 DLL이 시스템에 영구 방치됨.
- **방지 원칙**:
  - 인스톨러에서 대상 DLL/바이너리가 프로세스에 의해 잠겨 `ERROR_SHARING_VIOLATION` 또는 `ERROR_ACCESS_DENIED`가 발생하면, 기존 파일을 `.old_<timestamp>`로 이름 변경(`MoveFileExW`)하고 `MOVEFILE_DELAY_UNTIL_REBOOT`로 재부팅 시 삭제를 예약한 뒤 신규 DLL을 즉시 추출/생성하여 100% 최신 바이너리로 교체할 것.

### 📜 Windows 리소스 스크립트(.rc) 문자열 내 큰따옴표 이스케이프 (`RC2116`, `RC2114`)
- **실제 실수**: `.rc` 파일의 대화상자 컨트롤 캡션 텍스트에 큰따옴표를 포함할 때 C/C++ 이스케이프(`\"`)를 사용하여 `fatal error RC2116: expecting number for ID` 및 `RC2114: expected control class name` 컴파일 실패 발생.
- **방지 원칙**: RC 스크립트 문자열 내부에서 큰따옴표(`"`)를 표현할 때는 C++ 스타일의 역슬래시(`\"`)가 아니라 반드시 **연속 두 개(`""`, 예: `"""{파일명}""에 압축 풀기"`)**를 사용하거나 대괄호(`[{파일명}]`) 등으로 표기할 것.

### 🧠 반디집 호환 스마트 압축 풀기("알아서 풀기") 최상위 폴더 구조 분석 및 셸 메뉴 분기 원칙
- **방지 원칙**:
  1. **알아서 풀기 (Smart Extract)**: 압축 파일 내부 엔트리를 스캔하여 최상위 디렉터리가 단 1개뿐이고 루트 레벨에 독립 파일이 없는 경우(`topLevelRoots.size() == 1 && !hasRootFiles`)에만 상위 폴더(`parentDir`)에 직접 풀고, 그 외(루트 파일 다수, 폴더 복수 개)에는 아카이브 파일명 폴더(`parentDir\{파일명}\`)를 생성하여 풀어야 불필요한 이중 폴더 생성을 완벽 방어함.
  2. **단일 vs 다중 파일 우클릭 메뉴 분기**:
     - 최상단: `여기에 압축 풀기(&X)` (사용자 설정 체크 시 1위로 노출)
     - 기본 노출: `알아서 풀기(&A)`
     - 그 다음: 단일 파일인 경우 `"{파일명}\"에 압축 풀기(&E)`, 다중 선택인 경우 `각각의 폴더에 압축 풀기(&E)`로 명확히 분기.
  3. **셸 확장 inproc 설정 동기화**: `EasyZipShell.dll`이 탐색기 메뉴 팝업 시마다 무거운 IPC나 디스크 파일 I/O를 하지 않도록 `HKCU\Software\EasyZip\Shell` 레지스트리에 On/Off 플래그를 실시간 동기화하여 0ms 렌더링 성능을 보장할 것.

### 🎛️ 아카이버 메인 윈도우 UI/UX 미니멀리즘 원칙 (핵심 버튼 중심 정돈)
- **방지 원칙**: 빠른 뷰잉 및 압축 해제에 집중할 수 있도록 메인 툴바에는 오직 **`[압축 풀기]`**와 **`[설정]`** 딱 2개 버튼만 정갈하게 배치하고, 메뉴 바에서도 불필요한 사족 버튼(열기, 압축하기, 테스트, 삭제)을 깔끔하게 제거하여 직관적인 미니멀 UI를 유지할 것.

### ⚡ 대용량 아카이브 압축 해제 시 UI 갱신 스로틀링(Throttling) 필수 원칙
- **실제 실수**: `ArchiveEngine`의 `archive_read_data_block` 루프 내에서 매 블록(8KB~16KB)마다 UI 진행률 콜백(`onProgress`)을 호출하여 `SetDlgItemTextW` 및 `PeekMessageW` 메시지 펌프를 100만 번 이상 실행함. 이로 인해 CPU가 UI 메시지 루프 처리에 갇혀 압축 해제 I/O 속도가 바닥으로 곤두박질치는 치명적 병목 발생.
- **방지 원칙**:
  1. 압축 해제 루프 내부에서는 시간(`msElapsed >= 60ms`) 및 바이트(`processedBytes >= 2MB`) 기반 스로틀링을 필수로 적용하여 초당 최대 16회 수준으로만 UI를 갱신할 것.
  2. 디스크 쓰기 시 `FILE_FLAG_SEQUENTIAL_SCAN` 플래그를 적용하여 Windows 커널의 순차적 캐시 프리페칭을 극대화할 것.

### 🧩 다운로드 중단/손상된 아카이브(Truncated) 감지 및 명확한 오류 안내 원칙
- **실제 실수**: 다운로드가 중단된 대용량 파일(예: 9.28GB 선언, 5.32GB만 수신)을 풀 때 libarchive가 파일 끝에서 `ARCHIVE_EOF`를 반환하자, 선언된 크기 대비 턱없이 부족하게 풀렸음에도 불구하고 성공으로 오인하여 경고 없이 종료됨.
- **방지 원칙**: 엔트리 헤더에 선언된 크기(`archive_entry_size`)가 유효한 일반 파일에서 실제 추출 바이트가 선언 크기보다 작게 끝난 경우, 즉시 손상/잘린 파일(Truncated)로 판정하고 사용자에게 명확한 오류/경고 대화상자(`선언 크기 vs 실제 크기`)를 출력할 것.

### 📁 탐색기 마우스 우클릭 메뉴 최상단 [새 폴더 만들기] 전역 노출 및 무창(Silent) 생성 원칙
- **실제 실수**:
  1. 새 폴더를 만든 후 편리하게 선택해주겠다는 명목으로 `SHOpenFolderAndSelectItems`를 호출하여, 새 폴더를 만들 때마다 **불필요한 새 파일 탐색기 창이 계속 팝업**되어 사용자를 몹시 불쾌하게 만듦.
  2. 동적 COM 셸 확장(`IShellExtInit` / `IContextMenu`)에만 의존하여 구현했더니, 바탕화면 빈 공간이나 폴더 빈 공간에서 타사 정적 동사(AnyCode, CMD 등)에 밀리거나 파일 선택 시에만 팝업되는 편차가 발생함.
- **방지 원칙**:
  1. **무창 고속 생성 (Silent & Instant)**: 새 폴더 생성 시 `SHOpenFolderAndSelectItems`를 절대 호출하지 말 것! 순수하게 `CreateDirectoryW`로 폴더 생성 후 `SHChangeNotify(SHCNE_MKDIR, SHCNF_PATHW, ...)` 및 `SHChangeNotify(SHCNE_UPDATEDIR, ...)` 2개 알림만 전송하고 0.01초 만에 조용히 프로세스를 종료할 것. 이렇게 해야 현재 열려있는 창에서 조용히 폴더가 나타나며 새 창이 절대 뜨지 않음.
  2. **정적 Shell Verbs와 `Position="Top"` 필수 등록**:
     - 바탕화면/폴더 빈 공간: `Software\Classes\Directory\Background\shell\EasyZip.NewFolder`
     - 폴더 우클릭: `Software\Classes\Directory\shell\EasyZip.NewFolder`
     - 파일 우클릭: `Software\Classes\*\shell\EasyZip.NewFolder`
     - 모든 키에 `Position = "Top"` 문자열 값을 등록하여, Windows Shell 컨텍스트 메뉴의 **절대 최상단(Index 0, 첫 번째 줄)**에 영구 고정시킬 것.
  3. **바탕화면 중복 등록 방지**: `Directory\Background`는 일반 폴더 빈 공간과 바탕화면 빈 공간을 모두 커버하므로, `DesktopBackground`에 추가로 등록하면 바탕화면 우클릭 시 메뉴가 2개 중복 노출됨. 반드시 `Directory\Background` 1개만 등록하여 중복을 차단할 것.
  4. 환경설정의 `GetMenuNewFolder()` On/Off 상태에 따라 위 정적 동사 키들을 즉시 등록하거나 `RegDeleteTreeW`로 완전 삭제하여 사용자 설정을 100% 동기화할 것.

### 📁 Windows 탐색기 표준 시스템 폴더 아이콘 추출 시 드라이브 루트(`C:\\`) 지정 맹점
- **실제 실수**: `SystemIconHelper`에서 폴더 아이콘을 시스템 이미지 리스트에서 조회할 때 `SHGetFileInfoW(L"C:\\", FILE_ATTRIBUTE_DIRECTORY, ...)`를 호출하여, 일반 노란색 서류철 폴더 아이콘이 아니라 **C: 로컬 디스크(하드 드라이브) 아이콘**을 바인딩함. 이로 인해 리스트뷰와 트리뷰의 모든 폴더가 드라이브 아이콘으로 출력되는 심각한 시각적 버그 발생.
- **방지 원칙**:
  1. 가상 폴더의 순수한 탐색기 노란색 폴더 아이콘을 가져올 때는 절대 드라이브 문자(`C:\`)를 넘기지 말고, 반드시 `L"folder"` (또는 `SIID_FOLDER`) 가상 디렉터리 경로를 전달할 것.
  2. 확장자가 없는 파일의 경우에도 `.tmp` 등을 임의로 붙이지 말고 순수 기본 파일(`L"dummy"`)로 조회하여 윈도우 기본 문서 아이콘을 정확하게 매핑할 것.

### 🚫 '여기에 압축 풀기' 기본값 OFF(Unchecked) 엄수 원칙
- **방지 원칙**: '여기에 압축 풀기'는 여러 파일이 뒤섞여 폴더/바탕화면을 어지럽힐 위험이 크므로 절대 기본 활성화하지 말 것. 기본 압축 풀기 메뉴는 오직 **`알아서 풀기(&A)`**와 **`각각의 폴더에 압축 풀기(&E)`** 2가지만 기본 ON(체크)으로 유지하고, `여기에 풀기`는 사용자가 환경설정에서 수동 체크하기 전까지 무조건 0(OFF)을 유지할 것.

### 🚫 설치/삭제 시 탐색기(explorer.exe) 강제 종료 및 작업표시줄 깜빡임 절대 금지 원칙 (무간섭 Rename-on-Replace)
- **실제 실수**:
  - `EasyZipShell.dll`이 탐색기에 잠겨 있을 때 파일 쓰기/삭제 실패를 피하겠다고 인스톨러/언인스톨러에서 `explorer.exe`를 강제로 종료함. 이로 인해 설치/삭제할 때마다 작업표시줄이 사라지고 화면이 깜빡거리며 열려있던 탐색기 창이 닫혀 사용자 경험을 심각하게 훼손함.
- **방지 원칙**:
  1. **탐색기 프로세스 강제 종료 절대 금지**: 어떤 경우에도 `explorer.exe`를 임의로 종료하거나 작업표시줄을 사라지게 만들지 말 것.
  2. **설치 시 무간섭 리네임 교체 (Rename-on-Replace)**: 탐색기가 기존 DLL을 물고 있어 `CreateFile`이 막히면(`ERROR_SHARING_VIOLATION`), 윈도우 표준 기술인 `MoveFileExW(..., tempOld, MOVEFILE_REPLACE_EXISTING)`로 파일 이름을 임시명(`.old_<timestamp>`)으로 바꾼 후 신규 DLL을 바로 생성하고, 임시 파일은 `MOVEFILE_DELAY_UNTIL_REBOOT`로 시스템 재부팅 시 자동 정리되도록 예약할 것. 이렇게 하면 탐색기나 작업표시줄을 전혀 건드리지 않고 0초 만에 조용히 최신 버전으로 교체됨.
  3. **삭제 시 안전 삭제 및 재부팅 정리 예약**: 언인스톨러에서 잠긴 DLL은 `.deleted`로 리네임 및 `MOVEFILE_DELAY_UNTIL_REBOOT`로 예약하고, `--unregister`와 `SHChangeNotify(SHCNE_ASSOCCHANGED)`를 호출하여 탐색기를 끄지 않아도 마우스 우클릭 메뉴가 그 즉시 사라지도록 할 것.

### 📁 IContextMenu 구현 시 Explorer의 `indexMenu` 슬롯 준수 및 임의 위치(Position 0) 침범 금지 원칙
- **실제 실수**: 사용자의 "최상단 노출" 요구에 맞춰 탐색기가 전달한 공식 슬롯(`indexMenu`)을 무시하고 컨텍스트 메뉴의 `0`번 위치에 메뉴를 강제 삽입함. Windows Shell은 `0`번을 시스템 예약 기본 동사(Open, Play 등) 슬롯으로 취급하므로, 탐색기 메뉴 렌더러가 슬롯 침범 항목을 유효하지 않은 것으로 간주하여 통째로 필터링/드롭하거나 덮어써서 **어떤 상황에서도 새 폴더 메뉴가 화면에 전혀 나타나지 않는 대참사** 발생.
- **방지 원칙**:
  1. IContextMenu 구현 시 모든 메뉴 항목(새 폴더 만들기 포함)은 반드시 **Explorer가 할당한 `indexMenu` 위치부터 순차적으로 삽입(`InsertMenuItemCustom(hmenu, indexMenu++, id++, ...)`)**할 것.
  2. EasyZip 메뉴 그룹 내에서의 최상단 배치는 `indexMenu`의 맨 첫 번째 자리에 [새 폴더 만들기(&N)]를 삽입하고 그 바로 뒤에 구분선(`MF_SEPARATOR`)을 추가함으로써 100% 완벽하게 달성됨.
  3. `InsertMenuItemW` 호출 시 비트맵 호환 문제로 실패할 경우를 대비하여 순수 텍스트로 즉각 재시도하는 Fallback을 갖출 것.
  4. 반환값 HRESULT는 반드시 실제 사용된 ID 개수인 `MAKE_HRESULT(SEVERITY_SUCCESS, 0, (USHORT)(id - idCmdFirst))`를 반환할 것.

### 🔄 인스톨러/언인스톨러의 셸 메뉴 및 레지스트리 자동 연동 원칙 (`--register` & `--unregister`)
- **실제 실수**:
  1. 인스톨러(`InstallerWizard.cpp`)가 파일 복사 후 `--register`를 호출하지 않아, 사용자가 프로그램을 수동으로 1회 실행하기 전까지는 탐색기 마우스 우클릭 메뉴가 전혀 나타나지 않음.
  2. 언인스톨러(`Uninstaller.cpp`)가 프로그램 삭제 시 `--unregister`를 호출하지 않고 파일만 삭제하여, 레지스트리에 정적 셸 동사(`새 폴더 만들기` 등)와 COM 셸 확장이 그대로 남아 사용자가 삭제 후에도 우클릭 시 빈 껍데기 메뉴를 보게 됨.
- **방지 원칙**:
  1. **설치 시 (`InstallerWizard`)**: 파일 복사 완료 후 무창(`CREATE_NO_WINDOW`)으로 `mainExe --register`를 자동 실행하여, 설치 즉시 프로그램을 켜지 않아도 탐색기 우클릭 메뉴(`새 폴더 만들기(&N)`, 압축/해제 등)가 100% 활성화되도록 할 것. 탐색기를 재시작하지 않고 `SHChangeNotify(SHCNE_ASSOCCHANGED)`만 전송할 것.
  2. **삭제 시 (`Uninstaller`)**: 폴더 삭제 전 `mainExe --unregister`를 우선 실행하고, fail-safe 직접 레지스트리 정리 및 잠긴 DLL의 리네임/재부팅 삭제 예약으로 폴더를 정리할 것. 삭제 후 `SHChangeNotify(SHCNE_ASSOCCHANGED)`로 탐색기를 끄지 않고도 우클릭 메뉴에서 흔적도 없이 100% 즉시 사라지게 할 것.

### 📁 마우스 우클릭 [새 폴더 만들기] 메뉴 아이콘 표준화 (`shell32.dll,3`)
- **방지 원칙**: 마우스 우클릭 메뉴의 `새 폴더 만들기(&N)`에는 아카이버 전용 실행 파일 아이콘(`EasyZip.exe,0`)을 쓰지 말고, 윈도우 공식 표준 노란색 서류철 폴더 아이콘인 **`shell32.dll,3`**을 지정하여 탐색기 기본 기능처럼 자연스럽고 직관적인 UI를 제공할 것.

### 🗂️ 메인 윈도우 [압축 풀기] 시 대상 폴더 지정 대화상자(`IFileOpenDialog`) 필수 팝업 원칙
- **실제 실수**: 메인 프로그램 창에서 툴바의 [압축 풀기]를 눌렀을 때, 대상 경로를 묻지 않고 현재 압축 파일 위치에 임의로 폴더를 생성하여 곧바로 풀어버려 사용자가 원치 않는 위치에 파일이 풀리는 문제 발생.
- **방지 원칙**:
  1. 메인 윈도우에서 압축을 풀 때는 곧바로 풀지 말고, 반드시 최신 윈도우 표준 폴더 브라우저인 **`IFileOpenDialog` (`FOS_PICKFOLDERS`)**를 팝업하여 사용자가 압축을 풀 대상 폴더를 직접 확인하고 선택할 수 있도록 할 것.
  2. 기본 위치는 최근 푼 폴더(`GetLastExtractDir()`) 또는 압축 파일이 위치한 폴더를 기본값으로 지정하여 탐색 편의성을 높일 것.
  3. 사용자가 [취소]를 누른 경우 압축 해제를 진행하지 않고 안전하게 작업을 즉시 중단할 것.

### 🗜️ 윈도우 공식 압축 파일 아이콘(`zipfldr.dll,101`) 표준화 및 메인 트리뷰 루트 노드 아이콘 일치 원칙
- **실제 실수**:
  1. 메인 윈도우 좌측 폴더 트리뷰(`BuildFolderTree`)에서 최상위 루트 노드는 열려있는 압축 파일 자체(`m_archivePath`)를 나타냄에도 불구하고, 일반 폴더 아이콘(`folderIcon`, `folderOpenIcon`)을 매핑하여 사용자가 압축 파일인지 일반 디렉터리인지 시각적으로 구분하기 어렵게 만듦.
  2. 프로그램 대표 아이콘 및 압축 연결 확장자 아이콘이 윈도우 공식 지퍼 압축 폴더 아이콘과 달라 통일감이 떨어졌음.
- **방지 원칙**:
  1. **윈도우 공식 압축 파일 아이콘 추출**: Windows 기본 지퍼 압축 폴더 공식 아이콘은 `C:\Windows\System32\zipfldr.dll`의 리소스 ID `101`(`HKEY_CLASSES_ROOT\CompressedFolder\DefaultIcon`)에 정의되어 있음. 이를 256×256부터 16×16까지 8단계 32-bit RGBA 멀티 해상도로 추출하여 `EasyZip.ico` 및 설치 리소스에 패키징할 것.
  2. **트리뷰 루트 노드 아이콘 일치**: 좌측 트리뷰의 루트 노드는 열려있는 압축 파일 자체를 의미하므로, `SystemIconHelper::GetArchiveIconIndex()`를 통해 시스템 이미지 리스트(`m_hImageListSmall`)에 추가된 공식 압축 파일(지퍼) 아이콘 인덱스를 `tvis.item.iImage` 및 `iSelectedImage`로 반드시 지정할 것.
  3. 일반 하위 폴더 노드들만 `folderIcon` / `folderOpenIcon`을 적용하여 아카이브 루트와 내부 폴더 계층 구조의 명확성과 직관적인 UX를 제공할 것.

### 🚫 윈도우 기본 [압축 풀기] 메뉴 충돌 방지 및 설치/삭제 시 조건부 차단·복원 원칙 (`Shell Extensions\Blocked`)
- **실제 상황 및 문제**:
  - 서드파티 압축 프로그램을 설치하고 연결하더라도, Windows Shell은 시스템 확장자 연결(`SystemFileAssociations`)을 통해 `zipfldr.dll`의 컨텍스트 메뉴 핸들러를 계속 로드함.
  - 이로 인해 압축 파일 우클릭 시 Windows 기본 "압축 풀기..."(또는 "모두 압축 풀기...")가 EasyZip 메뉴와 중복 노출되어 사용자에게 혼란과 피로감을 줌.
- **방지 원칙**:
  1. **표준 셸 확장 차단 정책 활용 (`Shell Extensions\Blocked`)**: 시스템 파일 훼손이나 관리자 권한(HKLM) 강제 없이, Windows Shell이 공식 지원하는 사용자 단위 셸 확장 차단 키인 `HKCU\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked`를 사용할 것.
  2. **차단 대상 CLSID 2종 일괄 제어**:
     - `{b8cdcb65-b1bf-4b42-9428-1dfdb7ee92af}` : Windows 기본 ZIP '압축 풀기' (`Compressed (zipped) Folder Context Menu`)
     - `{EE07CEF5-3441-4CFB-870A-4002C724783A}` : Windows 11 신규 아카이브(7z, RAR, TAR 등) '압축 풀기' (`Compressed Archive Folder Context Menu`)
  3. **설치/등록 시 차단 (`FileAssociation::BlockWindowsDefaultZipMenu(true)`)**: `RegisterAll()` 및 `RegisterShellMenu()` 호출 시 위 2개 CLSID를 `Blocked` 키에 등록하여 Windows 기본 메뉴를 완전 차단할 것.
  4. **삭제/해제 시 100% 원상 복구 (`FileAssociation::BlockWindowsDefaultZipMenu(false)`)**: `UnregisterAll()` 및 `UnregisterShellMenu()` 호출 시 `RegDeleteValueW`로 위 CLSID들을 삭제하고, `Uninstaller.cpp`에서도 2중 Fail-Safe로 값을 제거하여 프로그램 삭제 시 윈도우 기본 압축 풀기 메뉴가 즉시 정상 복원되도록 보장할 것.
  5. 변경 후에는 `SHChangeNotify(SHCNE_ASSOCCHANGED)`를 호출하여 탐색기 프로세스를 끄지 않고도 즉각 반영할 것.

### 📦 외부 C/C++ 서드파티 라이브러리(libarchive) 배포 DLL 명칭 불일치 맹점 및 단일화 원칙
- **실제 실수**:
  - `EasyZip.exe`가 링크하는 `libarchive.lib`의 실제 내부 Import 모듈명이 `archive.dll`로 선언되어 있었음.
  - 그러나 프로젝트와 배포 패키지(`dist\EasyZip`)에는 `libarchive.dll`이라는 이름으로 패키징하여, 개발자 PC에서는 우연히 환경변수 PATH(Anaconda 등)에 있던 `archive.dll`이 로드되어 정상 동작했으나, **클린 PC에 설치했을 때 "archive.dll이 없어 코드 실행을 진행할 수 없습니다"라는 치명적 로더 오류가 발생**함.
  - 게다가 이에 대한 초기 해결책으로 `archive.dll`과 `libarchive.dll` 두 개를 중복 복사하여 배포하겠다는 황당한 제안을 하여 불필요한 용량 낭비(3.5MB 중복)를 초래할 뻔함.
- **방지 원칙**:
  1. **Import Library 실제 의존성 명칭 검증 (`dumpbin`)**: 서드파티 라이브러리를 동적 링크할 때는 파일명이 아닌 Import Library 내부의 DLL 선언명(`dumpbin /archivemembers xxx.lib` 또는 `dumpbin /dependents exe`)을 반드시 사전에 확인하여 실제 시스템 로더가 찾는 DLL 이름을 정확히 확인할 것.
  2. **단일 원본 DLL 파일명 정규화**: 불필요한 중복 복사(3.5MB 2개 배포 등)는 절대 금지하며, `third_party\libarchive\bin\archive.dll`, `EasyZip.vcxproj`의 PostBuildEvent 복사 명령, 그리고 `dist\EasyZip` 스테이징 폴더 전체에서 **오직 단 하나의 정식 파일명인 `archive.dll`로 일원화**하여 1개만 정갈하게 배포할 것.
  3. **클린 환경 격리 테스트 필수**: 개발자 PC 환경변수(PATH) 오염으로 인한 오탐을 방지하기 위해, 바이너리 검증 시 반드시 `set PATH=C:\Windows\system32`와 같이 격리된 PATH 환경에서 실행 테스트를 수행하여 누락된 DLL이 없는지 100% 확증할 것.

### 📚 분할 압축(Multi-volume Split) 아카이브 볼륨 단절 방지 및 자동 연속 해제 원칙 (`archive_read_open_filenames_w`)
- **실제 실수**:
  - `archive_read_open_filename_w` 단일 파일 API만 사용하여 아카이브를 열었기 때문에, 대용량 분할 압축(예: `Shadow of the Colossus.part1.rar` ~ `part6.rar`)을 풀 때 첫 번째 볼륨(2GB) 끝에 도달하자 다음 조각을 이어받지 못하고 EOF(0바이트)를 만나 `Too small block encountered (0 bytes)` 치명적 에러를 뿜으며 해제가 중단됨.
  - 또한 사용자가 탐색기에서 다수의 분할 파트(`part1`~`part6`)를 한꺼번에 다중 선택하여 우클릭 압축 풀기를 지시했을 때, 동일 분할 세트에 대한 중복 제거가 없어 동일 아카이브를 파트 개수만큼 중복으로 풀어버릴 위험이 있었음.
- **방지 원칙**:
  1. **다중 볼륨 자동 수집 (`PathHelper::GetVolumeFiles`)**: 현대식 RAR(`.part1.rar`..), 7z/분할 번호(`.001`..), 구형 WinRAR(`.rar`, `.r00`..), PKZIP 분할(`.z01`..`.zip`) 등 분할 압축 패턴을 자동 감지하여, 사용자가 어떤 파트를 열더라도 항상 1번 볼륨부터 N번 볼륨까지 연속된 전체 파일 목록을 올바른 순서로 수집할 것.
  2. **다중 볼륨 통합 오픈 (`ArchiveEngine::OpenArchiveRead`)**: 분할 볼륨이 2개 이상 감지되면 단일 파일 API 대신 `archive_read_open_filenames_w`에 NULL-terminated 볼륨 경로 배열을 전달하여, libarchive가 볼륨 경계를 투명하게 넘나들며 100% 완전 해제되도록 할 것.
  3. **다중 선택 중복 해제 필터링 (`FilterMultiVolumeDuplicates`)**: 탐색기에서 사용자가 분할 볼륨 여러 개를 동시에 선택하더라도 동일 아카이브 세트는 대표 볼륨(part1) 1개로 일원화 필터링하여 중복 해제 폭주를 원천 차단할 것.

### 📂 폴더 전용 선택 대화상자(`SHBrowseForFolderW`) 및 메인 창 스마트 압축 풀기 원칙
- **UI/UX 개선 배경**:
  - `IFileOpenDialog`(`FOS_PICKFOLDERS`)는 파일 목록까지 함께 노출되는 탐색기 창 형태로 표시되어 사용자가 폴더 선택 시 혼란을 느낌.
  - 메인 프로그램 창의 [압축 풀기] 버튼이 선택한 폴더에 무조건 파일들을 직접 풀어버려, 최상위에 여러 파일이 있는 압축 파일의 경우 사용자가 지정한 폴더에 파일들이 어지럽게 흩뿌려지는 문제 발생.
- **방지 및 구현 원칙**:
  1. **순수 폴더 트리 전용 브라우저(`SHBrowseForFolderW`) 표준화**: 파일이 일절 노출되지 않는 디렉터리 트리 전용 대화상자를 사용하고, `BFFM_INITIALIZED` 메시지에서 `BFFM_SETSELECTIONW`를 전송하여 최근 작업 폴더(또는 아카이브 폴더)가 자동 포커싱되도록 할 것.
  2. **메인 창 [알아서 풀기] 지능형 분기 (`IsSingleFolderArchive`)**:
     - 최상위에 단일 폴더 1개만 있는 아카이브: 대상 폴더에 바로 풀어 이중 폴더(`폴더\폴더`) 생성 방어.
     - 최상위에 복수 파일/폴더가 흩어져 있는 아카이브: 대상 폴더 아래에 압축 파일명 폴더(`targetDir\{파일명}\`)를 자동 생성하여 사용자 작업 공간을 깔끔하게 보호.
     - 완료 후 열리는 대상 폴더 경로도 실제 풀린 최종 디렉터리(`openTargetDir`)로 정확히 동기화할 것.


### 🛡️ 기업 보안 PC(DLP/EDR/GPO)에서의 COM 셸 확장 원천 차단 한계 및 100% 순수 정적 셸 메뉴(Static Verbs) 단일화 원칙
- **실제 실수**:
  - `Shell Extensions\Approved` 등록을 시도했음에도 불구하고, 실제 회사 PC(Privacy-i, DRM, AppLocker, 기업용 GPO 환경)에서는 여전히 `알아서 풀기`, `압축하기` 등 메뉴가 전혀 나타나지 않고 오직 정적 동사였던 `새 폴더 만들기`와 `열기`만 노출되는 현상이 지속됨.
  - 원인:
    1. 일반 권한(asInvoker) 설치 환경에서는 `HKLM\...\Approved`에 쓸 수 있는 권한이 없어 레지스트리 기록 자체가 무시됨.
    2. Privacy-i 등 사내 보안 프로그램과 기업 GPO 정책(`DisableUserShellExtensions = 1`)은 서명되지 않은 임의의 서드파티 DLL(`EasyZipShell.dll`)이 시스템 핵심 프로세스인 `explorer.exe`에 로드(In-process Injection)되는 것 자체를 커널 레벨에서 원천 차단함.
    3. 반면 `새 폴더 만들기`와 `열기`는 DLL을 로드하지 않는 **순수 레지스트리 정적 동사(Static Shell Verbs)**이므로 어떠한 보안 솔루션도 차단하지 못하고 100% 정상 작동함.
- **방지 원칙**:
  1. **골칫거리 COM DLL(`EasyZipShell.dll`) 전면 폐지**: 탐색기 파일 잠김(`ERROR_SHARING_VIOLATION`), 사내 보안 차단, 32/64비트 문제, 복잡한 COM 등록을 유발하는 `EasyZipShell.dll`을 완전히 제거하고, 배포 패키지에서도 제외할 것.
  2. **100% 순수 레지스트리 정적 셸 동사(Static Verbs) 단일화**:
     - `EasyZip.Archive\shell\ExtractSmart` (`알아서 풀기(&A)`) -> `"<exePath>" -xs "%1"`
     - `EasyZip.Archive\shell\ExtractHere` (`여기에 압축 풀기(&X)`) -> `"<exePath>" -x "%1"`
     - `EasyZip.Archive\shell\ExtractFolder` (`폴더에 압축 풀기(&E)`) -> `"<exePath>" -xd "%1"`
     - `*\shell\EasyZip.Compress` & `Directory\shell\EasyZip.Compress` (`EasyZip으로 압축하기(&C)...`) -> `"<exePath>" -c "%1"` (또는 `"%V"`)
  3. **설치 즉시 활성화, 삭제 즉시 완전 정리 (Fail-safe)**:
     - 설치 직후 `--register`로 정적 동사 즉시 등록 및 `SHCNE_ASSOCCHANGED` 전송 (재부팅 불필요).
     - 언인스톨러(`Uninstaller.cpp`) 및 `--unregister` 시 위 모든 정적 키 및 레거시 COM 키를 2중으로 완전히 파기(`RegDeleteTreeW`)하여 단 1개의 찌꺼기도 남기지 말 것.

### 🚫 탐색기 우클릭 메뉴 '열기' 중복 노출 방지 원칙 (ProgID `shell\open` vs COM 셸 확장)
- **실제 실수**:
  - 확장자 연결 ProgID의 `shell\open` 기본 텍스트를 `EasyZip으로 열기(&O)`로 등록한 상태에서, COM 셸 확장(`EasyZipShell.dll`)에서도 단일 파일 우클릭 시 `EasyZip으로 열기(&O)`를 무조건 메뉴에 삽입함.
  - 이로 인해 탐색기 우클릭 메뉴에 동일한 이름의 "EasyZip으로 열기"가 2개씩 중복 노출되어 UI가 지저분해지는 시각적 결함 발생.
- **방지 원칙**:
  1. **표준 기본 동사(`열기(&O)`) 일원화**: ProgID의 `shell\open` 텍스트는 윈도우 표준에 맞춰 순수 **`열기(&O)`**로 등록하여 더블클릭 기본 실행 항목으로 유지할 것.
  2. **서브메뉴 모드 조건부 노출**: `EasyZipShell.dll`에서는 **'하나의 메뉴로 묶어보기'(`s.showSubmenu`) 서브메뉴 모드일 때만** `EasyZip ▶` 폴더 안쪽에 `EasyZip으로 열기(&O)`를 포함하고, 기본 펼침 모드에서는 상단 기본 `열기`와의 중복 방지를 위해 DLL 메뉴 항목에서 완전히 배제할 것.
