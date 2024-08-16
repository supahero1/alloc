#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#ifndef ALLOC_THREADS
	/* This adds a noticable performance overhead.
		If you run singlethread, better set it to 0. */
	#define ALLOC_THREADS 1
#endif

#if ALLOC_THREADS == 1
	#ifdef _WIN32
		#include <windows.h>

		typedef SRWLOCK AllocMutex;
	#else
		#include <pthread.h>

		typedef pthread_mutex_t AllocMutex;
	#endif

	#define ALLOC_MUTEX_SIZE ((sizeof(AllocMutex) + 4 + 7) / 8)
#else
	#define ALLOC_MUTEX_SIZE 0
#endif

#ifndef _const_func_
	#define _const_func_ __attribute__((const))
#endif

#ifndef _pure_func_
	#define _pure_func_ __attribute__((pure))
#endif

#ifndef _warn_unused_result_
	#define _warn_unused_result_ __attribute__((warn_unused_result))
#endif

#ifndef _alloc_func_
	#define _alloc_func_(dealloc)	\
		__attribute__((malloc(dealloc))) _warn_unused_result_
#endif

#ifndef _nonnull_
	#define _nonnull_ __attribute__((nonnull))
#endif

#ifndef _inline_
	#define _inline_ __attribute__((always_inline)) inline
#endif

#ifndef _aligned_
	#define _aligned_(alignment) __attribute__((aligned(alignment)))
#endif

#ifndef _packed_
	#define _packed_ __attribute__((packed))
#endif

#ifndef _in_
	#define _in_ const
#endif

#ifndef _in_opt_
	#define _in_opt_ const
#endif

#ifndef _out_
	#define _out_ _nonnull_
#endif

#ifndef _out_opt_
	#define _out_opt_
#endif

#ifndef _inout_
	#define _inout_ _nonnull_
#endif

#ifndef _inout_opt_
	#define _inout_opt_
#endif

#ifndef _opaque_
	/* `_opaque_` is used for arguments that you must not access directly.
	 *
	 * There are 2 uses for this:
	 *
	 * 1. Memory deallocating functions accept an `_opaque_` (`const`)
	 *	pointer on the basis that you must not access the pointer afterwards.
	 *	The pointer might or might not change its contents, but that does not
	 *	change your code or impact you in any way.
	 *
	 * 2. Internal structures (like allocation handles) are marked `_opaque_`
	 *	even though they are modified in many places in the library. This is
	 *	safe as long as you yourself do not try to access the structures. If
	 *	you did, it would be undefined behavior already (if using mutexes) so
	 *	this just makes it more difficult (compiler warnings, ugly code, even
	 *	more undefined behavior).
	 */
	#define _opaque_ const
#endif

#ifndef ALLOC_HAS_TYPES
	#define ALLOC_HAS_TYPES 1

	/* `AllocType` - Allocation types.
	 *
	 * This is a way to help minimize the effects of external fragmentation. You
	 * can always just ignore this and pass `ALLOC_TYPE_ANY` to allocation
	 * functions, but you should use this to your advantage if your application
	 * is supposed to run for a long time. Over time, unfortunate sequence of
	 * allocations and deallocations can lead to really ugly fragmentation,
	 * which consumes more memory than necessary.
	 *
	 * The idea is to use the most appropriate type for the lifetime or
	 * frequency of the object you are allocating (having both be corelated is
	 * perfect, but if you have to choose, stick to your choice and do not mix
	 * them). This will clump together objects of the same type. If you do not
	 * care, using the same type for all allocations is fine (all objects then
	 * go into the same pool).
	 *
	 * This ONLY works if you allocate objects that are of the same size, but
	 * with different frequency or lifetime. If you allocate objects of
	 * different sizes, this will not change anything as they will be in
	 * different pools anyway.
	 *
	 * Always benchmark to see if this actually helps your use case. For most
	 * normal programs, this is just a micro-optimization.
	 *
	 * Every type is unique and self-contained, which means different types can
	 * be used in parallel without any lock contention or interference.
	 *
	 * There is no overhead of having more types in the enum, but using more
	 * types will lead to slightly increased memory footprint (more allocators
	 * are used). Due to that, it is best to not use a new type just for
	 * allocating one or two objects. `AllocType` has the most impact when a lot
	 * of objects are allocated.
	 *
	 * You can define your own types. After doing so, define `ALLOC_HAS_TYPES`
	 * so that no redefinition occurs. Note that you must preserve
	 * `ALLOC_TYPE_ANY` (no matter what value) and `kALLOC_TYPE` (last element).
	 */
	typedef enum AllocType
	{
		ALLOC_TYPE_0,
		ALLOC_TYPE_ANY				= ALLOC_TYPE_0,
		ALLOC_TYPE_A				= ALLOC_TYPE_0,
		ALLOC_TYPE_AUTO				= ALLOC_TYPE_0,

		ALLOC_TYPE_1,
		ALLOC_TYPE_MICROSECONDS		= ALLOC_TYPE_1,
		ALLOC_TYPE_US				= ALLOC_TYPE_1,
		ALLOC_TYPE_FASTEST			= ALLOC_TYPE_1,

		ALLOC_TYPE_2,
		ALLOC_TYPE_MILLISECONDS		= ALLOC_TYPE_2,
		ALLOC_TYPE_MS				= ALLOC_TYPE_2,
		ALLOC_TYPE_FASTER			= ALLOC_TYPE_2,

		ALLOC_TYPE_3,
		ALLOC_TYPE_SECONDS			= ALLOC_TYPE_3,
		ALLOC_TYPE_S				= ALLOC_TYPE_3,
		ALLOC_TYPE_FAST				= ALLOC_TYPE_3,

		ALLOC_TYPE_4,
		ALLOC_TYPE_HOURS			= ALLOC_TYPE_4,
		ALLOC_TYPE_H				= ALLOC_TYPE_4,
		ALLOC_TYPE_MEDIUM			= ALLOC_TYPE_4,

		ALLOC_TYPE_5,
		ALLOC_TYPE_DAYS				= ALLOC_TYPE_5,
		ALLOC_TYPE_D				= ALLOC_TYPE_5,
		ALLOC_TYPE_SLOW				= ALLOC_TYPE_5,

		ALLOC_TYPE_6,
		ALLOC_TYPE_MONTHS			= ALLOC_TYPE_6,
		ALLOC_TYPE_M				= ALLOC_TYPE_6,
		ALLOC_TYPE_SLOWER			= ALLOC_TYPE_6,

		ALLOC_TYPE_7,
		ALLOC_TYPE_FOREVER			= ALLOC_TYPE_7,
		ALLOC_TYPE_F				= ALLOC_TYPE_7,
		ALLOC_TYPE_SLOWEST			= ALLOC_TYPE_7,

		kALLOC_TYPE
	}
	AllocType;
