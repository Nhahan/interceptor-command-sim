# PROJECT

이 문서는 현재 워크트리 기준으로 이 저장소를 빠르게 파악하기 위한 상세 요약이다.
정식 기준 문서는 여전히 `README.md`와 `docs/`이며, 이 문서는 그 내용을 한 장의 운영 지도처럼 재구성한 참고 문서다.

## 1. 프로젝트 정체성

- 이름: `Interceptor Command Simulation System`
- 성격: C++ 기반 서버 권위형 실시간 시뮬레이션/통제 시스템
- 초점:
  - 서버 권위(authoritative) 상태 관리
  - 명령 검증과 상태 전이
  - TCP/UDP 역할 분리
  - 관측성(telemetry, event log, freshness, AAR)
  - 재현 가능성(replayable logging / AAR)
  - 제한된 범위 안에서의 운영 가능성

이 프로젝트는 게임 플레이 감각이나 시각 효과를 만드는 것이 목적이 아니다.
핵심은 "서버가 진실을 결정하고, 클라이언트는 요청하거나 렌더링하며, 사후 분석까지 가능해야 한다"는 시스템 설계 자체다.

## 2. 현재 구현 수준

현재 저장소는 설계 초안이 아니라 이미 구현과 검증이 끝난 상태에 가깝다.
핵심 특징은 다음과 같다.

- CMake 기반 구성/빌드/테스트 플로우가 정리되어 있다.
- `in_process` 백엔드로 결정론적(deterministic) 기준 실행이 가능하다.
- `socket_live` 백엔드로 TCP/UDP 실제 소켓 경로를 검증할 수 있다.
- 서버 실행 파일, fire control console, ASCII tactical display, SDL GUI viewer, artifact summary CLI가 존재한다.
- 샘플 산출물(AAR, sample output, GUI state golden, screenshots)이 저장소에 포함되어 있다.
- 회귀 테스트가 53개 존재하며 현재 워크트리 기준 모두 통과한다.

현재 확인 결과:

- `ctest --test-dir build --output-on-failure`
- 결과: `53/53` passed

## 3. 시스템 범위

이 저장소는 의도적으로 범위를 좁게 유지한다.

### 포함되는 범위

- 서버 1개
- 클라이언트 2개 역할
  - fire control console
  - read-only tactical display
- 대표 시나리오 1개
- 서버 측 명령 검증
- 서버 측 assessment 생산
- TCP/UDP 분리
- AAR / replay artifact
- 최소 1개 이상의 resilience path

### 의도적으로 제외되는 범위

- 화려한 HUD / 게임 UI / 점수 체계
- 직접 조작형 플레이(WASD, 수동 조준 등)
- 현실적 무기 물리/교전 전술 정밀 모델링
- 다중 세션/다중 시나리오 대확장
- AI 전투 시스템

## 4. 리포지토리 구조

실제 코드의 중심은 아래 경로들이다.

### 루트

- `CMakeLists.txt`
- `README.md`
- `AGENTS.md`
- `PROJECT.md`

### 공통 라이브러리

- `common/include/icss/core/`
  - 시뮬레이션 타입, 설정, 런타임 API
- `common/include/icss/protocol/`
  - 메시지 종류, payload, serialization, frame codec
- `common/include/icss/net/`
  - transport abstraction
- `common/include/icss/view/`
  - replay cursor, ASCII tactical renderer
- `common/src/`
  - 실제 구현 전부

### 실행 파일

- `server/src/main.cpp`
  - authoritative server entrypoint
- `clients/command-console/src/main.cpp`
  - fire control console CLI
- `clients/tactical-viewer/src/main.cpp`
  - ASCII tactical display
- `clients/tactical-viewer-gui/src/*.cpp`
  - SDL GUI tactical display
- `tools/src/artifact_summary.cpp`
  - artifact summary CLI

### 설정/스크립트/문서/산출물

- `configs/*.example.yaml`
- `scripts/run_live_demo.sh`
- `docs/*.md`
- `assets/sample-aar/`
- `assets/screenshots/`
- `examples/`
- `debug-frames/`

### 테스트

