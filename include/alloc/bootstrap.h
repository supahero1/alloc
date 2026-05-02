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
#include <alloc/consts.h>


extern bool
alloc_is_bootstrap_ptr(
	const volatile void* ptr
	);


extern void
alloc_bootstrap_init(
	void
	);


extern attr_noinline attr_alloc_fn void*
alloc_bootstrap_alloc(
	alloc_t size,
	int zero
	);


extern attr_noinline void
alloc_bootstrap_free(
	const volatile void* ptr,
	alloc_t size
	);


extern attr_noinline void*
alloc_bootstrap_realloc(
	const volatile void* ptr,
	alloc_t old_size,
	alloc_t new_size,
	int zero
	);