#endif


/* `AllocHandlePerf` - Performance statistics of an allocator handle.
 *
 * Handle's statistics are the sum of all its allocators' statistics. It is
 * not possible to retrieve individual statistics of every allocator held by
 * a handle in an automated way.
 */
typedef struct AllocHandlePerf
{
	/* The active number of allocations held by the handle.
	 */
	uint64_t Allocations;

	/* The number of active allocators held by the handle.
	 *
	 * This value is always 0 for handles with allocation size greater than
	 * the maximum specified to `AllocInitialize` (512KiB by default).
	 */
	uint64_t Allocators;
}
AllocHandlePerf;


/* `AllocHandleFlag` - Allocator handle flags.
 */
typedef enum AllocHandleFlag
{
	ALLOC_HANDLE_FLAG_NONE					= 0,

	/* By default, one allocator is freed automatically when there is two free
	 * ones. This flag does not wait for that condition and frees allocators
	 * as soon as they are empty. This can lead to a weird situation where
	 * one allocator is full and memory keeps being allocated and deallocated
	 * in succession, which keeps creating and destroying allocators.
	 */
	ALLOC_HANDLE_FLAG_IMMEDIATE_FREE		= 1 << 0,

	/* By default, allocators are freed when they are empty. This flag prevents
	 * that from happening. There is only one valid use case for this flag, and
	 * that is when you want to do a bulk deallocation followed by a bulk
	 * allocation of exactly the same size. That is rare, but when it happens,
	 * this flag might save some performance. In all other cases, using this is
	 * a bad idea (like, really bad).
	 */
	ALLOC_HANDLE_FLAG_DO_NOT_FREE			= 1 << 1,

	/* If you want to do some operations on a handle in bulk, you can cork it.
	 * That will prevent the lock from getting repetively locked and unlocked.
	 * Naturally, this only has any impact when `ALLOC_THREADS` is not disabled.
	 *
	 * You can also use this flag if, for instance, you create a separate state
	 * for a thread and do not share any of its handles with other threads. In
	 * that case, it is as if you are running singlethreaded, so you can use
	 * `AllocAddAllFlags(&Info, ALLOC_HANDLE_FLAG_CORK)` to instruct all handles
	 * to avoid locking. Note that before freeing the state, you must uncork all
	 * handles with `AllocDelAllFlags(&Info, ALLOC_HANDLE_FLAG_CORK)`.
	 */
	ALLOC_HANDLE_FLAG_CORK					= 1 << 2,
}
AllocHandleFlag;


/* `AllocHandle` - Allocator handle.
 *
 * An allocator handle is the main object that you use to manage memory. It
 * consists of a number of allocators, which are the actual entities that
 * allocate memory. Allocators are created and destroyed automatically by the
 * handle as needed.
 *
 * Every handle is unique and self-contained, which means different handles
 * can be used in parallel without any lock contention or interference.
 *
 * Handles do not have any allocation limit, they are only limited by the total
 * memory available to the process.
 *
 * Note that it is not possible to free all the memory allocated by a handle
 * automatically. You must pair every allocation you ever do with a matching
 * deallocation with the same handle and parameters.
 *
 * Handles themselves are lightweight - they do not allocate any memory for
 * themselves or require any additional besides this type. What uses memory
 * are the allocators that they hold, which are created on demand.
 */
typedef struct AllocHandle
{
	/* Private (could be protected by a mutex). Use getters and setters instead.
	 */
	uint64_t Internal[11 + ALLOC_MUTEX_SIZE];
}
AllocHandle;


/* `AllocPerf` - Performance statistics of an allocator.
 *
 * An allocator is part of a bigger whole and is owned by a handle. See
 * `AllocHandlePerf` for the statistics of a handle.
 */
typedef struct AllocPerf
{
	/* The active number of allocations held by the allocator.
	 */
	uint64_t Allocations;

	/* The highest number of active allocations ever held by the allocator.
	 * This value can never decrease.
	 */
	uint64_t HighWaterMark;
}
AllocPerf;


/* `AllocIndexFunc` - Callback fetching an allocator handle index from a size.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @return The index of the handle that will be used to allocate the object.
 *
 * This function is used to determine which handle will be used to allocate
 * an object of a given size. It is called once per allocation, and the returned
 * index is then used to access the handle.
 *
 * If the size is greater than the maximum allocation handle size specified to
 * `AllocInitialize`, the function must return `UINT32_MAX`.
 *
 * The default implementation is an optimized logarithm. You must provide your
 * own implementation if you want to use `CreateAllocHandle` with allocation
 * handles whose sizes are not consequent powers of 2 starting from 1.
 * Otherwise, pass `NULL` for the default implementation.
 */
typedef uint32_t
(*AllocIndexFunc)(
	uint64_t Size
	);


/* `AllocHandleInfo` - Allocator handle initialization information.
 */