- `tests/protocol/`
- `tests/scenario/`
- `tests/resilience/`
- `tests/cmake/`
- `tests/support/`

## 5. 핵심 아키텍처

프로젝트는 다음 4개 층으로 이해하면 가장 빠르다.

### 5.1 SimulationSession

중심은 `common/include/icss/core/simulation.hpp`와 `common/src/simulation_*.cpp`의 `SimulationSession`이다.
이 객체가 사실상 시스템의 도메인 모델이며 다음을 담당한다.

- 세션 phase 관리
- target/interceptor 세계 상태 관리
- track 상태 관리
- command validation
- tick advancement
- assessment 결정
- event record 축적
- snapshot 기록
- AAR artifact 생성

즉, "시스템이 실제로 무엇을 참으로 간주하는가"는 대부분 `SimulationSession`이 결정한다.

### 5.2 TransportBackend

`common/include/icss/net/transport.hpp`의 `TransportBackend`는 런타임과 전송 수단을 분리한다.

구현체는 두 개다.

- `InProcessTransport`
  - 테스트와 결정론적 샘플 생성의 기준 경로
- `SocketLiveTransport`
  - POSIX socket 기반 실시간 TCP/UDP 경로

이 분리는 매우 중요하다.
도메인 로직을 소켓 세부사항과 분리해서, 로컬 검증은 안정적으로 유지하고 네트워크 경계는 별도로 증명한다.

### 5.3 Runtime / Server CLI

`common/src/runtime.cpp`와 `server/src/main.cpp`는 실제 실행 orchestration을 담당한다.

- 설정 로드
- 백엔드 선택
- baseline demo 실행
- live server loop 실행
- startup/shutdown 출력
- session.log 기록
- AAR/sample output 기록

### 5.4 Viewer Layer

뷰어는 truth를 갖지 않는다.

- ASCII viewer: `common/src/ascii_tactical_view.cpp`, `clients/tactical-viewer/src/main.cpp`
- GUI viewer: `clients/tactical-viewer-gui/src/`

특히 GUI viewer는 화려한 연출보다 다음에 집중한다.

- phase 가시성
- authoritative status 가시성
- freshness / resilience 상태
- telemetry
- timeline log
- AAR cursor / summary
- tactical geometry

## 6. 상태 모델

핵심 상태 enum은 `common/include/icss/core/types.hpp`에 있다.

### 6.1 주요 enum

- `ClientRole`
  - `FireControlConsole`
  - `TacticalDisplay`
- `ConnectionState`
  - `Disconnected`
  - `Connected`
  - `Reconnected`
  - `TimedOut`
- `SessionPhase`
  - `Standby`
  - `Detecting`
  - `Tracking`
  - `InterceptorReady`
  - `EngageOrdered`
  - `Intercepting`
  - `Assessed`
  - `Archived`
- `InterceptorStatus`
  - `Idle`
  - `Ready`
  - `Intercepting`
  - `Complete`
- `EngageOrderStatus`
  - `None`
  - `Accepted`
  - `Executing`
  - `Completed`
  - `Rejected`
- `AssessmentCode`
  - `Pending`
  - `InterceptSuccess`
  - `InvalidTransition`
  - `TimeoutObserved`

### 6.2 Snapshot

`Snapshot`은 tactical picture와 telemetry를 한 번에 묶는 authoritative 상태 기록이다.

들어 있는 정보:

- 세션 envelope / snapshot header
- 현재 phase
- world width / height
- target / interceptor entity state
- 월드 좌표와 속도
- heading
- interceptor speed / acceleration
- intercept radius / timeout
- seeker FOV / lock / off-boresight
- predicted intercept point / time-to-intercept
- track estimate / residual / covariance / measurement age / missed updates
- interceptor status
- engage order status
- assessment
- display connection state
- telemetry sample
- launch angle

즉, viewer는 snapshot만 받아도 시뮬레이션을 거의 설명할 수 있다.

### 6.3 EventRecord

`EventRecord`는 AAR와 로그의 원재료다.

- tick
- timestamp
- event_type
- source
- entity ids
- summary
- details

## 7. 시나리오 흐름

대표 시나리오는 단 하나다.
그러나 결과는 둘로 분기할 수 있다.

