# zth

Rust bindings for [Zth](https://github.com/jhrutgers/zth).

## Native setup

This crate links with native `libzth` via `build.rs`.

If you are building the crate from the Zth repository, you don't have to tell cargo where to find Zth.
Otherwise, either set:

- `ZTH_INSTALL`: install prefix containing headers and libraries (such as `/usr/local`)
- `ZTH_REPO`: path to the Zth repository root (uses `dist/<target>/build` for the compiled library)

In case `ZTH_REPO` is set, it uses the detected `dist/` subdirectory.
If the autodetect fails, override it with `ZTH_DIST` (for example `ubuntu`, `macos`, `mingw`, `win32`).

Example:

```bash
export ZTH_INSTALL=/path/to/zth/install/prefix
cargo test
```

## Features

By default, Zth enables the `std` feature. As a result:

- Zth depends on `std`. Otherwise, it only uses `core` and `alloc`.
- `zth_terminate()` calls Rust `panic!` instead of calling C++ `std::terminate()`, which defaults to
  C `abort()`.
- C `zth_logv()` is redirected to Rust `print!` instead of C library's `puts()`/`write()` to
  `stdout`.

However, for `no_std` builds, like embedded systems, depend on Zth using `default-features = false`,
and enable one more of these:

- `panic-handler`: Implement a default `panic!` handler that calls C `zth_terminate()`.
- `allocator`: Implement the `global_allocator` using C `malloc()` and `free()`.
- `bare-metal`: Combines all features above.
