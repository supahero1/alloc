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

#include <alloc/attr.h>
#include <alloc/types.h>


extern attr_api alloc_handle_idx_t
alloc_test_default_idx_fn(
	alloc_t size
	);


extern attr_api const alloc_handle_t*
alloc_test_get_handle(
	alloc_t size
	);


extern attr_api alloc_t
alloc_test_find_size_for_class(
	alloc_t target_class_idx
	);


extern attr_api alloc_arena_header_t*
alloc_test_get_arena_from_ptr(
	const volatile void* ptr
	);
