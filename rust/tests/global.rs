use std::alloc::{alloc, alloc_zeroed, dealloc, realloc, GlobalAlloc, Layout};
use std::collections::HashMap;
use std::sync::mpsc;
use std::thread;

use shalloc::Shalloc;

#[global_allocator]
static GLOBAL: Shalloc = Shalloc;

const SIZES: [usize; 14] = [1, 7, 8, 24, 100, 256, 1000, 4096, 5000, 65536, 131073, 300000, 1 << 20, (1 << 21) + 5];

#[test]
fn alignment() {
	for shift in 0..=12 {
		let align = 1usize << shift;
		for &size in &SIZES {
			let layout = Layout::from_size_align(size, align).unwrap();
			unsafe {
				let ptr = alloc(layout);
				assert!(!ptr.is_null());
				assert_eq!(ptr as usize % align, 0, "size {size} align {align}");
				ptr.write_bytes(0xAB, size);
				dealloc(ptr, layout);
			}
		}
	}
}

#[test]
fn over_page_alignment_refused() {
	for shift in 13..=22 {
		let layout = Layout::from_size_align(64, 1usize << shift).unwrap();
		unsafe {
			assert!(GLOBAL.alloc(layout).is_null());
			assert!(GLOBAL.alloc_zeroed(layout).is_null());
		}
	}
}

#[test]
fn zeroed() {
	for &align in &[8usize, 64, 4096] {
		for &size in &SIZES {
			let layout = Layout::from_size_align(size, align).unwrap();
			unsafe {
				let dirty = alloc(layout);
				dirty.write_bytes(0xCD, size);
				dealloc(dirty, layout);

				let ptr = alloc_zeroed(layout);
				assert!(std::slice::from_raw_parts(ptr, size).iter().all(|&b| b == 0), "size {size} align {align}");
				dealloc(ptr, layout);
			}
		}
	}
}

#[test]
fn realloc_keeps_data() {
	for &align in &[1usize, 16, 64, 4096] {
		let mut layout = Layout::from_size_align(1, align).unwrap();
		unsafe {
			let mut ptr = alloc(layout);
			*ptr = 0;

			for &size in SIZES.iter().chain(SIZES.iter().rev()) {
				let keep = layout.size().min(size);
				ptr = realloc(ptr, layout, size);
				assert!(!ptr.is_null());
				assert_eq!(ptr as usize % align, 0);

				for i in 0..keep {
					assert_eq!(*ptr.add(i), i as u8, "size {size} align {align} byte {i}");
				}
				for i in keep..size {
					*ptr.add(i) = i as u8;
				}

				layout = Layout::from_size_align(size, align).unwrap();
			}

			dealloc(ptr, layout);
		}
	}
}

#[test]
fn collections_across_threads() {
	let (tx, rx) = mpsc::channel::<Vec<String>>();

	let producers: Vec<_> = (0..4)
		.map(|t| {
			let tx = tx.clone();
			thread::spawn(move || {
				for round in 0..200 {
					let batch: Vec<String> = (0..64).map(|i| format!("{t}-{round}-{i}").repeat(i % 13 + 1)).collect();
					tx.send(batch).unwrap();
				}
			})
		})
		.collect();
	drop(tx);

	let mut seen: HashMap<usize, usize> = HashMap::new();
	for batch in rx {
		for s in batch {
			*seen.entry(s.len()).or_default() += 1;
		}
	}

	for producer in producers {
		producer.join().unwrap();
	}

	assert_eq!(seen.values().sum::<usize>(), 4 * 200 * 64);
}