typedef struct AllocHandleInfo
{
	/* The size of objects allocated by the handle.
	 */
	uint64_t AllocSize;

	/* The handle will allocate memory in blocks of this size and then
	 * suballocate objects from those blocks. It must be a power of 2.
	 *
	 * `BlockSize` must not be smaller than the internal allocator size plus at
	 * least one allocation size (at the given alignment). This is formulated as
	 * `(AllocatorSize + AllocSize + Alignment - 1) & -(Alignment - 1)`. This
	 * does not equal the memory that will actually have physical backing. In
	 * the equation, `AllocatorSize` varies depending on `AllocSize` and
	 * `Alignment`, and it may change in the future, so the possible values will
	 * not be listed here. The equation is only here to give you an idea of the
	 * lower bound. You should never request that little memory.
	 *
	 * `BlockSize` for `AllocSize = 1` has an upper bound of `65536`. For
	 * `AllocSize = 2`, that number is `131072`. Anything larger has the limit
	 * of 2GiB, but you should not set it higher than perhaps some tens of
	 * megabytes (assuming you will be using gigabytes) to avoid fragmentation.
	 * If you specify a value larger than the limit, it will be clamped down.
	 */
	uint64_t BlockSize;

	/* The alignment of the first object in a block. The alignment of the
	 * subsequent objects is equal to the greatest common divisor of the
	 * alignment and the size of the object. It must be a power of 2.
	 *
	 * `Alignment` is predetermined for `AllocSize = 1` to be `1`, and so in
	 * that case it is ignored.
	 */
	uint64_t Alignment;

	/* See `AllocIndexFunc` for more information.
	 */
	AllocIndexFunc IndexFunc;
}
AllocHandleInfo;


/* `AllocInfo` - Library state information.
 */
typedef struct AllocInfo
{
	/* Either `NULL`, in which case the defaults are used, or an array of
	 * `AllocHandleInfo` structures, one for each handle that will be created.
	 */
	AllocHandleInfo* Handles;

	/* The number of elements in the `Handles` array. This does not translate to
	 * the number of handles that will actually be created.
	 */
	uint64_t HandleCount;
}
AllocInfo;


/* `AllocAllocVirtual` - Allocate virtual memory.
 *
 * @param `Size` The size of the memory that will be allocated. It will be
 *	rounded up to the next multiple of page size.
 *
 * @return The pointer to the allocated memory, or `NULL` on failure (except
 *	when `Size = 0`).
 *
 * This function does not use any of the library's allocators. It directly
 * allocates memory from the operating system. This is sometimes useful,
 * but you should know yourself when and why. If you do not, do not use this.
 *
 * The memory is zeroed out. If you are on an embedded system, you might want
 * to use `MAP_UNINITIALIZED` and not use this library (this is supposed to be
 * a general-purpose library, not to be used when you know what you are doing).
 */
extern _alloc_func_(AllocFreeVirtual) void*
AllocAllocVirtual(
	uint64_t Size
	);


/* `AllocAllocVirtualAligned` - Allocate virtual memory with a given alignment.
 *
 * @param `Size` The size of the memory that will be allocated. It must be a
 *	power of 2.
 *
 * @param `Alignment` The alignment of the memory that will be allocated. It
 *	must be a power of 2 and at least 4096.
 *
 * @param `Ptr` A pointer to where the aligned allocated memory's pointer should
 *	be stored.
 *
 * @return The pointer that should be passed to `AllocFreeVirtual`, or `NULL`
 *	on failure (except when `Size = 0`).
 *
 * `Ptr` is not modified on failure. However, note that setting `Size = 0` does
 * not count as a failure, and so it sets `Ptr` to `NULL` in that case.
 *
 * See `AllocAllocVirtual` for more information.
 */
extern _alloc_func_(AllocFreeVirtual) void*
AllocAllocVirtualAligned(
	uint64_t Size,
	uint64_t Alignment,
	_out_ void** Ptr
	);


/* `AllocFreeVirtual` - Free virtual memory.
 *
 * @param `Ptr` The pointer to the memory that will be freed. It must be NULL or
 *	allocated by `AllocAllocVirtual`.
 *
 * @param `Size` The size of the memory that will be freed. It must be the same
 *	as the size passed to the allocation function (NOT rounded up to a multiple
 *	of the page size).
 *
 * See `AllocAllocVirtual` for more information.
 */
extern void
AllocFreeVirtual(
	_opaque_ void* Ptr,
	uint64_t Size
	);


/* `CreateAllocHandle` - Create an allocator handle with given parameters.
 *
 * @param `Info` The initialization information of the handle.
 *
 * @param `Handle` The handle that will be created.
 *
 * Note that usually using the default allocators via general functions which do
 * not accept a handle directly is sufficient. Using `CreateAllocHandle` could
 * however be necessitated by any of:
 *
 * 1. Needing to allocate a large number of objects of the same size linearly.
 *
 * 2. Needing to allocate a large number of objects with a size that is not
 *	a power of 2, which with the default allocators would waste some memory.
 *
 * 3. Having custom requirements for alignment.
 *
 * Creating your own handles is a way to bypass the `AllocType` mechanism, but
 * it requires that you know what you are doing and that you are doing it well.
 * It is also a way to manually increase parallelism.
 *
 * You can create as many handles as you want, but you should not create one
 * just to allocate a single object, as that would be wasteful.
 */
extern void
CreateAllocHandle(
	_in_ AllocHandleInfo* Info,
	_opaque_ AllocHandle* Handle
	);


/* `AllocSetFlagsH` - Set flags of an allocator handle.
 *
 * @param `Handle` The handle whose flags you want to set. It must have been
 *	created by `CreateAllocHandle` or returned by `GetAllocHandle`.
 *
 * @param `Flags` The flags that you want to set.
 *
 * You can set multiple flags by ORing them together.
 *
 * This overrides the previous flags set on the handle.
 */
extern void
AllocSetFlagsH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	);


/* `AllocSetFlagsST` - Set flags of an allocator handle.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to set.
 *
 * You can set multiple flags by ORing them together.
 *
 * This overrides the previous flags set on the handle.
 */
_inline_ void
AllocSetFlagsST(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocType Type,
	AllocHandleFlag Flags
	)
{
	AllocSetFlagsH(GetAllocHandleST(Info, Size, Type), Flags);
}


/* `AllocSetFlagsS` - Set flags of an allocator handle.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to set.
 *
 * You can set multiple flags by ORing them together.
 *
 * This overrides the previous flags set on the handle.
 */
_inline_ void
AllocSetFlagsS(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocHandleFlag Flags
	)
{
	AllocSetFlagsH(GetAllocHandleS(Info, Size), Flags);
}


