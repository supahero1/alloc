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

#include <alloc/types.h>

#include <stdint.h>


extern void
alloc_btree_init(
	uint64_t* avail
	);


extern alloc_t
alloc_btree_size(
	void
	);


extern alloc_t
alloc_btree_get(
	uint64_t* avail,
	alloc_t slab_idx,
	uint32_t* avail_mask,
	uint32_t* changed_mask
	);


extern void
alloc_btree_ret(
	uint64_t* avail,
	alloc_t slab_idx,
	alloc_t block_idx,
	uint32_t* avail_mask,
	uint32_t* changed_mask
	);