### 기본 흐름

1. session start
2. target detection
3. track acquire
4. interceptor ready
5. engage order
6. intercepting
7. assessment
8. archive
9. AAR

### 정상 GUI 제어 순서

`Start -> Track -> Ready -> Engage -> AAR -> Reset`

### 분기 결과

- `tracked_intercept`
  - track을 유지한 채 launch
  - 일반적으로 `intercept_success`
- `unguided_intercept`
  - launch 전 `Drop Track`
  - 일반적으로 `timeout_observed`

이 비교가 현재 저장소의 대표 샘플 비교 축이다.

## 8. Track, interceptor, assessment의 실제 동작

### 8.1 Track

track은 단순 on/off 플래그가 아니다.

포함 정보:

- estimated position
- estimated velocity
- measurement position
- measurement valid
- residual distance
- covariance trace
- measurement age
- missed updates

`common/src/simulation_state.cpp`와 `common/src/simulation.cpp`에서 deterministic noisy observation 기반으로 갱신된다.
따라서 GUI나 ASCII 출력의 tracking 정보는 가짜 percentage가 아니라 실제 추적 상태를 반영한다.

### 8.2 Launch / intercept behavior

interceptor는 항상 `(0,0)` origin에서 시작한다.
launch angle은 설정 가능하며 기본은 `45 deg`다.

`issue_command()` 시점에:

- launch angle로 초기 속도를 만든다.
- track이 살아 있으면 tracked intercept
- track이 없으면 unguided intercept

tracked 상태에서는 target predicted intercept point를 향하도록 velocity correction이 들어간다.

### 8.3 Assessment

assessment는 서버에서만 결정한다.

- 거리 <= intercept radius면 `InterceptSuccess`
- timeout tick 초과면 `TimeoutObserved`
- invalid phase/order면 `InvalidTransition`

성공 시:

- target 비활성화
- target/interceptor motion 정지
- interceptor complete

assessment 다음 tick에 runtime은 auto-archive한다.

## 9. TCP/UDP 프로토콜

핵심 프로토콜 정의는 다음 헤더가 기준이다.

- `common/include/icss/protocol/messages.hpp`
- `common/include/icss/protocol/payloads.hpp`
- `common/include/icss/protocol/serialization.hpp`
- `common/include/icss/protocol/frame_codec.hpp`

### 9.1 TCP 역할

TCP는 제어와 신뢰가 필요한 메시지 전용이다.

대표 종류:

- `session_create`
- `session_join`
- `session_leave`
- `scenario_start`
- `scenario_stop`
- `scenario_reset`
- `track_acquire`
- `track_drop`
- `interceptor_ready`
- `engage_order`
- `command_ack`
- `assessment_event`
- `aar_request`
- `aar_response`

### 9.2 UDP 역할

UDP는 freshness가 중요한 상태 전파용이다.

- `world_snapshot`
- `entity_state`
- `track_summary`
- `telemetry`
- `display_heartbeat`

### 9.3 Framing

payload serialization과 framing은 분리되어 있다.

- payload: deterministic textual `key=value;...`
- TCP frame:
  - `json` line framing
  - `binary` length-prefixed framing

즉, logical schema와 wire carriage를 나눠둔 구조다.

## 10. 실행 모드

### 10.1 in_process

용도:

- baseline demo
- deterministic sample generation
- 빠른 regression

특징:

- 네트워크 없이 transport abstraction 위에서 실행
- 샘플 AAR / sample output / session.log 생성 기준
- `tracked_intercept`, `unguided_intercept` sample mode 지원

### 10.2 socket_live

용도:

- 실제 TCP/UDP 경계 검증
- GUI/console live interaction

특징:

- TCP listener + UDP socket binding
- fire control console TCP client 1개만 허용
- tactical display UDP registration 1개만 허용
- heartbeat timeout 처리
- pending snapshot batching / latest-only 정책 지원
- `aar_request`/`aar_response` live 처리

## 11. 클라이언트들

### 11.1 Fire control console

파일:

- `clients/command-console/src/main.cpp`

역할:

