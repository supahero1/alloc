#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>


/* `alloc_t` - Index type.
 *
 * The library often stores variables that directly corelate to memory, and
 * that often also matches the native word size. To not type `uintptr_t` all
 * the time, this type is defined.
 *
 * This cannot be `size_t`, because `size_t` is not guaranteed to be able to
 * hold a pointer. Also, having this match a pointer size makes things easier.
 */
typedef uintptr_t alloc_t;


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

	#define ALLOC_MUTEX_SIZE	\
		((sizeof(AllocMutex) + sizeof(alloc_t) - 1) & ~(sizeof(alloc_t) - 1))
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
	#define _alloc_func_ __attribute__((malloc)) _warn_unused_result_
#endif

#ifndef _nonnull_
	#define _nonnull_
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
	 *	safe as long as you do not try to access the structures by yourself.
	 */
	#define _opaque_ const
#endif


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
	 *
	 * This might be useful when you do not want to cache deallocated memory.
	 *
	 * This flag has precedence over `ALLOC_HANDLE_FLAG_DO_NOT_FREE`.
	 */
	ALLOC_HANDLE_FLAG_IMMEDIATE_FREE		= 1 << 0,

	/* By default, allocators are freed when they are empty. This flag prevents
	 * that from happening. There is only one valid use case for this flag, and
	 * that is when you want to do a bulk deallocation followed by a bulk
	 * allocation of exactly the same size or larger. In any other cases, you
	 * will get a memory leak. Note that it does not matter if it is "bulk" or
	 * not, but this flag only becomes noticable with a lot of operations.
	 */
	ALLOC_HANDLE_FLAG_DO_NOT_FREE			= 1 << 1,
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
 * are the allocators that they hold.
 */
typedef struct AllocHandle
{
	/* Private. Use getters and setters instead.
	 */
	alloc_t Internal[10 + ALLOC_MUTEX_SIZE];
}
AllocHandle;


/* `AllocHandleInfo` - Allocator handle initialization information.
 */
typedef struct AllocHandleInfo
{
	/* The size of objects allocated by the handle.
	 */
	alloc_t AllocSize;

	/* The handle will allocate memory in blocks of this size and then
	 * suballocate objects from those blocks. It must be a power of 2.
	 *
	 * `BlockSize` for `AllocSize = 1` has an upper bound of `65536`. For
	 * `AllocSize = 2`, that number is `131072`. Anything larger has the limit
	 * of 1GiB, but you should not set it higher than perhaps some tens of
	 * megabytes (assuming you will be using gigabytes) to avoid fragmentation.
	 * If you specify a value larger than the limit, it will be clamped.
	 *
	 * The value will automatically be clamped to the bare minimum required so
	 * that the allocator is able to make at least one allocation. It will also
	 * be automatically adjusted to meet the desired `AllocSize` and `Alignment`
	 * requirements while wasting as little memory as possible. This means that
	 * you do not actually have to specify a power of 2 as the value, since it
	 * will be adjusted to one, but it is better if you precalculate it yourself
	 * so that the library does not have to do it.
	 */
	alloc_t BlockSize;

	/* The alignment of the first object in a block. The alignment of the
	 * subsequent objects is equal to the greatest common divisor of the
	 * alignment and the size of the object. It must be a power of 2. It will
	 * be capped at the page size.
	 *
	 * `Alignment` is predetermined for `AllocSize = 1` to be `1`, and so in
	 * that case it is ignored. In other cases, it is theoretically unlimited.
	 */
	alloc_t Alignment;
}
AllocHandleInfo;


