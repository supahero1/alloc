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

#ifndef ALLOC_RELEASE


	extern void
	alloc_red_zone_init(
		void* ptr
		);


	extern void
	alloc_red_zone_check(
		volatile const void* ptr,
		int dir
		);


#else
	#define alloc_red_zone_init(ptr) (void)(ptr)
	#define alloc_red_zone_check(ptr, dir) (void)(ptr); (void)(dir)
#endif