- live TCP 경로로 session join
- scenario start
- track acquire
- interceptor ready
- engage order
- AAR polling

실질적으로 scripted operator 흐름을 재현한다.

### 11.2 ASCII tactical display

파일:

- `clients/tactical-viewer/src/main.cpp`
- `common/src/ascii_tactical_view.cpp`

역할:

- snapshot 기반 textual tactical frame 출력
- telemetry / freshness / recent events / AAR cursor 가시화

### 11.3 SDL GUI tactical display

핵심 파일:

- `clients/tactical-viewer-gui/src/main.cpp`
- `app_args.cpp`
- `app_control.cpp`
- `app_snapshot.cpp`
- `app_net.cpp`
- `app_render_*.cpp`
- `app_labels.cpp`
- `app_setup.cpp`

역할:

- UDP `session_join` 및 heartbeat 송신
- UDP snapshot / telemetry 수신
- 필요 시 TCP control channel attach
- `Start`, `Track`, `Ready`, `Engage`, `AAR`, `Reset` 제공
- setup panel에서 다음 run 파라미터 수정
- authoritative headline / badge / resilience panel / event timeline / AAR panel 표시

중요한 GUI 정책:

- viewer는 read-only/observability-first
- 다만 control button은 live control을 위한 thin client 역할로 제공
- `AAR`는 live control chain과 분리된 post-assessment 동작
- timeline panel은 live mode와 AAR mode를 공유
- stale 시 `session_join` 재시도로 auto-rejoin 가능

## 12. 설정

설정은 `configs/*.example.yaml`에서 로드된다.

### 12.1 server config

`configs/server.example.yaml`

- bind host
- tcp frame format
- tick rate
- TCP/UDP port
- UDP batch count
- latest-only policy
- heartbeat interval/timeout
- max clients

### 12.2 scenario config

`configs/scenario.example.yaml`

- scenario name / description
- world size
- target start / velocity
- interceptor origin / speed
- intercept radius
- timeout ticks
- seeker FOV
- launch angle

### 12.3 logging config

`configs/logging.example.yaml`

- level
- outputs
- file path
- AAR enabled
- AAR output dir

### 12.4 precedence

CLI override가 config file보다 우선한다.

## 13. 산출물

### 13.1 Runtime log

- `logs/session.log`

JSON line 기반이며 다음을 포함한다.

- session summary record
- event records
- backend
- assessment code
- effective track state
- intercept profile
- launch angle
- resilience summary

### 13.2 AAR artifacts

기본 tracked sample:

- `assets/sample-aar/session-summary.md`
- `assets/sample-aar/session-summary.json`
- `assets/sample-aar/replay-timeline.json`

comparison unguided sample:

- `assets/sample-aar/unguided_intercept/session-summary.md`
- `assets/sample-aar/unguided_intercept/session-summary.json`
- `assets/sample-aar/unguided_intercept/replay-timeline.json`

### 13.3 Example outputs

- `examples/sample-output.md`
- `examples/sample-output-unguided_intercept.md`

### 13.4 GUI review assets

- `assets/screenshots/tactical-display-tracked_intercept-state.json`
- `assets/screenshots/tactical-display-unguided_intercept-state.json`
- `assets/screenshots/tactical-display-tracked_intercept.bmp`
- `assets/screenshots/tactical-display-unguided_intercept.bmp`

### 13.5 Debug frames

- `debug-frames/*.json`
- `debug-frames/*.bmp`
- `debug-frames/*.png`

GUI layout/telemetry 시각 상태를 개발 중 확인하기 위한 프레임이다.

## 14. 테스트 체계

테스트는 단순 단위 테스트가 아니라 "시스템 계약"을 고정하는 방향으로 설계돼 있다.

### 14.1 protocol

- payload serialization round-trip
- frame codec round-trip
- transport backend dispatch
- live process smoke
- single session policy
- fire control console live flow
- GUI live flow
- snapshot ordering / malformed UDP / duplicate start / AAR / parameter timeout 등

### 14.2 scenario

- scenario flow
- invalid ordering rejection
- artifact generation
- artifact determinism
- artifact summary CLI
- replay cursor
- config validation

### 14.3 resilience