/* `AllocIndexFunc` - Callback fetching an allocator handle index from a size.
 *
 * @param `Size` The size of the object that will be allocated (greater than 0).
 *
 * @return The index of the handle that will be used to allocate the object.
 *
 * This function is used to determine which handle will be used to allocate
 * an object of a given size. It is called once per a memory operation, and
 * the returned index is then used to access the handle.
 *
 * If the size is greater than the maximum allocation handle size specified to
 * `AllocAllocState`, the function must return a value greater or equal to
 * the number of handles provided as initialization to the creation of the state
 * this function is used with (see `AllocInfo`). If you are not sure what that
 * value is, or if you change it often, you can return `UINT32_MAX`.
 *
 * The default implementation is an optimized logarithm. You must provide your
 * own implementation if you want to use `AllocCreateHandle` with allocation
 * handles whose sizes are not consequent powers of 2 starting from 1.
 * Otherwise, pass `NULL` for the default implementation.
 *
 * Handles are retrieved often, which is why this function should be rather
 * fast (ideally, `O(1)`). That is also why it does not accept state as
 * an argument. You should not write generic index functions. Write an optimized
 * one for the allocator handles you are using.
 *
 * You can skip the indexing step by using functions that accept an allocator
 * handle directly (the `H` suffix). That is faster, but requires you to do
 * the work yourself.
 */
typedef uint32_t
(*AllocIndexFunc)(
	alloc_t Size
	);


/* `AllocStateInfo` - Library state initialization information.
 */
typedef struct AllocStateInfo
{
	/* An array of `AllocHandleInfo` structures, one for each handle that will
	 * be created.
	 */
	AllocHandleInfo* Handles;

	/* The number of elements in the `Handles` array. This plus one virtual
	 * handle is what will be created.
	 */
	alloc_t HandleCount;

	/* See `AllocIndexFunc` for more information.
	 */
	AllocIndexFunc IndexFunc;
}
AllocStateInfo;


/* `AllocState` - A library state.
 *
 * You can access this structure directly.
 */
typedef struct AllocState
{
	AllocIndexFunc IndexFunc;

	alloc_t HandleCount;
	AllocHandle Handles[/*HandleCount*/];
}
AllocState;


/* `AllocGetGlobalState` - Get the library's global state.
 *
 * The global state is used by default for all general functions that do not
 * accept a state directly. It is sufficient for most use cases.
 */
extern _const_func_ const AllocState*
AllocGetGlobalState(
	void
	);


/* `AllocGetPageSize` - Get the system's page size.
 */
extern _const_func_ alloc_t
AllocGetPageSize(
	void
	);


/* `AllocAllocVirtual` - Allocate virtual memory.
 *
 * @param `Size` The size of the memory that will be allocated.
 *
 * @return The pointer to the allocated memory, or `NULL` on failure (except
 *	when `Size = 0`).
 *
 * This function does not use any of the library's allocators. It directly
 * allocates memory from the operating system. This is sometimes useful,
 * but you should know yourself when and why. If you do not, do not use this.
 *
 * Every call to `AllocAllocVirtual` must be paired with a call to
 * `AllocFreeVirtual`.
 */
extern _alloc_func_ void*
AllocAllocVirtual(
	alloc_t Size
	);


/* `AllocFreeVirtual` - Free virtual memory.
 *
 * @param `Ptr` The pointer to the memory that will be freed. It must be `NULL`
 *	or allocated by `AllocAllocVirtual`.
 *
 * @param `Size` The size of the memory that will be freed. It must be the same
 *	as the size passed to the allocation function.
 *
 * See `AllocAllocVirtual` for more information.
 */
extern void
AllocFreeVirtual(
	_opaque_ void* Ptr,
	alloc_t Size
	);


/* `AllocAllocVirtualAligned` - Allocate virtual memory with a given alignment.
 *
 * @param `Size` The size of the memory that will be allocated. It must be a
 *	power of 2.
 *
 * @param `Alignment` The alignment of the memory that will be allocated. It
 *	must be a power of 2.
 *
 * @param `Ptr` A pointer to where the aligned allocated memory's pointer should
 *	be stored.
 *
 * @return The pointer (called `RealPtr`) that should be passed to
 *	`AllocFreeVirtual`, or `NULL` on failure (except when `Size = 0`).
 *
 * See `AllocAllocVirtual` for more information.
 */
extern _alloc_func_ void*
AllocAllocVirtualAligned(
	alloc_t Size,
	alloc_t Alignment,
	_out_ void** Ptr
	);


