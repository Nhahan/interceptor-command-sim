# Scenario Design

## Scenario

Use one representative training/control scenario.

### Narrative Shape
1. session starts
2. target appears / is detected
3. guidance is enabled for pre-launch target-following
4. interceptor becomes available / activated
5. command is issued from the command console
6. server validates and judges the outcome
7. result is reviewed through AAR/replay

## Required Properties

- shows command -> validation -> state transition -> judgment
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

`Initialized -> Detecting -> Tracking -> AssetReady -> CommandIssued -> Engaging -> Judged -> Archived`

The runtime auto-archives on the tick after judgment, so the normal GUI flow is `Start -> Guidance -> Activate -> Command -> Review -> Reset`.

The runtime rejects out-of-order control steps:
- `scenario_start` is only valid from `Initialized`
- `asset_activate` is only valid from `Tracking`
- `command_issue` is only valid from `AssetReady`
- `Guidance On` / `Guidance Off` is only useful before `Command`; once the interceptor is launched, guidance control is locked
- post-archive control requests remain rejected while AAR stays available

## Entity Types

- `Target`
- `Interceptor asset`
- `Guidance / track solution`
- `Session`
- `Command`
- `Judgment`

## Viewer Requirements

The tactical viewer should make the scenario legible as an observability-first display. It should show:
- where targets/assets are
- whether guidance is enabled
- tracker quality/residual state
- what interceptor status and command lifecycle state are active
- whether connection/snapshot status is healthy
- where the AAR playback cursor is in replay mode

## Non-Goals

- realistic combat doctrine
- detailed missile guidance or ballistics
- multiple mission packs
- complex enemy behavior trees
