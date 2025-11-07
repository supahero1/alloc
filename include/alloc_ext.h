/*
 *   Copyright 2024-2025 Franciszek Balcerak
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

#include "alloc.h"

#ifndef _inline_
	#define _inline_ __attribute__((always_inline)) inline
#endif


_inline_ _pure_func_ _opaque_ alloc_handle_t*
alloc_get_handle(
	alloc_t size
	)
{
	return alloc_get_handle_s(alloc_get_global_state(), size);
}


_inline_ void
alloc_handle_lock_s(
	_in_ alloc_state_t* state,
	alloc_t size
	)
{
	alloc_handle_lock_h(alloc_get_handle_s(state, size));
}


_inline_ void
alloc_handle_lock(
	alloc_t size
	)
{
	alloc_handle_lock_h(alloc_get_handle(size));
}


_inline_ void
alloc_handle_unlock_s(
	_in_ alloc_state_t* state,
	alloc_t size
	)
{
	alloc_handle_unlock_h(alloc_get_handle_s(state, size));
}


_inline_ void
alloc_handle_unlock(
	alloc_t size
	)
{
	alloc_handle_unlock_h(alloc_get_handle(size));
}


_inline_ void
alloc_handle_set_flags_s(
	_in_ alloc_state_t* state,
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_set_flags_h(alloc_get_handle_s(state, size), flags);
}


_inline_ void
alloc_handle_set_flags_us(
	_in_ alloc_state_t* state,
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_set_flags_uh(alloc_get_handle_s(state, size), flags);
}


_inline_ void
alloc_handle_set_flags(
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_set_flags_h(alloc_get_handle(size), flags);
}


_inline_ void
alloc_handle_set_flags_u(
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_set_flags_uh(alloc_get_handle(size), flags);
}


_inline_ void
alloc_handle_add_flags_s(
	_in_ alloc_state_t* state,
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_add_flags_h(alloc_get_handle_s(state, size), flags);
}


_inline_ void
alloc_handle_add_flags_us(
	_in_ alloc_state_t* state,
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_add_flags_uh(alloc_get_handle_s(state, size), flags);
}


_inline_ void
alloc_handle_add_flags(
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_add_flags_h(alloc_get_handle(size), flags);
}


_inline_ void
alloc_handle_add_flags_u(
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_add_flags_uh(alloc_get_handle(size), flags);
}


_inline_ void
alloc_handle_del_flags_s(
	_in_ alloc_state_t* state,
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_del_flags_h(alloc_get_handle_s(state, size), flags);
}


_inline_ void
alloc_handle_del_flags_us(
	_in_ alloc_state_t* state,
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_del_flags_uh(alloc_get_handle_s(state, size), flags);
}


_inline_ void
alloc_handle_del_flags(
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_del_flags_h(alloc_get_handle(size), flags);
}


_inline_ void
alloc_handle_del_flags_u(
	alloc_t size,
	alloc_handle_flag_t flags
	)
{
	alloc_handle_del_flags_uh(alloc_get_handle(size), flags);
}


_inline_ alloc_handle_flag_t
alloc_handle_get_flags_s(
	_in_ alloc_state_t* state,
	alloc_t size
	)
{
	return alloc_handle_get_flags_h(alloc_get_handle_s(state, size));
}


_inline_ alloc_handle_flag_t
alloc_handle_get_flags_us(
	_in_ alloc_state_t* state,
	alloc_t size
	)
{
	return alloc_handle_get_flags_uh(alloc_get_handle_s(state, size));
}


_inline_ alloc_handle_flag_t
alloc_handle_get_flags(
	alloc_t size
	)
{
	return alloc_handle_get_flags_h(alloc_get_handle(size));
}


_inline_ alloc_handle_flag_t
alloc_handle_get_flags_u(
	alloc_t size
	)
{
	return alloc_handle_get_flags_uh(alloc_get_handle(size));
}


#define alloc_alloc_s(state, ptr, size, zero)						\
({																	\
	__typeof__(state) _state = (state);								\
	alloc_t _size = sizeof(*ptr) * (size);							\
	int _zero = (zero);												\
																	\
	alloc_alloc_h(alloc_get_handle_s(_state, _size), _size, _zero);	\
})


#define alloc_alloc_us(state, ptr, size, zero)							\
({																		\
	__typeof__(state) _state = (state);									\
	alloc_t _size = sizeof(*ptr) * (size);								\
	int _zero = (zero);													\
																		\
	alloc_alloc_uh(alloc_get_handle_s(_state, _size), _size, _zero);	\
})


#define alloc_alloc(ptr, size, zero)						\
({															\
	alloc_t _size = sizeof(*ptr) * (size);					\
	int _zero = (zero);										\
															\
	alloc_alloc_h(alloc_get_handle(_size), _size, _zero);	\
})


#define alloc_alloc_u(ptr, size, zero)							\
({																\
	alloc_t _size = sizeof(*ptr) * (size);						\
	int _zero = (zero);											\
																\
	alloc_alloc_uh(alloc_get_handle_u(_size), _size, _zero);	\
})


#define alloc_free_s(state, ptr, size)								\
({																	\
	__typeof__(state) _state = (state);								\
	__typeof__(ptr) _ptr = (ptr);									\
	alloc_t _size = sizeof(*_ptr) * (size);							\
																	\
	alloc_free_h(alloc_get_handle_s(_state, _size), _ptr, _size);	\
})


#define alloc_free_us(state, ptr, size)								\
({																	\
	__typeof__(state) _state = (state);								\
	__typeof__(ptr) _ptr = (ptr);									\
	alloc_t _size = sizeof(*_ptr) * (size);							\
																	\
	alloc_free_uh(alloc_get_handle_s(_state, _size), _ptr, _size);	\
})


#define alloc_free(ptr, size)							\
({														\
	__typeof__(ptr) _ptr = (ptr);						\
	alloc_t _size = sizeof(*_ptr) * (size);				\
														\
	alloc_free_h(alloc_get_handle(_size), _ptr, _size);	\
})


#define alloc_free_u(ptr, size)								\
({															\
	__typeof__(ptr) _ptr = (ptr);							\
	alloc_t _size = sizeof(*_ptr) * (size);					\
															\
	alloc_free_uh(alloc_get_handle_u(_size), _ptr, _size);	\
})


#define alloc_realloc_s(old_state, ptr, old_size, new_state, new_size, zero)	\
({																				\
	__typeof__(old_state) _old_state = (old_state);								\
	__typeof__(ptr) _ptr = (ptr);												\
	alloc_t _old_size = sizeof(*_ptr) * (old_size);								\
	__typeof__(new_state) _new_state = (new_state);								\
	alloc_t _new_size = sizeof(*_ptr) * (new_size);								\
	int _zero = (zero);															\
																				\
	alloc_realloc_h(															\
		alloc_get_handle_s(_old_state, _old_size),								\
		_ptr,																	\
		_old_size,																\
		alloc_get_handle_s(_new_state, _new_size),								\
		_new_size,																\
		_zero																	\
	);																			\
})


#define alloc_realloc_us(old_state, ptr, old_size, new_state, new_size, zero)	\
({																				\
	__typeof__(old_state) _old_state = (old_state);								\
	__typeof__(ptr) _ptr = (ptr);												\
	alloc_t _old_size = sizeof(*_ptr) * (old_size);								\
	__typeof__(new_state) _new_state = (new_state);								\
	alloc_t _new_size = sizeof(*_ptr) * (new_size);								\
	int _zero = (zero);															\
																				\
	allow_realloc_uh(															\
		alloc_get_handle_s(_old_state, _old_size),								\
		_ptr,																	\
		_old_size,																\
		alloc_get_handle_s(_new_state, _new_size),								\
		_new_size,																\
		_zero																	\
	);																			\
})


#define alloc_realloc(ptr, old_size, new_size, zero)	\
({														\
	__typeof__(ptr) _ptr = (ptr);						\
	alloc_t _old_size = sizeof(*_ptr) * (old_size);		\
	alloc_t _new_size = sizeof(*_ptr) * (new_size);		\
	int _zero = (zero);									\
														\
	alloc_realloc_h(									\
		alloc_get_handle(_old_size),					\
		_ptr,											\
		_old_size,										\
		alloc_get_handle(_new_size),					\
		_new_size,										\
		_zero											\
	);													\
})


#define alloc_realloc_u(ptr, old_size, new_size, zero)	\
({														\
	__typeof__(ptr) _ptr = (ptr);						\
	alloc_t _old_size = sizeof(*_ptr) * (old_size);		\
	alloc_t _new_size = sizeof(*_ptr) * (new_size);		\
	int _zero = (zero);									\
														\
	allow_realloc_uh(									\
		alloc_get_handle_u(_old_size),					\
		_ptr,											\
		_old_size,										\
		alloc_get_handle_u(_new_size),					\
		_new_size,										\
		_zero											\
	);													\
})


#define alloc_malloc_s(state, ptr, size)	\
alloc_alloc_s(state, ptr, size, 0)


#define alloc_malloc_us(state, ptr, size)	\
alloc_alloc_us(state, ptr, size, 0)


#define alloc_malloc(ptr, size)	\
alloc_alloc(ptr, size, 0)


#define alloc_malloc_u(ptr, size)	\
alloc_alloc_u(ptr, size, 0)


#define alloc_calloc_s(state, ptr, size)	\
alloc_alloc_s(state, ptr, size, 1)


#define alloc_calloc_us(state, ptr, size)	\
alloc_alloc_us(state, ptr, size, 1)


#define alloc_calloc(ptr, size)	\
alloc_alloc(ptr, size, 1)


#define alloc_calloc_u(ptr, size)	\
alloc_alloc_u(ptr, size, 1)


#define alloc_remalloc_s(old_state, ptr, old_size, new_state, new_size)	\
alloc_realloc_s(old_state, ptr, old_size, new_state, new_size, 0)


#define alloc_remalloc_us(old_state, ptr, old_size, new_state, new_size)	\
alloc_realloc_us(old_state, ptr, old_size, new_state, new_size, 0)


#define alloc_remalloc(ptr, old_size, new_size)	\
alloc_realloc(ptr, old_size, new_size, 0)


#define alloc_remalloc_u(ptr, old_size, new_size)	\
alloc_realloc_u(ptr, old_size, new_size, 0)


#define alloc_recalloc_s(old_state, ptr, old_size, new_state, new_size)	\
alloc_realloc_s(old_state, ptr, old_size, new_state, new_size, 1)


#define alloc_recalloc_us(old_state, ptr, old_size, new_state, new_size)	\
alloc_realloc_us(old_state, ptr, old_size, new_state, new_size, 1)


#define alloc_recalloc(ptr, old_size, new_size)	\
alloc_realloc(ptr, old_size, new_size, 1)


#define alloc_recalloc_u(ptr, old_size, new_size)	\
alloc_realloc_u(ptr, old_size, new_size, 1)


#ifdef __cplusplus
}
#endif
