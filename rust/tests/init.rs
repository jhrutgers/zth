// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: MPL-2.0

#[test]
fn run_vv() {
    zth::run(|| {}, ()).expect("zth::run failed");
}

#[test]
fn run_vi() {
    zth::run(|_: i32| {}, (3,)).expect("zth::run failed");
}

#[test]
fn run_ii() {
    assert_eq!(zth::run(|x: i32| { x }, (4,)).expect("zth::run failed"), 4);
}

#[test]
fn run_iii() {
    assert_eq!(
        zth::run(|a: i32, b: i32| { a + b }, (5, 6)).expect("zth::run failed"),
        11
    );
}