/* `AllocSetFlagsT` - Set flags of an allocator handle.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to set.
 *
 * You can set multiple flags by ORing them together.
 *
 * This overrides the previous flags set on the handle.
 */
_inline_ void
AllocSetFlagsT(
	uint64_t Size,
	AllocType Type,
	AllocHandleFlag Flags
	)
{
	AllocSetFlagsH(GetAllocHandleT(Size, Type), Flags);
}


/* `AllocSetFlags` - Set flags of an allocator handle.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to set.
 *
 * You can set multiple flags by ORing them together.
 *
 * This overrides the previous flags set on the handle.
 */
_inline_ void
AllocSetFlags(
	uint64_t Size,
	AllocHandleFlag Flags
	)
{
	AllocSetFlagsH(GetAllocHandle(Size), Flags);
}


/* `AllocSetAllFlags` - Set flags of all allocator handles in a state.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Flags` The flags that you want to set.
 *
 * You can set multiple flags by ORing them together.
 *
 * This overrides the previous flags set on all handles in the state.
 */
extern void
AllocSetAllFlags(
	_opaque_ AllocInfo* Info,
	AllocHandleFlag Flags
	);


/* `AllocAddFlagsH` - Add flags to an allocator handle.
 *
 * @param `Handle` The handle whose flags you want to add. It must have been
 *	created by `CreateAllocHandle` or returned by `GetAllocHandle`.
 *
 * @param `Flags` The flags that you want to add.
 *
 * You can add multiple flags by ORing them together.
 *
 * This adds the given flags to the existing flags of the handle.
 */
extern void
AllocAddFlagsH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	);


/* `AllocAddFlagsST` - Add flags to an allocator handle.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to add.
 *
 * You can add multiple flags by ORing them together.
 *
 * This adds the given flags to the existing flags of the handle.
 */
_inline_ void
AllocAddFlagsST(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocType Type,
	AllocHandleFlag Flags
	)
{
	AllocAddFlagsH(GetAllocHandleST(Info, Size, Type), Flags);
}


/* `AllocAddFlagsS` - Add flags to an allocator handle.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to add.
 *
 * You can add multiple flags by ORing them together.
 *
 * This adds the given flags to the existing flags of the handle.
 */
_inline_ void
AllocAddFlagsS(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocHandleFlag Flags
	)
{
	AllocAddFlagsH(GetAllocHandleS(Info, Size), Flags);
}


/* `AllocAddFlagsT` - Add flags to an allocator handle.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to add.
 *
 * You can add multiple flags by ORing them together.
 *
 * This adds the given flags to the existing flags of the handle.
 */
_inline_ void
AllocAddFlagsT(
	uint64_t Size,
	AllocType Type,
	AllocHandleFlag Flags
	)
{
	AllocAddFlagsH(GetAllocHandleT(Size, Type), Flags);
}


/* `AllocAddFlags` - Add flags to an allocator handle.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to add.
 *
 * You can add multiple flags by ORing them together.
 *
 * This adds the given flags to the existing flags of the handle.
 */
_inline_ void
AllocAddFlags(
	uint64_t Size,
	AllocHandleFlag Flags
	)
{
	AllocAddFlagsH(GetAllocHandle(Size), Flags);
}


/* `AllocAddAllFlags` - Add flags to all allocator handles in a state.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Flags` The flags that you want to add.
 *
 * You can add multiple flags by ORing them together.
 *
 * This adds the given flags to the existing flags of all handles in the state.
 */
extern void
AllocAddAllFlags(
	_opaque_ AllocInfo* Info,
	AllocHandleFlag Flags
	);


/* `AllocDelFlagsH` - Remove flags from an allocator handle.
 *
 * @param `Handle` The handle whose flags you want to remove. It must have been
 *	created by `CreateAllocHandle` or returned by `GetAllocHandle`.
 *
 * @param `Flags` The flags that you want to remove.
 *
 * You can remove multiple flags by ORing them together.
 *
 * This removes the given flags from the existing flags of the handle.
 */
extern void
AllocDelFlagsH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	);


/* `AllocDelFlagsST` - Remove flags from an allocator handle.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to remove.
 *
 * You can remove multiple flags by ORing them together.
 *
 * This removes the given flags from the existing flags of the handle.
 */
_inline_ void
AllocDelFlagsST(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocType Type,
	AllocHandleFlag Flags
	)
{
	AllocDelFlagsH(GetAllocHandleST(Info, Size, Type), Flags);
}


/* `AllocDelFlagsS` - Remove flags from an allocator handle.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to remove.
 *
 * You can remove multiple flags by ORing them together.
 *
 * This removes the given flags from the existing flags of the handle.
 */
_inline_ void
AllocDelFlagsS(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocHandleFlag Flags
	)
{
	AllocDelFlagsH(GetAllocHandleS(Info, Size), Flags);
}


/* `AllocDelFlagsT` - Remove flags from an allocator handle.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to remove.
 *
 * You can remove multiple flags by ORing them together.
 *
 * This removes the given flags from the existing flags of the handle.
 */
_inline_ void
AllocDelFlagsT(
	uint64_t Size,
	AllocType Type,
	AllocHandleFlag Flags
	)
{
	AllocDelFlagsH(GetAllocHandleT(Size, Type), Flags);
}


/* `AllocDelFlags` - Remove flags from an allocator handle.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Flags` The flags that you want to remove.
 *
 * You can remove multiple flags by ORing them together.
 *
 * This removes the given flags from the existing flags of the handle.
 */
_inline_ void
AllocDelFlags(
	uint64_t Size,
	AllocHandleFlag Flags
	)
{
	AllocDelFlagsH(GetAllocHandle(Size), Flags);
}


/* `AllocDelAllFlags` - Remove flags from all allocator handles in a state.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Flags` The flags that you want to remove.
 *
 * You can remove multiple flags by ORing them together.
 *
 * This removes the given flags from the existing flags of all handles in the
 * state.
 */
extern void
AllocDelAllFlags(
	_opaque_ AllocInfo* Info,
	AllocHandleFlag Flags
	);


