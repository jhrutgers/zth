// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#[test]
fn fiber_vv() {
    zth::run(
        || {
            let fiber = zth::fiber_with(|| (), (), zth::FiberOptions::default().with_future())
                .expect("failed to spawn child fiber");

            fiber.future().expect("missing child future").wait()
        },
        (),
    )
    .expect("zth::run failed")
    .expect("Future is empty");
}

#[test]
fn fiber_iv() {
    let x = zth::run(
        || {
            let fiber = zth::fiber_with(|| 42usize, (), zth::FiberOptions::default().with_future())
                .expect("failed to spawn child fiber");

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
}

#[test]
fn fiber_iii() {
    let x = zth::run(
        || {
            let fiber = zth::fiber_with(
                |a, b| a + b,
                (3, 4),
                zth::FiberOptions::default().with_future(),
            )
            .expect("failed to spawn child fiber");

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
}