- reconnect / resync
- timeout visibility
- UDP snapshot batching
- telemetry alignment
- degraded freshness

### 14.4 drift protection

샘플 산출물과 goldens가 체크인된 canonical 상태와 계속 일치하는지까지 테스트한다.

## 15. 실제로 기억해야 할 핵심 파일

질문이 들어왔을 때 가장 먼저 열어볼 파일 기준은 아래다.

### 아키텍처/상태 질문

- `common/include/icss/core/types.hpp`
- `common/include/icss/core/simulation.hpp`
- `common/src/simulation.cpp`
- `common/src/simulation_commands.cpp`
- `common/src/simulation_snapshot.cpp`
- `common/src/simulation_summary.cpp`

### 프로토콜/네트워크 질문

- `common/include/icss/protocol/messages.hpp`
- `common/include/icss/protocol/payloads.hpp`
- `common/include/icss/protocol/serialization.hpp`
- `common/src/serialization.cpp`
- `common/src/frame_codec.cpp`
- `common/src/transport_socket_live.cpp`

### 런타임/CLI 질문

- `common/include/icss/core/runtime.hpp`
- `common/src/runtime.cpp`
- `common/src/config.cpp`
- `server/src/main.cpp`

### GUI 질문

- `clients/tactical-viewer-gui/src/app.hpp`
- `clients/tactical-viewer-gui/src/app_control.cpp`
- `clients/tactical-viewer-gui/src/app_snapshot.cpp`
- `clients/tactical-viewer-gui/src/app_net.cpp`
- `clients/tactical-viewer-gui/src/app_render_*.cpp`
- `clients/tactical-viewer-gui/src/app_labels.cpp`

### 산출물/운영 질문

- `tools/src/artifact_summary.cpp`
- `scripts/run_live_demo.sh`
- `assets/sample-aar/*`
- `examples/*`
- `logs/session.log`

### 검증 질문

- `docs/test-report.md`
- `tests/protocol/*`
- `tests/scenario/*`
- `tests/resilience/*`

## 16. 자주 받을 질문과 답의 방향

### 왜 서버 권위형인가?

명령 검증, assessment, replay/AAR의 기준을 한 곳에 고정하기 위해서다.
클라이언트는 truth를 만들지 않는다.

### 왜 TCP와 UDP를 나누나?

제어/ack/AAR은 reliability와 ordering이 중요하고, snapshot/telemetry는 freshness가 더 중요하기 때문이다.

### 왜 in_process와 socket_live를 따로 두나?

결정론적 검증과 실제 네트워크 경계를 동시에 확보하기 위해서다.

### AAR은 어디서 오나?

클라이언트 재구성이 아니라 서버 event history와 snapshots에서 직접 생성된다.

### 뷰어는 왜 read-only 성격을 유지하나?

이 프로젝트의 viewer 목적은 "조작"이 아니라 "상태 전파와 운영 상태를 읽기 쉽게 만드는 것"이기 때문이다.

## 17. 현재 워크트리에서 보이는 변경의 큰 방향

현재 아직 커밋되지 않은 변경들은 대체로 다음 축으로 읽힌다.

- 용어 정렬
  - command console -> fire control console
  - tactical viewer -> tactical display
  - judgment -> assessment
  - guidance -> track
  - asset -> interceptor
  - command lifecycle -> engage order status
- tracked/unguided 비교 모델 강화
  - `tracked_intercept`
  - `unguided_intercept`
- launch angle / track drop / intercept profile 노출 강화
- GUI의 AAR 분리 및 timeline 개선
- 샘플 산출물/스크린샷/golden bundle 구조 개편
- 관련 문서와 회귀 테스트 동기화

## 18. 한 줄 요약

이 저장소는 "작은 범위 안에서 서버 권위형 실시간 통제 시스템을 끝까지 설명 가능하게 만드는 것"에 집중한 C++ 시스템 프로젝트다.
핵심은 서버가 상태와 판단을 소유하고, TCP/UDP를 목적에 맞게 분리하며, 실행 후에는 AAR와 로그로 왜 그런 결과가 나왔는지 다시 설명할 수 있게 만드는 데 있다.
