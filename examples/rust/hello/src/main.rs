// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(not(feature = "std"), no_main)]

#[cfg(not(feature = "std"))]
use core::result::Result;

fn hello_fiber() {
    zth::log!("Hello from a fiber!\n");
}

#[zth::main_fiber]
fn app_main() -> Result<(), zth::Error> {
    // Create another fiber.
    zth::fiber(hello_fiber, ())?;
    Result::Ok(())
}

// If you skip #[zth::main_fiber], you can also implement main yourself:
//
// fn main() -> Result<(), zth::Error> {
//     zth::run(app_main, ())?
// }
//
// zth::main_fiber only makes it easier and compatible with bare-metal systems without std's main.
