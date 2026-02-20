# CHANGELOG

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).



## [Unreleased](https://github.com/jhrutgers/zth/compare/v2.0.0...HEAD)

### Added

- `Fiber::cancel()` such that a runner fiber is terminated via throwing `zth::cancelled`.



## [2.0.0] - 2026-02-16

### Added

- `zth::Locked` RAII for scoped locks of `zth::Mutex`.
- `std::chrono` conversions for `zth::Timestamp` and `zth::TimeInterval`.
- `std::promise` and `std::future` specializations to wrap `zth::Future`.
- `std::async` specialization for fibers.
- C++20 Coroutine support using `zth::coro::task<T>` and `zth::coro::generator<T>`.
- `zth::join` and `zth::joiner` for easy fiber/future joins.

### Changed

- Make REUSE compliant.
- Fixes for tooling on Ubuntu 24.04.
- Use `std::terminate()` instead of `std::abort()` on errors, to allow setting a handler.
- Redirect standard ``assert()`` failure handling to `zth::assert_handler()`.
- Fix support for newlib 4.4.0.
- `Config::MinTimeslice` and `Config::TimesliceOverrunReportThreshold` return `struct timespec` for performance reasons.
- Removed `async` keyword in favor of `zth_async`, as it conflicts with C++11 `<future>`.
- Deprecated `zth_async` in favor of `zth::fiber` and `zth::async`.
- Renamed `zth::fiber()` to `zth::factory()` and implement `zth::fiber()` to actually return a fiber.
- Streamlined fiber type member functions.
- Removed deprecated old FSM implementation.

[2.0.0]: https://github.com/jhrutgers/zth/releases/tag/v2.0.0



## [1.1.0] - 2022-10-18

### Added

- `zth::fiber()` as alternative to `zth_fiber()` and friends with `async`.
- Defining `async` can be prevented by defining `ZTH_NO_ASYNC_KEYWORD` before
  including `<zth>`.  `zth_async` is always defined, with the original behavior
  of `async`.
- Make split `zth::fsm::Fsm` into a `BasicFsm` and `Fsm`, of which the former
  does not need fiber and timestamps to run.  As a result, the `BasicFsm` can
  be executed in an interrupt routine.
- Fix support for builds without exceptions and environmental variables to
  reduce library size.
- Change license to MPLv2.

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
