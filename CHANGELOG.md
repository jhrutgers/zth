# CHANGELOG

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased](https://github.com/jhrutgers/zth/compare/v1.0.0...HEAD)

## [1.1.0] - xxxx-xx-xx

### Added

- `zth::fiber()` as alternative to `zth_fiber()` and friends with `async`.

[1.1.0]: https://github.com/jhrutgers/zth/releases/tag/v1.1.0

## [1.0.0] - 2021-12-21

Initial version.

### Added

- Fibers/worker/scheduling and all other required basics.
- `setjmp()`/`longjmp()`, `sigaltstack()`, `ucontext()`, WinFiber context
  switching approaches.
- FSM (C++14) implementation.
- Extensible Poller framework.
- Ubuntu/macOS/Windows/ARM (QEMU) example builds.

[1.0.0]: https://github.com/jhrutgers/zth/releases/tag/v1.0.0