/* `AllocFreeVirtualAligned` - Free virtual memory allocated with a given
 * alignment.
 *
 * @param `RealPtr` The pointer to the memory that will be freed. It must be
 *	`NULL` or allocated by `AllocAllocVirtualAligned`.
 *
 * @param `Size` The size of the memory that will be freed. It must be the same
 *	as the size passed to the allocation function.
 *
 * @param `Alignment` The alignment of the memory that was allocated. It must be
 *	the same as the alignment passed to the allocation function.
 *
 * See `AllocAllocVirtualAligned` for more information.
 *
 * Note that `RealPtr` is the output from `AllocAllocVirtualAligned`, and NOT
 * the pointer that is populated by the function that you pass as an argument.
 */
extern void
AllocFreeVirtualAligned(
	_opaque_ void* RealPtr,
	alloc_t Size,
	alloc_t Alignment
	);


/* See `AllocAllocVirtual` for more information.
 */
extern _alloc_func_ void*
AllocReallocVirtual(
	_opaque_ void* Ptr,
	alloc_t OldSize,
	alloc_t NewSize
	);


/* See `AllocAllocVirtualAligned` for more information.
 *
 * Note that `RealPtr` is the output from `AllocAllocVirtualAligned`, and NOT
 * the pointer that is populated by the function that you pass as an argument.
 */
extern _alloc_func_ void*
AllocReallocVirtualAligned(
	_opaque_ void* RealPtr,
	alloc_t OldSize,
	alloc_t NewSize,
	alloc_t Alignment,
	_out_ void** NewPtr
	);


/* `AllocCreateHandle` - Create an allocator handle with given parameters.
 *
 * @param `Info` The initialization information of the handle.
 *
 * @param `Handle` The handle that will be created.
 *
 * Note that usually using the default allocators via general functions which do
 * not accept a handle directly is sufficient. Using `AllocCreateHandle` could
 * however be necessitated by any of:
 *
 * 1. Needing to allocate a large number of objects of the same size linearly,
 *	uninterrupted by other allocations that might occur in the meantime.
 *
 * 2. Needing to allocate a large number of objects with a size that is not
 *	a power of 2, which with the default allocators would waste some memory.
 *
 * 3. Having custom requirements for alignment.
 *
 * There is no limit on the number of handles you can create.
 *
 * Passing `Info = NULL` will create a virtual memory handle (all allocations
 * handled explicitly by the operating system). Virtual handles are aligned to
 * the page size, performance statistics show zero allocators (but the correct
 * number of allocations), caching is non existent (memory freed immediately),
 * and they use the same APIs as any other handle. Note that usually this only
 * makes sense for big allocations.
 *
 * Every call to `AllocCreateHandle` must be paired with a call to
 * `AllocDestroyHandle`.
 */
extern void
AllocCreateHandle(
	_in_ AllocHandleInfo* Info,
	_opaque_ AllocHandle* Handle
	);


/* `AllocDestroyHandle` - Destroy an allocator handle.
 *
 * @param `Handle` The handle that you want to destroy. It must have been
 *	created by you using `AllocCreateHandle`.
 *
 * If the handle is not virtual and there have been allocations made with it,
 * there will be one leftover allocator that this function will free. There can
 * however be multiple leftover allocators if you have misused the library. In
 * that case, only one allocator will be freed. Possible causes for that are:
 *
 * 1. You have not properly freed all the memory you allocated.
 *
 * 2. You have used `ALLOC_HANDLE_FLAG_DO_NOT_FREE` where not applicable.
 *
 * Every call to `AllocCreateHandle` must be paired with a call to
 * `AllocDestroyHandle`.
 */
extern void
AllocDestroyHandle(
	_opaque_ AllocHandle* Handle
	);


/* `AllocAllocState` - Allocate a library state.
 *
 * @param `Info` Custom initialization information or `NULL` for the default.
 *
 * @return The state on success, or `NULL` on failure (lack of memory).
 *
 * You probably do not need to call this function. If you are only using general
 * functions that do not accept `AllocState`, they will use the global state,
 * which is sufficient for most use cases.
 *
 * Examples that could use or necessitate this function:
 *
 * 1. You want to create a state with custom allocation handles (custom block
 *  sizes, allocation sizes, alignments, the number of handles, etc.).
 *
 * 2. You want to wrap the library in a class or a module.
 *
 * 3. You want to decrease lock contention by creating multiple states (which
 *	only requires separate handles, but having a state might make it easier).
 *
 * 4. You want to decrease memory fragmentation.
 *
 * This function is called automatically for the global state unless you define
 * `ALLOC_DO_NOT_AUTO_INIT_GLOBAL_STATE`.
 *
 * Every call to `AllocAllocState` must be paired with a call to
 * `AllocFreeState`.
 */
