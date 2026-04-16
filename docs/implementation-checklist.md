# Implementation Checklist

## Objective

Lock the architecture, protocol categories, scenario flow, and viewer contract before deeper implementation.

## Current State

The repository scaffold and minimal CMake/C++ skeleton already exist.
Use this checklist to lock implementation decisions on top of the current codebase.

## Pass 1
- [ ] review the current scaffold and confirm it matches the intended runtime shape
- [ ] freeze top-level module boundaries
- [ ] name key runtime/config artifacts

## Pass 2
- [ ] define scenario state flow
- [ ] define entity list and command list
- [ ] define judgment outputs

## Pass 3
- [ ] define TCP message families
- [ ] define UDP snapshot/telemetry families
- [ ] define reconnect/timeout/loss path to implement

## Pass 4
- [ ] define event schema for AAR
- [ ] define sample replay timeline format
- [ ] define tactical viewer telemetry contract

## Pass 5
- [ ] update README with locked structure
- [ ] update architecture/protocol/scenario docs with concrete shapes
- [ ] review scope against current implementation boundaries

## Exit Criteria
- [ ] core architecture decisions are stable
- [ ] protocol categories are stable
- [ ] no uncontrolled feature creep
- [ ] implementation can continue without reopening architecture direction

## Already in Place

- canonical CMake configure/build/test commands available
- protocol message schema available
- payload serialization available
- runtime config loading available
- reusable runtime separation available
- regression tests available
