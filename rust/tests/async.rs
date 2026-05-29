// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

use env_logger;
use log;

fn init() {
    let _ = env_logger::try_init();
}

#[test]
fn fiber_vv() {
    init();
    log::info!("Starting test: fiber_vv");
    zth::run(
        || {
            log::info!("Inside fiber_vv main closure");
            let fiber = zth::fiber_with(
                || {
                    log::info!("Inside child fiber");
                },
                (),
                zth::FiberOptions::default().with_future(),
            )
            .expect("failed to spawn child fiber");
            log::info!("Spawned child fiber");
            fiber.future().expect("missing child future").wait()
        },
        (),
    )
    .expect("zth::run failed")
    .expect("Future is empty");
    log::info!("Completed test: fiber_vv");
}

#[test]
fn fiber_iv() {
    init();
    log::info!("Starting test: fiber_iv");
    let x = zth::run(
        || {
            log::info!("Inside fiber_iv main closure");
            let fiber = zth::fiber_with(
                || {
                    log::info!("Inside child fiber");
                    42usize
                },
                (),
                zth::FiberOptions::default().with_future(),
            )
            .expect("failed to spawn child fiber");
            log::info!("Spawned child fiber");
            fiber
                .future()
                .expect("missing child future")
                .get()
                .expect("future empty")
        },
        (),
    )
    .expect("zth::run failed");
    assert_eq!(x, 42);
    log::info!("Completed test: fiber_iv");
}

#[test]
fn fiber_iii() {
    init();
    log::info!("Starting test: fiber_iii");
    let x = zth::run(
        || {
            log::info!("Inside fiber_iii main closure");
            let fiber = zth::fiber_with(
                |a, b| {
                    log::info!("Inside child fiber: a={}, b={}", a, b);
                    a + b
                },
                (3, 4),
                zth::FiberOptions::default().with_future(),
            )
            .expect("failed to spawn child fiber");
            log::info!("Spawned child fiber");
            fiber
                .future()
                .expect("missing child future")
                .get()
                .expect("future empty")
        },
        (),
    )
    .expect("zth::run failed");
    assert_eq!(x, 7);
    log::info!("Completed test: fiber_iii");
}
