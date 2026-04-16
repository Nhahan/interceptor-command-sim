# Week 1 Checklist

## Objective

Lock the architecture, protocol categories, scenario flow, and tactical viewer contract before deeper implementation.

## Current Assumption

The repository scaffold and minimal CMake/C++ skeleton now exist.
This checklist is for **locking implementation decisions on top of the committed skeleton**, not for re-scaffolding the repo from scratch.

## Day 1
- [ ] review the committed scaffold and confirm it still matches the frozen PRD
- [ ] freeze top-level module boundaries
- [ ] name key runtime/config artifacts

## Day 2
- [ ] define scenario state flow
- [ ] define entity list and command list
- [ ] define baseline judgment outputs

## Day 3
- [ ] define TCP message families
- [ ] define UDP snapshot/telemetry families
- [ ] define reconnect/timeout/loss baseline path to implement

## Day 4
- [ ] define event schema for AAR
- [ ] define sample replay timeline format
- [ ] define tactical viewer telemetry contract

## Day 5
- [ ] update README with locked structure
- [ ] update architecture/protocol/scenario docs with concrete shapes
- [ ] review scope against 4-week cutline

## End-of-Week Exit Criteria
- [ ] Gate A draft passed
- [ ] Gate B draft passed
- [ ] no uncontrolled feature creep
- [ ] Week 2 implementation can start without reopening architecture direction

## Completed Baseline Work Since This Checklist Was Written

- canonical CMake configure/build/test commands committed
- protocol message schema committed
- payload serialization committed
- runtime config loading committed
- reusable runtime separation committed
- baseline regression tests committed
