# zth

Rust bindings for Zth.

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
