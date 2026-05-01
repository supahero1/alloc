/*
 *   Copyright 2026 Franciszek Balcerak
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

#define attr_attr(...) __attribute__((__VA_ARGS__))
#define attr_fallthrough() attr_attr(fallthrough)
#define attr_ctor attr_attr(constructor)
#define attr_dtor attr_attr(destructor)
#define attr_used attr_attr(used)
#define attr_unused attr_attr(unused)
#define attr_packed attr_attr(packed)
#define attr_aligned(x) attr_attr(aligned(x))
#define attr_noreturn attr_attr(noreturn)
#define attr_section(name) attr_attr(section(name))
#define attr_format(...) attr_attr(format(__VA_ARGS__))
#define attr_inline attr_attr(always_inline) inline
#define attr_noinline attr_attr(noinline)
#define attr_warn_unused_result attr_attr(warn_unused_result)
#define attr_alloc_fn attr_attr(malloc) attr_warn_unused_result
#define attr_const_fn attr_attr(const)
#define attr_pure_fn attr_attr(pure)
#define attr_hot_fn attr_attr(hot)
#define attr_cold_fn attr_attr(cold)
#define attr_api attr_attr(visibility("default"))
#define attr_test_fn attr_api attr_used

#define attr_likely(x) __builtin_expect(!!(x), 1)
#define attr_unlikely(x) __builtin_expect(!!(x), 0)