/* `AllocInitialize` - Create a library's state.
 *
 * @param `Info` Custom initialization information or `NULL` for the default.
 *
 * @return The state on success, or `NULL` on failure (lack of memory).
 *
 * You probably do not need to call this function. If you are only using general
 * functions that do not accept `AllocInfo`, they will automatically create and
 * use the default state, which is sufficient for most use cases.
 *
 * Examples that could use or necessitate this function:
 *
 * 1. You want to create a state with custom allocation handles (custom block
 *  sizes, allocation sizes, alignments, the number of handles, etc.).
 *
 * 2. You want to wrap the library in a class or a module.
 *
 * 3. You want to decrease lock contention by creating multiple states (which
 *	only requires separate handles, but having a state makes it easier).
 */
extern _alloc_func_(AllocDeinitialize) _opaque_ AllocInfo*
AllocInitialize(
	_in_ AllocInfo* Info
	);


/* `AllocDeinitialize` - Destroy a library's state.
 *
 * @param `Info` The library's state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`.
 *
 * If `Info` it is `NULL`, the library's private global state is destroyed.
 *
 * Every call to `AllocInitialize` must be paired with a call to
 * `AllocDeinitialize`.
 *
 * Any left-over allocators are destroyed, but only if there is at most one per
 * a handle. If that is not the case, you will get a memory leak. This could
 * happen only if:
 *
 * 1. You have not properly freed all the memory you allocated.
 *
 * 2. You have used `ALLOC_HANDLE_FLAG_DO_NOT_FREE` where not applicable.
 *
 * Note that using `ALLOC_HANDLE_FLAG_DO_NOT_FREE` does not cause any memory
 * leaks (kind of, or should maybe say "sometimes"). How the flag affects
 * the library is complex and will not be explained here. Use it only in
 * the use cases mentioned in the flag's description to avoid any problems.
 */
extern void
AllocDeinitialize(
	_opaque_ AllocInfo* Info
	);


/* `GetAllocHandleST` - Get an allocator handle given its parameters.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @return The handle.
 */
extern _pure_func_ _opaque_ AllocHandle*
GetAllocHandleST(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocType Type
	);


/* `GetAllocHandleS` - Get an allocator handle given its parameters.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @return The handle.
 */
_inline_ _pure_func_ _opaque_ AllocHandle*
GetAllocHandleS(
	_opaque_ AllocInfo* Info,
	uint64_t Size
	)
{
	return GetAllocHandleST(Info, Size, ALLOC_TYPE_ANY);
}


/* `GetAllocHandleT` - Get an allocator handle given its parameters.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @return The handle.
 */
_inline_ _pure_func_ _opaque_ AllocHandle*
GetAllocHandleT(
	uint64_t Size,
	AllocType Type
	)
{
	return GetAllocHandleST(NULL, Size, Type);
}


/* `GetAllocHandle` - Get an allocator handle given its size.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @return The handle.
 */
_inline_ _pure_func_ _opaque_ AllocHandle*
GetAllocHandle(
	uint64_t Size
	)
{
	return GetAllocHandleST(NULL, Size, ALLOC_TYPE_ANY);
}


/* `GetAllocHandlePerfH` - Get the performance statistics of an allocator
 * handle.
 *
 * @param `Handle` The handle whose statistics you want to get. It must have
 *	been created by `CreateAllocHandle` or returned by `GetAllocHandle`.
 *
 * @param `Perf` The statistics of the handle.
 */
extern void
GetAllocHandlePerfH(
	_opaque_ AllocHandle* Handle,
	_out_ AllocHandlePerf* Perf
	);


/* `GetAllocHandlePerfST` - Get the performance statistics of an allocator
 * handle.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @param `Perf` The statistics of the handle.
 */
_inline_ void
GetAllocHandlePerfST(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocType Type,
	_out_ AllocHandlePerf* Perf
	)
{
	GetAllocHandlePerfH(GetAllocHandleST(Info, Size, Type), Perf);
}


/* `GetAllocHandlePerfS` - Get the performance statistics of an allocator
 * handle.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Perf` The statistics of the handle.
 */
_inline_ void
GetAllocHandlePerfS(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	_out_ AllocHandlePerf* Perf
	)
{
	GetAllocHandlePerfH(GetAllocHandleS(Info, Size), Perf);
}


/* `GetAllocHandlePerfT` - Get the performance statistics of an allocator
 * handle.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Type` The `AllocType` of the objects that the handle allocates.
 *
 * @param `Perf` The statistics of the handle.
 */
_inline_ void
GetAllocHandlePerfT(
	uint64_t Size,
	AllocType Type,
	_out_ AllocHandlePerf* Perf
	)
{
	GetAllocHandlePerfH(GetAllocHandleT(Size, Type), Perf);
}


/* `GetAllocHandlePerf` - Get the performance statistics of an allocator handle.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @param `Perf` The statistics of the handle.
 */
_inline_ void
GetAllocHandlePerf(
	uint64_t Size,
	_out_ AllocHandlePerf* Perf
	)
{
	GetAllocHandlePerfH(GetAllocHandle(Size), Perf);
}


/* `GetAllocPerf` - Get the performance statistics of an allocator.
 *
 * @param `Ptr` The pointer to an object allocated by an allocator. It must be
 *	valid (allocated by the library and active). It does not matter which handle
 *	or library state allocated the object.
 *
 * @param `Size` The size of the object. It must be the same as the size passed
 *	to the allocation function.
 *
 * @param `Perf` The statistics of the allocator that allocated the object.
 *
 * This function dynamically resolves the pointer to an object allocated by an
 * allocator to the specific allocator that allocated it and returns the
 * allocator's statistics. It does not matter which `AllocType` was used to
 * allocate the object.
 *
 * An allocator is part of a bigger whole and is owned by a handle. To get the
 * total statistics of the handle, use `GetAllocHandlePerf`.
 */
extern void
GetAllocPerf(
	_in_ void* Ptr,
	uint64_t Size,
	_out_ AllocPerf* Perf
	);