extern _alloc_func_ const AllocState*
AllocAllocState(
	_in_ AllocStateInfo* Info
	);


/* `AllocFreeState` - Free a library state.
 *
 * @param `State` A library state returned by `AllocAllocState`.
 *
 * See `AllocDestroyHandle` for more information.
 *
 * This function is called automatically for the global state unless you define
 * `ALLOC_DO_NOT_AUTO_INIT_GLOBAL_STATE`.
 *
 * Every call to `AllocAllocState` must be paired with a call to
 * `AllocFreeState`.
 */
extern void
AllocFreeState(
	_opaque_ AllocState* State
	);


/* `AllocGetHandleS` - Get an allocator handle given its allocation size.
 *
 * @param `State` A library state returned by `AllocAllocState`.
 *
 * @param `Size` The size of the objects that the handle allocates.
 *
 * @return The handle.
 *
 * The returned handle is a part of the specified state.
 */
extern _pure_func_ _opaque_ AllocHandle*
AllocGetHandleS(
	_in_ AllocState* State,
	alloc_t Size
	);


/* `AllocHandleLockH` - Lock an allocator handle.
 *
 * @param `Handle` The handle that you want to lock. It must have been created
 *	by `AllocCreateHandle` or returned by `AllocGetHandle`.
 *
 * This function is not reentrant. If you lock a handle that is already locked
 * by the same thread, the program will deadlock.
 *
 * Locking a handle enables the use of unlocked functions. This has performance
 * benefits if multiple of these functions are called in succession. If you are
 * using only one or two functions, you should use the default locked versions.
 *
 * Every call to `AllocHandleLockH` must be paired with a call to
 * `AllocHandleUnlockH`.
 */
extern void
AllocHandleLockH(
	_opaque_ AllocHandle* Handle
	);


/* `AllocHandleUnlockH` - Unlock an allocator handle.
 *
 * @param `Handle` The handle that you want to unlock. It must have been locked
 *	by `AllocHandleLockH`.
 *
 * Every call to `AllocHandleLockH` must be paired with a call to
 * `AllocHandleUnlockH`.
 */
extern void
AllocHandleUnlockH(
	_opaque_ AllocHandle* Handle
	);


/* `AllocHandleSetFlagsH` - Set flags of an allocator handle.
 *
 * @param `Handle` The handle whose flags you want to set. It must have been
 *	created by `AllocCreateHandle` or returned by `AllocGetHandle`.
 *
 * @param `Flags` The flags that you want to set.
 *
 * You can set multiple flags by ORing them together.
 *
 * This overrides the previous flags set on the handle.
 */
extern void
AllocHandleSetFlagsH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	);


/* See `AllocHandleSetFlagsH` and `AllocHandleLockH` for more information.
 */
extern void
AllocHandleSetFlagsUH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	);


/* `AllocHandleAddFlagsH` - Add flags to an allocator handle.
 *
 * @param `Handle` The handle whose flags you want to add. It must have been
 *	created by `AllocCreateHandle` or returned by `AllocGetHandle`.
 *
 * @param `Flags` The flags that you want to add.
 *
 * You can add multiple flags by ORing them together.
 *
 * This adds the given flags to the existing flags of the handle.
 */
extern void
AllocHandleAddFlagsH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	);


/* See `AllocHandleAddFlagsH` and `AllocHandleLockH` for more information.
 */
extern void
AllocHandleAddFlagsUH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	);


/* `AllocHandleDelFlagsH` - Remove flags from an allocator handle.
 *
 * @param `Handle` The handle whose flags you want to remove. It must have been
 *	created by `AllocCreateHandle` or returned by `AllocGetHandle`.
 *
 * @param `Flags` The flags that you want to remove.
 *
 * You can remove multiple flags by ORing them together.
 *
 * This removes the given flags from the existing flags of the handle.
 */
