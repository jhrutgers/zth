// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(not(feature = "std"), no_main)]

#[cfg(not(feature = "std"))]
use core::result::Result;

extern crate alloc;

use alloc::rc::Rc;

fn producer_fiber() -> usize {
    zth::may_yield();
    zth::print!("Producer: computing value...\n");
    // Force a yield here.
    zth::out_of_work();

    42usize
}

fn consumer_fiber(producer_value: Rc<zth::Future<usize>>) {
    zth::print!("Consumer: waiting for producer...\n");
    producer_value
        .wait()
        .expect("failed waiting for producer value");
    let value = producer_value.get().expect("failed reading producer value");
    zth::print!("Consumer: got value {}\n", value);
}

#[zth::main_fiber]
fn app_main() -> Result<(), zth::Error> {
    let producer = zth::fiber_with(
        producer_fiber,
        (),
        zth::FiberOptions::default().with_future(),
    )?;

    let producer_value = producer.future().expect("missing producer future");

    let consumer = zth::fiber_with(
        consumer_fiber,
        (producer_value.clone(),),
        zth::FiberOptions::default().with_future(),
    )?;

    let consumer_done = consumer.future().expect("missing consumer future");
    consumer_done
        .wait()
        .expect("failed waiting for consumer fiber");

    Ok(())
}
