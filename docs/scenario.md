# Scenario Design

## Scenario

Use one representative training/control scenario.

### Narrative Shape
1. session starts
2. target appears / is detected
3. tracking request is issued
4. asset becomes available / activated
5. command is issued from the command console
6. server validates and judges the outcome
7. result is reviewed through AAR/replay

## Required Properties

- shows command -> validation -> state transition -> judgment
- shows multi-client visibility
- can generate meaningful AAR output
- can survive at least one abnormal network case in validation coverage

## State Flow

`Initialized -> Detecting -> Tracking -> AssetReady -> CommandIssued -> Engaging -> Judged -> Archived`

## Entity Types

- `Target`
- `Asset`
- `Track`
- `Session`
- `Command`
- `Judgment`

## Viewer Requirements

The tactical viewer should make the scenario legible as an observability-first display. It should show:
- where targets/assets are
- whether tracking exists
- how confident tracking currently is
- what asset status and command lifecycle state are active
- whether connection/snapshot status is healthy
- where the AAR playback cursor is in replay mode

## Non-Goals

- realistic combat doctrine
- detailed missile guidance or ballistics
- multiple mission packs
- complex enemy behavior trees
