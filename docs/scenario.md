# Scenario Design

## Scenario

Use one representative training/control scenario.

### Narrative Shape
1. session starts
2. target appears / is detected
3. track is acquired for pre-launch target-following
4. interceptor becomes available / activated
5. command is issued from the fire control console
6. server validates and judges the outcome
7. result is reviewed through AAR/replay

## Required Properties

- shows command -> validation -> state transition -> assessment
- shows multi-client visibility
- can generate meaningful AAR output
- can survive at least one abnormal network case in validation coverage
- uses a larger 2304x1536 world-space rather than a toy-sized board
- keeps target/interceptor motion live over time so waiting changes the geometry
- exposes velocity, heading, predicted intercept point, TTI, and seeker/FOV state
- keeps the interceptor launch origin fixed at `(0,0)` with a default `45 deg` launch angle
- treats the world as a bottom-left-origin 2D coordinate space; the viewer camera maps that world into SDL screen space
- can branch between `intercept_success` and `timeout_observed` based on timing and kinematics
- deactivates the target on `intercept_success` so the tactical picture shows the intercept as completed rather than leaving the target moving
- when launched from the GUI, each `Start` applies a small bounded random jitter to actual target start geometry/target velocity around the planned setup values while keeping the interceptor at its planned origin point

## State Flow

`Standby -> Detecting -> Tracking -> InterceptorReady -> EngageOrdered -> Intercepting -> Assessed -> Archived`

The runtime auto-archives on the tick after assessment, so the normal GUI flow is `Start -> Track -> Ready -> Engage -> AAR -> Reset`.

The runtime rejects out-of-order control steps:
- `scenario_start` is only valid from `Standby`
- `interceptor_ready` is only valid from `Tracking`
- `engage_order` is only valid from `InterceptorReady`
- `Acquire Track` / `Drop Track` is only useful before `Engage`; once the interceptor is launched, track control is locked
- post-archive control requests remain rejected while AAR stays available

## Entity Types

- `Target`
- `Interceptor`
- `Track / firing solution`
- `Session`
- `Command`
- `Assessment`

## Viewer Requirements

The tactical display should make the scenario legible as an observability-first display. It should show:
- where targets/assets are
- whether track is established
- tracker quality/residual state
- what interceptor status and engage order status state are active
- whether connection/snapshot status is healthy
- where the AAR playback cursor is in replay mode

## Non-Goals

- realistic combat doctrine
- detailed missile guidance or ballistics
- multiple mission packs
- complex enemy behavior trees