/* `AllocAllocH` - Allocate an object.
 *
 * @param `Handle` The handle that will be used to allocate the object.
 *
 * @param `Zero` If non-zero, the object will be zeroed out.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
extern _alloc_func_(AllocFreeH) void*
AllocAllocH(
	_inout_ AllocHandle* Handle,
	int Zero
	);


/* `AllocAllocST` - Allocate an object.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @param `Type` The `AllocType` of the object that will be allocated.
 *
 * @param `Zero` If non-zero, the object will be zeroed out.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeST) void*
AllocAllocST(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocType Type,
	int Zero
	)
{
	return AllocAllocH(GetAllocHandleST(Info, Size, Type), Zero);
}


/* `AllocAllocS` - Allocate an object.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @param `Zero` If non-zero, the object will be zeroed out.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeS) void*
AllocAllocS(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	int Zero
	)
{
	return AllocAllocH(GetAllocHandleS(Info, Size), Zero);
}


/* `AllocAllocT` - Allocate an object.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @param `Type` The `AllocType` of the object that will be allocated.
 *
 * @param `Zero` If non-zero, the object will be zeroed out.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeT) void*
AllocAllocT(
	uint64_t Size,
	AllocType Type,
	int Zero
	)
{
	return AllocAllocH(GetAllocHandleT(Size, Type), Zero);
}


/* `AllocAlloc` - Allocate an object.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @param `Zero` If non-zero, the object will be zeroed out.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFree) void*
AllocAlloc(
	uint64_t Size,
	int Zero
	)
{
	return AllocAllocH(GetAllocHandle(Size), Zero);
}


/* `AllocFreeH` - Free an object.
 *
 * @param `Handle` The handle that was used to allocate the object.
 *
 * @param `Ptr` The pointer to the object that will be freed. It must be NULL or
 *	allocated by the library and active.
 *
 * Every `AllocAllocH` must be paired with an `AllocFreeH`.
 */
extern void
AllocFreeH(
	_opaque_ AllocHandle* Handle,
	_opaque_ void* Ptr
	);


/* `AllocFreeST` - Free an object.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the object that will be freed.
 *
 * @param `Type` The `AllocType` of the object that will be freed.
 *
 * @param `Ptr` The pointer to the object that will be freed. It must be NULL or
 *	allocated by the library and active.
 *
 * Every `AllocAllocST` must be paired with an `AllocFreeST`.
 */
_inline_ void
AllocFreeST(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocType Type,
	_opaque_ void* Ptr
	)
{
	AllocFreeH(GetAllocHandleST(Info, Size, Type), Ptr);
}


/* `AllocFreeS` - Free an object.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the object that will be freed.
 *
 * @param `Ptr` The pointer to the object that will be freed. It must be NULL or
 *	allocated by the library and active.
 *
 * Every `AllocAllocS` must be paired with an `AllocFreeS`.
 */
_inline_ void
AllocFreeS(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	_opaque_ void* Ptr
	)
{
	AllocFreeH(GetAllocHandleS(Info, Size), Ptr);
}


/* `AllocFreeT` - Free an object.
 *
 * @param `Size` The size of the object that will be freed.
 *
 * @param `Type` The `AllocType` of the object that will be freed.
 *
 * @param `Ptr` The pointer to the object that will be freed. It must be NULL or
 *	allocated by the library and active.
 *
 * Every `AllocAllocT` must be paired with an `AllocFreeT`.
 */
_inline_ void
AllocFreeT(
	uint64_t Size,
	AllocType Type,
	_opaque_ void* Ptr
	)
{
	AllocFreeH(GetAllocHandleT(Size, Type), Ptr);
}


/* `AllocFree` - Free an object.
 *
 * @param `Size` The size of the object that will be freed.
 *
 * @param `Ptr` The pointer to the object that will be freed. It must be NULL or
 *	allocated by the library and active.
 *
 * Every `AllocAlloc` must be paired with an `AllocFree`.
 */
_inline_ void
AllocFree(
	uint64_t Size,
	_opaque_ void* Ptr
	)
{
	AllocFreeH(GetAllocHandle(Size), Ptr);
}


