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


extern void
alloc_report_init(
	void
	);


extern void
alloc_report_refresh(
	void
	);


extern void
alloc_report_stop(
	void
	);


extern void
alloc_report_virtual_alloc(
	alloc_t size
	);


extern void
alloc_report_virtual_free(
	alloc_t size
	);

