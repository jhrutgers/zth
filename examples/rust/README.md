# Zth Rust Examples

This workspace contains two small examples:

- `hello`: create one fiber that prints a message.
- `futures`: one fiber produces a value, another waits for it through `zth::Future`.

## Build all examples

```bash
cargo build
```

## Run one example

```bash
cargo run -p hello
cargo run -p futures
```
