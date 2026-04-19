# Implementation Checklist

## Objective

Lock the architecture, protocol categories, scenario flow, and viewer contract before deeper implementation.

## Current State

The architecture, protocol split, representative scenario flow, viewer contract, and AAR surface are already implemented.
Use this checklist as a maintenance audit so follow-on changes do not silently drift from the locked system story.

## Pass 1
- [x] review the current scaffold and confirm it matches the intended runtime shape
- [x] freeze top-level module boundaries
- [x] name key runtime/config artifacts

## Pass 2
- [x] define scenario state flow
- [x] define entity list and command list
- [x] define judgment outputs

## Pass 3
- [x] define TCP message families
- [x] define UDP snapshot/telemetry families
- [x] define reconnect/timeout/loss path to implement

## Pass 4
- [x] define event schema for AAR
- [x] define sample replay timeline format
- [x] define tactical viewer telemetry contract

## Pass 5
- [x] update README with locked structure
- [x] update architecture/protocol/scenario docs with concrete shapes
- [x] review scope against current implementation boundaries

## Exit Criteria
- [x] core architecture decisions are stable
- [x] protocol categories are stable
- [x] no uncontrolled feature creep
- [x] implementation can continue without reopening architecture direction

## Already in Place

- canonical CMake configure/build/test commands available
- protocol message schema available
- payload serialization available
- runtime config loading available
- reusable runtime separation available
- deterministic regression tests available
- authoritative in-process and `socket_live` paths available
- replay/AAR/sample output artifacts available
