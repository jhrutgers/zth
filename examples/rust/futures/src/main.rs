// SPDX-FileCopyrightText: 2019-2026 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

use std::rc::Rc;

fn producer_fiber() -> usize {
    zth::may_yield();
    println!("Producer: computing value...");
    // Force a yield here.
    zth::out_of_work();

    42usize
}

fn consumer_fiber(producer_value: Rc<zth::Future<usize>>) {
    println!("Consumer: waiting for producer...");
    producer_value
        .wait()
        .expect("failed waiting for producer value");
    let value = producer_value.get().expect("failed reading producer value");
    println!("Consumer: got value {}", value);
}

fn main_fiber() {
    let producer = zth::fiber_with(
        producer_fiber,
        (),
        zth::FiberOptions::default().with_future(),
    )
    .expect("failed to create producer fiber");

    let producer_value = producer.future().expect("missing producer future");

    let consumer = zth::fiber_with(
        consumer_fiber,
        (producer_value.clone(),),
        zth::FiberOptions::default().with_future(),
    )
    .expect("failed to create consumer fiber");

    let consumer_done = consumer.future().expect("missing consumer future");
    consumer_done
        .wait()
        .expect("failed waiting for consumer fiber");
}

fn main() -> Result<(), zth::Error> {
    zth::run(main_fiber, ())
}