/* `AllocReallocH` - Reallocate an object.
 *
 * @param `OldHandle` The handle that was used to allocate the object.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocAllocH`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function. It must be smaller
 *	or equal to the `OldHandle`'s allocation size. It is ignored if
 *	`OldPtr = NULL`.
 *
 * @param `NewHandle` The handle that will be used to allocate the new object.
 *	It can be the same as `OldHandle`.
 *
 * @param `NewSize` The size of the new object. It must be smaller or equal to
 *	the `NewHandle`'s allocation size.
 *
 * @param `Zero` If non-zero, the new object will be zeroed out.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocAllocH` and `AllocFreeH`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If `Zero` is non-zero and the new object is larger,
 * the new memory is zeroed out. The sizes are given to optimize the zeroing
 * process.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
extern _alloc_func_(AllocFreeH) void*
AllocReallocH(
	_opaque_ AllocHandle* OldHandle,
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	_opaque_ AllocHandle* NewHandle,
	uint64_t NewSize,
	int Zero
	);


/* `AllocReallocST` - Reallocate an object.
 *
 * @param `OldInfo` The old library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. `OldPtr` must have been allocated by it.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocAllocST`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `OldType` The `AllocType` of the object that will be reallocated.
 *
 * @param `NewInfo` The new library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. It can be the same as `OldInfo`.
 *
 * @param `NewSize` The size of the new object.
 *
 * @param `NewType` The `AllocType` of the new object.
 *
 * @param `Zero` If non-zero, the new object will be zeroed out.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocAllocST` and `AllocFreeST`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If `Zero` is non-zero and the new object is larger,
 * the new memory is zeroed out.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeST) void*
AllocReallocST(
	_opaque_ AllocInfo* OldInfo,
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	AllocType OldType,
	_opaque_ AllocInfo* NewInfo,
	uint64_t NewSize,
	AllocType NewType,
	int Zero
	)
{
	return AllocReallocH(
		GetAllocHandleST(OldInfo, OldSize, OldType),
		OldPtr,
		OldSize,
		GetAllocHandleST(NewInfo, NewSize, NewType),
		NewSize,
		Zero
	);
}


/* `AllocReallocS` - Reallocate an object.
 *
 * @param `OldInfo` The old library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. `OldPtr` must have been allocated by it.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocAllocS`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `NewInfo` The new library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. It can be the same as `OldInfo`.
 *
 * @param `NewSize` The size of the new object.
 *
 * @param `Zero` If non-zero, the new object will be zeroed out.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocAllocS` and `AllocFreeS`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If `Zero` is non-zero and the new object is larger,
 * the new memory is zeroed out.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeS) void*
AllocReallocS(
	_opaque_ AllocInfo* OldInfo,
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	_opaque_ AllocInfo* NewInfo,
	uint64_t NewSize,
	int Zero
	)
{
	return AllocReallocH(
		GetAllocHandleS(OldInfo, OldSize),
		OldPtr,
		OldSize,
		GetAllocHandleS(NewInfo, NewSize),
		NewSize,
		Zero
	);
}


/* `AllocReallocT` - Reallocate an object.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocAllocT`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `OldType` The `AllocType` of the object that will be reallocated.
 *
 * @param `NewSize` The size of the new object.
 *
 * @param `NewType` The `AllocType` of the new object.
 *
 * @param `Zero` If non-zero, the new object will be zeroed out.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocAllocT` and `AllocFreeT`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If `Zero` is non-zero and the new object is larger,
 * the new memory is zeroed out.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeT) void*
AllocReallocT(
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	AllocType OldType,
	uint64_t NewSize,
	AllocType NewType,
	int Zero
	)
{
	return AllocReallocH(
		GetAllocHandleT(OldSize, OldType),
		OldPtr,
		OldSize,
		GetAllocHandleT(NewSize, NewType),
		NewSize,
		Zero
	);
}


/* `AllocRealloc` - Reallocate an object.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocAlloc`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `NewSize` The size of the new object.
 *
 * @param `Zero` If non-zero, the new object will be zeroed out.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocAlloc` and `AllocFree`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If `Zero` is non-zero and the new object is larger,
 * the new memory is zeroed out.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFree) void*
AllocRealloc(
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	uint64_t NewSize,
	int Zero
	)
{
	return AllocReallocH(
		GetAllocHandle(OldSize),
		OldPtr,
		OldSize,
		GetAllocHandle(NewSize),
		NewSize,
		Zero
	);
}



/* `AllocMallocH` - Allocate an object.
 *
 * @param `Handle` The handle that will be used to allocate the object.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeH) void*
AllocMallocH(
	_opaque_ AllocHandle* Handle,
	uint64_t Size
	)
{
	return AllocAllocH(Handle, 0);
}


/* `AllocMallocST` - Allocate an object.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @param `Type` The `AllocType` of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeST) void*
AllocMallocST(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocType Type
	)
{
	return AllocAllocST(Info, Size, Type, 0);
}


/* `AllocMallocS` - Allocate an object.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeS) void*
AllocMallocS(
	_opaque_ AllocInfo* Info,
	uint64_t Size
	)
{
	return AllocAllocS(Info, Size, 0);
}


/* `AllocMallocT` - Allocate an object.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @param `Type` The `AllocType` of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeT) void*
AllocMallocT(
	uint64_t Size,
	AllocType Type
	)
{
	return AllocAllocT(Size, Type, 0);
}


/* `AllocMalloc` - Allocate an object.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFree) void*
AllocMalloc(
	uint64_t Size
	)
{
	return AllocAlloc(Size, 0);
}


/* `AllocCallocH` - Allocate an object and zero it out.
 *
 * @param `Handle` The handle that will be used to allocate the object.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeH) void*
AllocCallocH(
	_opaque_ AllocHandle* Handle,
	uint64_t Size
	)
{
	return AllocAllocH(Handle, 1);
}


/* `AllocCallocST` - Allocate an object and zero it out.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @param `Type` The `AllocType` of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeST) void*
AllocCallocST(
	_opaque_ AllocInfo* Info,
	uint64_t Size,
	AllocType Type
	)
{
	return AllocAllocST(Info, Size, Type, 1);
}


/* `AllocCallocS` - Allocate an object and zero it out.
 *
 * @param `Info` A library state. It must be `NULL` or a valid instance created
 *	by `AllocInitialize`.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeS) void*
AllocCallocS(
	_opaque_ AllocInfo* Info,
	uint64_t Size
	)
{
	return AllocAllocS(Info, Size, 1);
}


/* `AllocCallocT` - Allocate an object and zero it out.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @param `Type` The `AllocType` of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFreeT) void*
AllocCallocT(
	uint64_t Size,
	AllocType Type
	)
{
	return AllocAllocT(Size, Type, 1);
}


/* `AllocCalloc` - Allocate an object and zero it out.
 *
 * @param `Size` The size of the object that will be allocated.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 */
_inline_ _alloc_func_(AllocFree) void*
AllocCalloc(
	uint64_t Size
	)
{
	return AllocAlloc(Size, 1);
}


