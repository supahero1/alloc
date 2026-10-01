#![no_std]

use core::alloc::{GlobalAlloc, Layout};
use core::ffi::{c_int, c_void};
use core::ptr;

extern "C" {
	fn alloc_alloc_e(size: usize, zero: c_int) -> *mut c_void;
	fn alloc_free_e(ptr: *const c_void, size: usize);
	fn alloc_realloc_e(ptr: *const c_void, old_size: usize, new_size: usize, zero: c_int) -> *mut c_void;
}

const MAX_ALIGN: usize = 4096;

pub struct Shalloc;

fn natural_size(layout: Layout) -> usize {
	layout.size().max(layout.align())
}

#[cold]
fn refuse_alignment() -> *mut u8 {
	ptr::null_mut()
}

unsafe impl GlobalAlloc for Shalloc {
	#[inline]
	unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
		if layout.align() > MAX_ALIGN {
			return refuse_alignment();
		}

		alloc_alloc_e(natural_size(layout), 0) as *mut u8
	}

	#[inline]
	unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
		if layout.align() > MAX_ALIGN {
			return refuse_alignment();
		}

		alloc_alloc_e(natural_size(layout), 1) as *mut u8
	}

	#[inline]
	unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
		alloc_free_e(ptr as *const c_void, natural_size(layout));
	}

	#[inline]
	unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
		alloc_realloc_e(
			ptr as *const c_void,
			natural_size(layout),
			new_size.max(layout.align()),
			0,
		) as *mut u8
	}
}
