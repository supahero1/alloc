use std::alloc::{GlobalAlloc, Layout, System};
use std::hint::black_box;
use std::time::Instant;

use shalloc::Shalloc;

const PAIRS: usize = 50_000_000;
const BATCH: usize = 64;
const ROUNDS: usize = 500_000;

fn next(seed: &mut u32) -> u32 {
	*seed = seed.wrapping_mul(1103515245).wrapping_add(12345);
	*seed >> 16
}

fn bench<A: GlobalAlloc>(name: &str, a: &A) {
	let small = Layout::from_size_align(16, 8).unwrap();
	let start = Instant::now();
	for _ in 0..PAIRS {
		unsafe {
			let ptr = black_box(a.alloc(small));
			a.dealloc(ptr, small);
		}
	}
	let pair = start.elapsed().as_nanos() as f64 / PAIRS as f64 / 2.0;

	let medium = Layout::from_size_align(64, 8).unwrap();
	let mut ptrs = [std::ptr::null_mut(); BATCH];
	let start = Instant::now();
	for _ in 0..ROUNDS {
		for ptr in ptrs.iter_mut() {
			*ptr = unsafe { black_box(a.alloc(medium)) };
		}
		for &ptr in ptrs.iter() {
			unsafe { a.dealloc(ptr, medium) };
		}
	}
	let batch = start.elapsed().as_nanos() as f64 / (ROUNDS * BATCH * 2) as f64;

	let mut seed = 1u32;
	let mut layouts = [small; BATCH];
	let start = Instant::now();
	for _ in 0..ROUNDS {
		for (ptr, layout) in ptrs.iter_mut().zip(layouts.iter_mut()) {
			*layout = Layout::from_size_align(8 + next(&mut seed) as usize % 1017, 8).unwrap();
			*ptr = unsafe { black_box(a.alloc(*layout)) };
		}
		for (&ptr, &layout) in ptrs.iter().zip(layouts.iter()).rev() {
			unsafe { a.dealloc(ptr, layout) };
		}
	}
	let mixed = start.elapsed().as_nanos() as f64 / (ROUNDS * BATCH * 2) as f64;

	println!("{name:8} pair16 {pair:6.2} ns/op  batch64 {batch:6.2} ns/op  mixed {mixed:6.2} ns/op");
}

fn main() {
	for _ in 0..3 {
		bench("system", &System);
		bench("shalloc", &Shalloc);
	}
}
