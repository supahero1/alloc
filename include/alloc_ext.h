/*
 *   Copyright 2024 Franciszek Balcerak
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

#ifdef __cplusplus
extern "C" {
#endif

#include "alloc_std.h"

#ifndef _inline_
	#define _inline_ __attribute__((always_inline)) inline
#endif


_inline_ _pure_func_ _opaque_ AllocHandle*
AllocGetHandle(
	alloc_t Size
	)
{
	return AllocGetHandleS(AllocGetGlobalState(), Size);
}


_inline_ void
AllocHandleLockS(
	_in_ AllocState* State,
	alloc_t Size
	)
{
	AllocHandleLockH(AllocGetHandleS(State, Size));
}


_inline_ void
AllocHandleLock(
	alloc_t Size
	)
{
	AllocHandleLockH(AllocGetHandle(Size));
}


_inline_ void
AllocHandleUnlockS(
	_in_ AllocState* State,
	alloc_t Size
	)
{
	AllocHandleUnlockH(AllocGetHandleS(State, Size));
}


_inline_ void
AllocHandleUnlock(
	alloc_t Size
	)
{
	AllocHandleUnlockH(AllocGetHandle(Size));
}


_inline_ void
AllocHandleSetFlagsS(
	_in_ AllocState* State,
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleSetFlagsH(AllocGetHandleS(State, Size), Flags);
}


_inline_ void
AllocHandleSetFlagsUS(
	_in_ AllocState* State,
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleSetFlagsUH(AllocGetHandleS(State, Size), Flags);
}


_inline_ void
AllocHandleSetFlags(
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleSetFlagsH(AllocGetHandle(Size), Flags);
}


_inline_ void
AllocHandleSetFlagsU(
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleSetFlagsUH(AllocGetHandle(Size), Flags);
}


_inline_ void
AllocHandleAddFlagsS(
	_in_ AllocState* State,
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleAddFlagsH(AllocGetHandleS(State, Size), Flags);
}


_inline_ void
AllocHandleAddFlagsUS(
	_in_ AllocState* State,
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleAddFlagsUH(AllocGetHandleS(State, Size), Flags);
}


_inline_ void
AllocHandleAddFlags(
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleAddFlagsH(AllocGetHandle(Size), Flags);
}


_inline_ void
AllocHandleAddFlagsU(
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleAddFlagsUH(AllocGetHandle(Size), Flags);
}


_inline_ void
AllocHandleDelFlagsS(
	_in_ AllocState* State,
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleDelFlagsH(AllocGetHandleS(State, Size), Flags);
}


_inline_ void
AllocHandleDelFlagsUS(
	_in_ AllocState* State,
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleDelFlagsUH(AllocGetHandleS(State, Size), Flags);
}


_inline_ void
AllocHandleDelFlags(
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleDelFlagsH(AllocGetHandle(Size), Flags);
}


_inline_ void
AllocHandleDelFlagsU(
	alloc_t Size,
	AllocHandleFlag Flags
	)
{
	AllocHandleDelFlagsUH(AllocGetHandle(Size), Flags);
}


_inline_ AllocHandleFlag
AllocHandleGetFlagsS(
	_in_ AllocState* State,
	alloc_t Size
	)
{
	return AllocHandleGetFlagsH(AllocGetHandleS(State, Size));
}


_inline_ AllocHandleFlag
AllocHandleGetFlagsUS(
	_in_ AllocState* State,
	alloc_t Size
	)
{
	return AllocHandleGetFlagsUH(AllocGetHandleS(State, Size));
}


_inline_ AllocHandleFlag
AllocHandleGetFlags(
	alloc_t Size
	)
{
	return AllocHandleGetFlagsH(AllocGetHandle(Size));
}


_inline_ AllocHandleFlag
AllocHandleGetFlagsU(
	alloc_t Size
	)
{
	return AllocHandleGetFlagsUH(AllocGetHandle(Size));
}


_inline_ void*
AllocAllocS(
	_in_ AllocState* State,
	alloc_t Size,
	int Zero
	)
{
	return AllocAllocH(AllocGetHandleS(State, Size), Size, Zero);
}


_inline_ void*
AllocAllocUS(
	_in_ AllocState* State,
	alloc_t Size,
	int Zero
	)
{
	return AllocAllocUH(AllocGetHandleS(State, Size), Size, Zero);
}


_inline_ void*
AllocAlloc(
	alloc_t Size,
	int Zero
	)
{
	return AllocAllocH(AllocGetHandle(Size), Size, Zero);
}


_inline_ void*
AllocAllocU(
	alloc_t Size,
	int Zero
	)
{
	return AllocAllocUH(AllocGetHandle(Size), Size, Zero);
}


_inline_ void
AllocFreeS(
	_in_ AllocState* State,
	alloc_t Size,
	_in_ void* Ptr
	)
{
	AllocFreeH(AllocGetHandleS(State, Size), Ptr, Size);
}


_inline_ void
AllocFreeUS(
	_in_ AllocState* State,
	alloc_t Size,
	_in_ void* Ptr
	)
{
	AllocFreeUH(AllocGetHandleS(State, Size), Ptr, Size);
}


_inline_ void
AllocFree(
	alloc_t Size,
	_in_ void* Ptr
	)
{
	AllocFreeH(AllocGetHandle(Size), Ptr, Size);
}


_inline_ void
AllocFreeU(
	alloc_t Size,
	_in_ void* Ptr
	)
{
	AllocFreeUH(AllocGetHandle(Size), Ptr, Size);
}


_inline_ void*
AllocReallocS(
	_in_ AllocState* OldState,
	alloc_t OldSize,
	_in_ void* Ptr,
	_in_ AllocState* NewState,
	alloc_t NewSize,
	int Zero
	)
{
	return AllocReallocH(AllocGetHandleS(OldState, OldSize), Ptr, OldSize,
		AllocGetHandleS(NewState, NewSize), NewSize, Zero);
}


_inline_ void*
AllocReallocUS(
	_in_ AllocState* OldState,
	alloc_t OldSize,
	_in_ void* Ptr,
	_in_ AllocState* NewState,
	alloc_t NewSize,
	int Zero
	)
{
	return AllocReallocUH(AllocGetHandleS(OldState, OldSize), Ptr, OldSize,
		AllocGetHandleS(NewState, NewSize), NewSize, Zero);
}


_inline_ void*
AllocRealloc(
	alloc_t OldSize,
	_in_ void* Ptr,
	alloc_t NewSize,
	int Zero
	)
{
	return AllocReallocH(AllocGetHandle(OldSize), Ptr, OldSize,
		AllocGetHandle(NewSize), NewSize, Zero);
}


_inline_ void*
AllocReallocU(
	alloc_t OldSize,
	_in_ void* Ptr,
	alloc_t NewSize,
	int Zero
	)
{
	return AllocReallocUH(AllocGetHandle(OldSize), Ptr, OldSize,
		AllocGetHandle(NewSize), NewSize, Zero);
}


_inline_ void*
AllocMallocS(
	_in_ AllocState* State,
	alloc_t Size
	)
{
	return AllocAllocS(State, Size, 0);
}


_inline_ void*
AllocMallocUS(
	_in_ AllocState* State,
	alloc_t Size
	)
{
	return AllocAllocUS(State, Size, 0);
}


_inline_ void*
AllocMalloc(
	alloc_t Size
	)
{
	return AllocAlloc(Size, 0);
}


_inline_ void*
AllocMallocU(
	alloc_t Size
	)
{
	return AllocAllocU(Size, 0);
}


_inline_ void*
AllocCallocS(
	_in_ AllocState* State,
	alloc_t Size
	)
{
	return AllocAllocS(State, Size, 1);
}


_inline_ void*
AllocCallocUS(
	_in_ AllocState* State,
	alloc_t Size
	)
{
	return AllocAllocUS(State, Size, 1);
}


_inline_ void*
AllocCalloc(
	alloc_t Size
	)
{
	return AllocAlloc(Size, 1);
}


_inline_ void*
AllocCallocU(
	alloc_t Size
	)
{
	return AllocAllocU(Size, 1);
}


_inline_ void*
AllocRemallocS(
	_in_ AllocState* OldState,
	alloc_t OldSize,
	_in_ void* Ptr,
	_in_ AllocState* NewState,
	alloc_t NewSize
	)
{
	return AllocReallocS(OldState, OldSize, Ptr, NewState, NewSize, 0);
}


_inline_ void*
AllocRemallocUS(
	_in_ AllocState* OldState,
	alloc_t OldSize,
	_in_ void* Ptr,
	_in_ AllocState* NewState,
	alloc_t NewSize
	)
{
	return AllocReallocUS(OldState, OldSize, Ptr, NewState, NewSize, 0);
}


_inline_ void*
AllocRemalloc(
	alloc_t OldSize,
	_in_ void* Ptr,
	alloc_t NewSize
	)
{
	return AllocRealloc(OldSize, Ptr, NewSize, 0);
}


_inline_ void*
AllocRemallocU(
	alloc_t OldSize,
	_in_ void* Ptr,
	alloc_t NewSize
	)
{
	return AllocReallocU(OldSize, Ptr, NewSize, 0);
}


_inline_ void*
AllocRecallocS(
	_in_ AllocState* OldState,
	alloc_t OldSize,
	_in_ void* Ptr,
	_in_ AllocState* NewState,
	alloc_t NewSize
	)
{
	return AllocReallocS(OldState, OldSize, Ptr, NewState, NewSize, 1);
}


_inline_ void*
AllocRecallocUS(
	_in_ AllocState* OldState,
	alloc_t OldSize,
	_in_ void* Ptr,
	_in_ AllocState* NewState,
	alloc_t NewSize
	)
{
	return AllocReallocUS(OldState, OldSize, Ptr, NewState, NewSize, 1);
}


_inline_ void*
AllocRecalloc(
	alloc_t OldSize,
	_in_ void* Ptr,
	alloc_t NewSize
	)
{
	return AllocRealloc(OldSize, Ptr, NewSize, 1);
}


_inline_ void*
AllocRecallocU(
	alloc_t OldSize,
	_in_ void* Ptr,
	alloc_t NewSize
	)
{
	return AllocReallocU(OldSize, Ptr, NewSize, 1);
}


#ifdef __cplusplus
}
#endif
