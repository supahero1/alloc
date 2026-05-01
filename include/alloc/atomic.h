/*
 *   Copyright 2025-2026 Franciszek Balcerak
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include <stdatomic.h>

#define atomic_load_rx(value)	\
atomic_load_explicit(value, memory_order_relaxed)

#define atomic_load_acq(value)	\
atomic_load_explicit(value, memory_order_acquire)

#define atomic_load_seq_cst(value)	\
atomic_load_explicit(value, memory_order_seq_cst)

#define atomic_store_rx(value, new_value)	\
atomic_store_explicit(value, new_value, memory_order_relaxed)

#define atomic_store_rel(value, new_value)	\
atomic_store_explicit(value, new_value, memory_order_release)

#define atomic_store_seq_cst(value, new_value)	\
atomic_store_explicit(value, new_value, memory_order_seq_cst)

#define atomic_exchange_rx(value, new_value)	\
atomic_exchange_explicit(value, new_value, memory_order_relaxed)

#define atomic_exchange_acq(value, new_value)	\
atomic_exchange_explicit(value, new_value, memory_order_acquire)

#define atomic_exchange_rel(value, new_value)	\
atomic_exchange_explicit(value, new_value, memory_order_release)

#define atomic_exchange_acq_rel(value, new_value)	\
atomic_exchange_explicit(value, new_value, memory_order_acq_rel)

#define atomic_exchange_seq_cst(value, new_value)	\
atomic_exchange_explicit(value, new_value, memory_order_seq_cst)

#define atomic_exchange_strong_rx(value, expected, new_value)	\
atomic_compare_exchange_strong_explicit(						\
	value, expected, new_value, memory_order_relaxed, memory_order_relaxed)

#define atomic_exchange_strong_acq(value, expected, new_value)	\
atomic_compare_exchange_strong_explicit(						\
	value, expected, new_value, memory_order_acquire, memory_order_acquire)

#define atomic_exchange_strong_rel(value, expected, new_value)	\
atomic_compare_exchange_strong_explicit(						\
	value, expected, new_value, memory_order_release, memory_order_relaxed)

#define atomic_exchange_strong_acq_rel(value, expected, new_value)	\
atomic_compare_exchange_strong_explicit(							\
	value, expected, new_value, memory_order_acq_rel, memory_order_acquire)

#define atomic_exchange_strong_seq_cst(value, expected, new_value)	\
atomic_compare_exchange_strong_explicit(							\
	value, expected, new_value, memory_order_seq_cst, memory_order_seq_cst)

#define atomic_exchange_weak_rx(value, expected, new_value)	\
atomic_compare_exchange_weak_explicit(						\
	value, expected, new_value, memory_order_relaxed, memory_order_relaxed)

#define atomic_exchange_weak_acq(value, expected, new_value)	\
atomic_compare_exchange_weak_explicit(							\
	value, expected, new_value, memory_order_acquire, memory_order_acquire)

#define atomic_exchange_weak_rel(value, expected, new_value)	\
atomic_compare_exchange_weak_explicit(							\
	value, expected, new_value, memory_order_release, memory_order_relaxed)

#define atomic_exchange_weak_acq_rel(value, expected, new_value)	\
atomic_compare_exchange_weak_explicit(								\
	value, expected, new_value, memory_order_acq_rel, memory_order_acquire)

#define atomic_exchange_weak_seq_cst(value, expected, new_value)	\
atomic_compare_exchange_weak_explicit(								\
	value, expected, new_value, memory_order_seq_cst, memory_order_seq_cst)

#define atomic_fetch_add_rx(value, add_value)	\
atomic_fetch_add_explicit(value, add_value, memory_order_relaxed)

#define atomic_fetch_add_acq(value, add_value)	\
atomic_fetch_add_explicit(value, add_value, memory_order_acquire)

#define atomic_fetch_add_rel(value, add_value)	\
atomic_fetch_add_explicit(value, add_value, memory_order_release)

#define atomic_fetch_add_acq_rel(value, add_value)	\
atomic_fetch_add_explicit(value, add_value, memory_order_acq_rel)

#define atomic_fetch_add_seq_cst(value, add_value)	\
atomic_fetch_add_explicit(value, add_value, memory_order_seq_cst)

#define atomic_fetch_sub_rx(value, sub_value)	\
atomic_fetch_sub_explicit(value, sub_value, memory_order_relaxed)

#define atomic_fetch_sub_acq(value, sub_value)	\
atomic_fetch_sub_explicit(value, sub_value, memory_order_acquire)

#define atomic_fetch_sub_rel(value, sub_value)	\
atomic_fetch_sub_explicit(value, sub_value, memory_order_release)

#define atomic_fetch_sub_acq_rel(value, sub_value)	\
atomic_fetch_sub_explicit(value, sub_value, memory_order_acq_rel)

#define atomic_fetch_sub_seq_cst(value, sub_value)	\
atomic_fetch_sub_explicit(value, sub_value, memory_order_seq_cst)

#define atomic_fetch_or_rx(value, or_value)	\
atomic_fetch_or_explicit(value, or_value, memory_order_relaxed)

#define atomic_fetch_or_acq(value, or_value)	\
atomic_fetch_or_explicit(value, or_value, memory_order_acquire)

#define atomic_fetch_or_rel(value, or_value)	\
atomic_fetch_or_explicit(value, or_value, memory_order_release)

#define atomic_fetch_or_acq_rel(value, or_value)	\
atomic_fetch_or_explicit(value, or_value, memory_order_acq_rel)

#define atomic_fetch_or_seq_cst(value, or_value)	\
atomic_fetch_or_explicit(value, or_value, memory_order_seq_cst)

#define atomic_fetch_and_rx(value, and_value)	\
atomic_fetch_and_explicit(value, and_value, memory_order_relaxed)

#define atomic_fetch_and_acq(value, and_value)	\
atomic_fetch_and_explicit(value, and_value, memory_order_acquire)

#define atomic_fetch_and_rel(value, and_value)	\
atomic_fetch_and_explicit(value, and_value, memory_order_release)

#define atomic_fetch_and_acq_rel(value, and_value)	\
atomic_fetch_and_explicit(value, and_value, memory_order_acq_rel)

#define atomic_fetch_and_seq_cst(value, and_value)	\
atomic_fetch_and_explicit(value, and_value, memory_order_seq_cst)

#define atomic_fetch_xor_rx(value, xor_value)	\
atomic_fetch_xor_explicit(value, xor_value, memory_order_relaxed)

#define atomic_fetch_xor_acq(value, xor_value)	\
atomic_fetch_xor_explicit(value, xor_value, memory_order_acquire)

#define atomic_fetch_xor_rel(value, xor_value)	\
atomic_fetch_xor_explicit(value, xor_value, memory_order_release)

#define atomic_fetch_xor_acq_rel(value, xor_value)	\
atomic_fetch_xor_explicit(value, xor_value, memory_order_acq_rel)

#define atomic_fetch_xor_seq_cst(value, xor_value)	\
atomic_fetch_xor_explicit(value, xor_value, memory_order_seq_cst)
