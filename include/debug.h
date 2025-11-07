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

#include "macro.h"


__attribute__((noreturn))
extern void
assert_failed(
	const char* msg1,
	const char* type1,
	const char* msg2,
	const char* type2,
	const char* msg3,
	...
	);


__attribute__((noreturn))
extern void
unreachable_assert_failed(
	const char* msg
	);


extern void
location_logger(
	const char* msg,
	...
	);


#define ASSERT_NULL ((const volatile void*) 0)

#define hard_assert_base(a, b, Op, ROp, ...)	\
do												\
{												\
	typeof(b) _a = (a);							\
	typeof(b) _b = (b);							\
												\
	if(!__builtin_expect(_a Op _b, 1))			\
	{											\
		__VA_ARGS__ __VA_OPT__(;)				\
												\
		assert_fail(_a, _b, #a, #b, #Op, #ROp);	\
	}											\
}												\
while(0)
#define hard_assert_eq(a, b, ...) hard_assert_base(a, b, ==, != __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_neq(a, b, ...) hard_assert_base(a, b, !=, == __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_true(a, ...) hard_assert_eq(a, true __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_false(a, ...) hard_assert_eq(a, false __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_null(a, ...) hard_assert_eq(a, ASSERT_NULL __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_not_null(a, ...) hard_assert_neq(a, ASSERT_NULL __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_ptr(ptr, size, ...)				\
hard_assert_not_null(ptr,							\
	{												\
		bool is_zero = sizeof(*ptr) * size == 0;	\
		if(__builtin_expect(is_zero, 1)) break;		\
		__VA_ARGS__ __VA_OPT__(;)					\
	}												\
	)
#define hard_assert_lt(a, b, ...) hard_assert_base(a, b, <, >= __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_le(a, b, ...) hard_assert_base(a, b, <=, > __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_gt(a, b, ...) hard_assert_base(a, b, >, <= __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_ge(a, b, ...) hard_assert_base(a, b, >=, < __VA_OPT__(,) __VA_ARGS__)
#define hard_assert_unreachable(...)			\
do												\
{												\
	__VA_ARGS__ __VA_OPT__(;)					\
												\
	unreachable_assert_failed(					\
		"Unreachable assertion failed, at "		\
		__FILE__ ":" MACRO_STR(__LINE__) "\n");	\
}												\
while(0)
#define hard_assert_log(...)	\
location_logger("at " __FILE__ ":" MACRO_STR(__LINE__) __VA_OPT__(":") "\n" __VA_ARGS__)

#define empty_assert_base(a, b, Op, ...)	\
do											\
{											\
	typeof(b) _a = (a);						\
	typeof(b) _b = (b);						\
											\
	if(!__builtin_expect(_a Op _b, 1))		\
	{										\
		__VA_ARGS__ __VA_OPT__(;)			\
											\
		__builtin_unreachable();			\
	}										\
}											\
while(0)
#define empty_assert_eq(a, b, ...) empty_assert_base(a, b, ==)
#define empty_assert_neq(a, b, ...) empty_assert_base(a, b, !=)
#define empty_assert_true(a, ...) empty_assert_eq(a, true)
#define empty_assert_false(a, ...) empty_assert_eq(a, false)
#define empty_assert_null(a, ...) empty_assert_eq(a, ASSERT_NULL)
#define empty_assert_not_null(a, ...) empty_assert_neq(a, ASSERT_NULL)
#define empty_assert_ptr(ptr, size, ...)			\
empty_assert_base(ptr, ASSERT_NULL, !=,				\
	{												\
		bool is_zero = sizeof(*ptr) * size == 0;	\
		if(__builtin_expect(is_zero, 1)) break;		\
	}												\
	)
#define empty_assert_lt(a, b, ...) empty_assert_base(a, b, <)
#define empty_assert_le(a, b, ...) empty_assert_base(a, b, <=)
#define empty_assert_gt(a, b, ...) empty_assert_base(a, b, >)
#define empty_assert_ge(a, b, ...) empty_assert_base(a, b, >=)
#define empty_assert_unreachable() __builtin_unreachable()
#define empty_assert_log()

#define assert_fail_base(a, b, Op, ROp, assert_str)	\
assert_failed(										\
	"Assertion \"" assert_str "\" failed: '",		\
	MACRO_FORMAT_TYPE(a),							\
	"' " ROp " '",									\
	MACRO_FORMAT_TYPE(b),							\
	"', at " __FILE__ ":" MACRO_STR(__LINE__) "\n",	\
	a,												\
	b												\
	)

#ifndef NDEBUG
	#define assert_fail(a, b, a_str, b_str, Op, ROp)	\
	assert_fail_base(a, b, Op, ROp, a_str " " Op " " b_str)

	#define assert_eq(...) hard_assert_eq(__VA_ARGS__)
	#define assert_neq(...) hard_assert_neq(__VA_ARGS__)
	#define assert_true(...) hard_assert_true(__VA_ARGS__)
	#define assert_false(...) hard_assert_false(__VA_ARGS__)
	#define assert_null(...) hard_assert_null(__VA_ARGS__)
	#define assert_not_null(...) hard_assert_not_null(__VA_ARGS__)
	#define assert_ptr(...) hard_assert_ptr(__VA_ARGS__)
	#define assert_lt(...) hard_assert_lt(__VA_ARGS__)
	#define assert_le(...) hard_assert_le(__VA_ARGS__)
	#define assert_gt(...) hard_assert_gt(__VA_ARGS__)
	#define assert_ge(...) hard_assert_ge(__VA_ARGS__)
	#define assert_unreachable(...) hard_assert_unreachable(__VA_ARGS__)
	#define assert_log(...) hard_assert_log(__VA_ARGS__)
	#define private
#else
	#define assert_fail(a, b, a_str, b_str, Op, ROp)	\
	assert_fail_base(a, b, Op, ROp, "(anonymous)")

	#define assert_eq(...) empty_assert_eq(__VA_ARGS__)
	#define assert_neq(...) empty_assert_neq(__VA_ARGS__)
	#define assert_true(...) empty_assert_true(__VA_ARGS__)
	#define assert_false(...) empty_assert_false(__VA_ARGS__)
	#define assert_null(...) empty_assert_null(__VA_ARGS__)
	#define assert_not_null(...) empty_assert_not_null(__VA_ARGS__)
	#define assert_ptr(...) empty_assert_ptr(__VA_ARGS__)
	#define assert_lt(...) empty_assert_lt(__VA_ARGS__)
	#define assert_le(...) empty_assert_le(__VA_ARGS__)
	#define assert_gt(...) empty_assert_gt(__VA_ARGS__)
	#define assert_ge(...) empty_assert_ge(__VA_ARGS__)
	#define assert_unreachable(...) empty_assert_unreachable()
	#define assert_log(...) empty_assert_log()
	#define private static
#endif

#define assert_attr(...) __attribute__((__VA_ARGS__))
#define assert_fallthrough() assert_attr(fallthrough)
#define assert_ctor assert_attr(constructor)
#define assert_dtor assert_attr(destructor)
#define assert_used assert_attr(used)
#define assert_packed assert_attr(packed)


#ifdef __cplusplus
}
#endif