/* `AllocRemallocH` - Reallocate an object.
 *
 * @param `OldHandle` The handle that was used to allocate the object.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMallocH`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function. It must be smaller
 *	or equal to the `OldHandle`'s allocation size. It is ignored if
 *	`OldPtr = NULL`.
 *
 * @param `NewHandle` The handle that will be used to allocate the new object.
 *	It can be the same as `OldHandle`.
 *
 * @param `NewSize` The size of the new object. It must be smaller or equal to
 *	the `NewHandle`'s allocation size.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocMallocH` and `AllocFreeH`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeH) void*
AllocRemallocH(
	_opaque_ AllocHandle* OldHandle,
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	_opaque_ AllocHandle* NewHandle,
	uint64_t NewSize
	)
{
	return AllocReallocH(OldHandle, OldPtr, OldSize, NewHandle, NewSize, 0);
}


/* `AllocRemallocST` - Reallocate an object.
 *
 * @param `OldInfo` The old library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. `OldPtr` must have been allocated by it.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMallocST`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `OldType` The `AllocType` of the object that will be reallocated.
 *
 * @param `NewInfo` The new library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. It can be the same as `OldInfo`.
 *
 * @param `NewSize` The size of the new object.
 *
 * @param `NewType` The `AllocType` of the new object.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocMallocST` and `AllocFreeST`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeST) void*
AllocRemallocST(
	_opaque_ AllocInfo* OldInfo,
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	AllocType OldType,
	_opaque_ AllocInfo* NewInfo,
	uint64_t NewSize,
	AllocType NewType
	)
{
	return AllocReallocST(OldInfo, OldPtr, OldSize,
		OldType, NewInfo, NewSize, NewType, 0);
}


/* `AllocRemallocS` - Reallocate an object.
 *
 * @param `OldInfo` The old library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. `OldPtr` must have been allocated by it.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMallocS`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `NewInfo` The new library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. It can be the same as `OldInfo`.
 *
 * @param `NewSize` The size of the new object.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocMallocS` and `AllocFreeS`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeS) void*
AllocRemallocS(
	_opaque_ AllocInfo* OldInfo,
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	_opaque_ AllocInfo* NewInfo,
	uint64_t NewSize
	)
{
	return AllocReallocS(OldInfo, OldPtr, OldSize, NewInfo, NewSize, 0);
}


/* `AllocRemallocT` - Reallocate an object.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMallocT`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `OldType` The `AllocType` of the object that will be reallocated.
 *
 * @param `NewSize` The size of the new object.
 *
 * @param `NewType` The `AllocType` of the new object.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocMallocT` and `AllocFreeT`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeT) void*
AllocRemallocT(
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	AllocType OldType,
	uint64_t NewSize,
	AllocType NewType
	)
{
	return AllocReallocT(OldPtr, OldSize, OldType, NewSize, NewType, 0);
}


/* `AllocRemalloc` - Reallocate an object.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMalloc`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `NewSize` The size of the new object.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocMalloc` and `AllocFree`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFree) void*
AllocRemalloc(
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	uint64_t NewSize
	)
{
	return AllocRealloc(OldPtr, OldSize, NewSize, 0);
}


/* `AllocRecallocH` - Reallocate an object and zero it out.
 *
 * @param `OldHandle` The handle that was used to allocate the object.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMallocH`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function. It must be smaller
 *	or equal to the `OldHandle`'s allocation size. It is ignored if
 *	`OldPtr = NULL`.
 *
 * @param `NewHandle` The handle that will be used to allocate the new object.
 *	It can be the same as `OldHandle`.
 *
 * @param `NewSize` The size of the new object. It must be smaller or equal to
 *	the `NewHandle`'s allocation size.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocCallocH` and `AllocFreeH`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If the new object is larger, the new memory is
 * zeroed out. The sizes are given to optimize the zeroing process.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeH) void*
AllocRecallocH(
	_opaque_ AllocHandle* OldHandle,
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	_opaque_ AllocHandle* NewHandle,
	uint64_t NewSize
	)
{
	return AllocReallocH(OldHandle, OldPtr, OldSize, NewHandle, NewSize, 1);
}


/* `AllocRecallocST` - Reallocate an object and zero it out.
 *
 * @param `OldInfo` The old library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. `OldPtr` must have been allocated by it.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMallocST`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `OldType` The `AllocType` of the object that will be reallocated.
 *
 * @param `NewInfo` The new library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. It can be the same as `OldInfo`.
 *
 * @param `NewSize` The size of the new object.
 *
 * @param `NewType` The `AllocType` of the new object.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocCallocST` and `AllocFreeST`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If the new object is larger, the new memory is
 * zeroed out.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeST) void*
AllocRecallocST(
	_opaque_ AllocInfo* OldInfo,
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	AllocType OldType,
	_opaque_ AllocInfo* NewInfo,
	uint64_t NewSize,
	AllocType NewType
	)
{
	return AllocReallocST(OldInfo, OldPtr, OldSize,
		OldType, NewInfo, NewSize, NewType, 1);
}


/* `AllocRecallocS` - Reallocate an object and zero it out.
 *
 * @param `OldInfo` The old library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. `OldPtr` must have been allocated by it.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMallocS`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `NewInfo` The new library state. It must be `NULL` or a valid instance
 *	created by `AllocInitialize`. It can be the same as `OldInfo`.
 *
 * @param `NewSize` The size of the new object.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocCallocS` and `AllocFreeS`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If the new object is larger, the new memory is
 * zeroed out.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeS) void*
AllocRecallocS(
	_opaque_ AllocInfo* OldInfo,
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	_opaque_ AllocInfo* NewInfo,
	uint64_t NewSize
	)
{
	return AllocReallocS(OldInfo, OldPtr, OldSize, NewInfo, NewSize, 1);
}


/* `AllocRecallocT` - Reallocate an object and zero it out.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMallocT`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `OldType` The `AllocType` of the object that will be reallocated.
 *
 * @param `NewSize` The size of the new object.
 *
 * @param `NewType` The `AllocType` of the new object.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocCallocT` and `AllocFreeT`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If the new object is larger, the new memory is
 * zeroed out.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFreeT) void*
AllocRecallocT(
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	AllocType OldType,
	uint64_t NewSize,
	AllocType NewType
	)
{
	return AllocReallocT(OldPtr, OldSize, OldType, NewSize, NewType, 1);
}


/* `AllocRecalloc` - Reallocate an object and zero it out.
 *
 * @param `OldPtr` The pointer to the object that will be reallocated. It can
 *	be `NULL`, in which case the function behaves like `AllocMalloc`.
 *
 * @param `OldSize` The size of the object that will be reallocated. It must be
 *	the same as the size passed to the allocation function.
 *
 * @param `NewSize` The size of the new object.
 *
 * @return The pointer to the reallocated object.
 *
 * This is a combination of `AllocCalloc` and `AllocFree`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If the new object is larger, the new memory is
 * zeroed out.
 *
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
_inline_ _alloc_func_(AllocFree) void*
AllocRecalloc(
	_opaque_ void* OldPtr,
	uint64_t OldSize,
	uint64_t NewSize
	)
{
	return AllocRealloc(OldPtr, OldSize, NewSize, 1);
}


#ifdef __cplusplus
}
#endif