extern void
AllocHandleDelFlagsH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	);


/* See `AllocHandleDelFlagsH` and `AllocHandleLockH` for more information.
 */
extern void
AllocHandleDelFlagsUH(
	_opaque_ AllocHandle* Handle,
	AllocHandleFlag Flags
	);


/* `AllocHandleGetFlagsH` - Get the flags of an allocator handle.
 *
 * @param `Handle` The handle whose flags you want to get. It must have been
 *	created by `AllocCreateHandle` or returned by `AllocGetHandle`.
 *
 * @return The flags of the handle.
 */
extern AllocHandleFlag
AllocHandleGetFlagsH(
	_opaque_ AllocHandle* Handle
	);


/* See `AllocHandleGetFlagsH` and `AllocHandleLockH` for more information.
 */
extern AllocHandleFlag
AllocHandleGetFlagsUH(
	_opaque_ AllocHandle* Handle
	);


/* `AllocAllocH` - Allocate an object.
 *
 * @param `Handle` The handle that will be used to allocate the object.
 *
 * @param `Size` The size of the object.
 *
 * @param `Zero` If non-zero, the object will be zeroed out.
 *
 * @return The pointer to the allocated object.
 *
 * The output pointer is guaranteed to be aligned to the handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * For all handles except virtual ones, `Size` must be less than or equal to
 * the handle's allocation size. For virtual handles, there is no such limit.
 */
extern _alloc_func_ void*
AllocAllocH(
	_opaque_ AllocHandle* Handle,
	alloc_t Size,
	int Zero
	);


/* See `AllocAllocH` and `AllocHandleLockH` for more information.
 */
extern _alloc_func_ void*
AllocAllocUH(
	_opaque_ AllocHandle* Handle,
	alloc_t Size,
	int Zero
	);


/* `AllocFreeH` - Free an object.
 *
 * @param `Handle` The handle that allocated the object. It must have been
 *	created by `AllocCreateHandle` or returned by `AllocGetHandle`.
 *
 * @param `Ptr` The pointer to the object that will be freed. It must be `NULL`
 *	or a valid pointer returned by an allocation function.
 *
 * @param `Size` The size of the object that will be freed. It must be the same
 *	as the size passed to the allocation function.
 *
 * The pointer is invalidated, if it was not `NULL`, after the function returns.
 * This does not however mean that any further access to it will cause a crash.
 *
 * Every `AllocAllocH` must be paired with an `AllocAllocH`.
 */
extern void
AllocFreeH(
	_opaque_ AllocHandle* Handle,
	_opaque_ void* Ptr,
	alloc_t Size
	);


/* See `AllocFreeH` and `AllocHandleLockH` for more information.
 */
extern void
AllocFreeUH(
	_opaque_ AllocHandle* Handle,
	_opaque_ void* Ptr,
	alloc_t Size
	);


/* `AllocReallocH` - Reallocate an object.
 *
 * See `AllocAllocH` and `AllocFreeH` for more information.
 *
 * This is a combination of `AllocAllocH` and `AllocFree`. The old object is
 * freed and a new object is allocated. The contents of the old object are
 * copied to the new object. If `Zero` is non-zero and the new object is larger,
 * the new memory is zeroed out. Sizes are given to optimize the zeroing out.
 * The output pointer is guaranteed to be aligned to the new handle's alignment.
 * See `AllocHandleInfo` for more information.
 *
 * Upon failure, the old object is not freed and the function returns `NULL`.
 */
extern void*
AllocReallocH(
	_opaque_ AllocHandle* OldHandle,
	_opaque_ void* Ptr,
	alloc_t OldSize,
	_opaque_ AllocHandle* NewHandle,
	alloc_t NewSize,
	int Zero
	);


/* See `AllocReallocH` and `AllocHandleLockH` for more information.
 */
extern void*
AllocReallocUH(
	_opaque_ AllocHandle* OldHandle,
	_opaque_ void* Ptr,
	alloc_t OldSize,
	_opaque_ AllocHandle* NewHandle,
	alloc_t NewSize,
	int Zero
	);


#ifdef __cplusplus
}
#endif
