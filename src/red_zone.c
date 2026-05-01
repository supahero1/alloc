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

#include <alloc/log.h>
#include <alloc/macro.h>
#include <alloc/types.h>
#include <alloc/consts.h>
#include <alloc/red_zone.h>

#include <stdint.h>

#ifndef ALLOC_RELEASE


	uint8_t
	alloc_red_zone_get_byte(
		alloc_t i
		)
	{
		assert_lt(i, alloc_consts.red_zone.size);

		return (alloc_consts.red_zone.random ^ (i * 0x9e3779b9)) >> ((i & 3) * 8);
	}


	void
	alloc_red_zone_init(
		void* ptr
		)
	{
		assert_not_null(ptr);

		uint8_t* cur_ptr = ptr;
		uint8_t* cur_ptr_end = ptr + alloc_consts.red_zone.size;
		alloc_t i = 0;

		while(cur_ptr < cur_ptr_end)
		{
			*(cur_ptr++) = alloc_red_zone_get_byte(i++);
		}
	}


	void
	alloc_red_zone_check(
		volatile const void* ptr,
		int dir
		)
	{
		assert_not_null(ptr);

		uint8_t* cur_ptr = (void*) ptr;
		uint8_t* cur_ptr_end = cur_ptr + alloc_consts.red_zone.size;
		alloc_t i = 0;

		while(cur_ptr < cur_ptr_end)
		{
			uint8_t want = alloc_red_zone_get_byte(i);
			assert_eq(*cur_ptr, want,
				{
					int off = dir == -1 ? alloc_consts.red_zone.size - i : i + 1;
					alloc_log_error("red_zone_check(): corruption detected at offset ", off, " from user pointer");
				}
				);

			++cur_ptr;
			++i;
		}
	}


#endif
