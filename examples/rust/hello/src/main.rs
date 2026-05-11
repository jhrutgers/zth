// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

fn hello_fiber() {
    println!("Hello from a fiber!");
}

fn main_fiber() {
    zth::fiber(hello_fiber, ()).expect("failed to create hello fiber");
}

fn main() -> Result<(), zth::Error> {
    zth::run(main_fiber, ())
}
