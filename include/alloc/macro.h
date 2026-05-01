/*
 *   Copyright 2024-2026 Franciszek Balcerak
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

#include <alloc/debug.h>

#include <stddef.h>
#include <stdint.h>

#define TYPEOF(x) typeof_unqual(x)

#define MACRO_EMPTY()
#define MACRO_DEFER(id) id MACRO_EMPTY()

#define MACRO_EVAL(...) MACRO_EVAL128(__VA_ARGS__)
#define MACRO_EVAL128(...) MACRO_EVAL64(MACRO_EVAL64(__VA_ARGS__))
#define MACRO_EVAL64(...) MACRO_EVAL32(MACRO_EVAL32(__VA_ARGS__))
#define MACRO_EVAL32(...) MACRO_EVAL16(MACRO_EVAL16(__VA_ARGS__))
#define MACRO_EVAL16(...) MACRO_EVAL8(MACRO_EVAL8(__VA_ARGS__))
#define MACRO_EVAL8(...) MACRO_EVAL4(MACRO_EVAL4(__VA_ARGS__))
#define MACRO_EVAL4(...) MACRO_EVAL2(MACRO_EVAL2(__VA_ARGS__))
#define MACRO_EVAL2(...) MACRO_EVAL1(MACRO_EVAL1(__VA_ARGS__))
#define MACRO_EVAL1(...) __VA_ARGS__

#define MACRO_FOR_EACH_INDIRECT() MACRO_FOR_EACH_I
#define MACRO_FOR_EACH_I(macro, delim, x, ...) macro(x) __VA_OPT__(MACRO_DEFER(delim)() MACRO_DEFER(MACRO_FOR_EACH_INDIRECT)()(macro, delim, __VA_ARGS__))
#define MACRO_FOR_EACH(macro, delim, ...) MACRO_EVAL(MACRO_FOR_EACH_I(macro, delim, __VA_ARGS__))

#define MACRO_TOKEN(x) x
#define MACRO_ONE(x) 1
#define MACRO_COMMA() ,
#define MACRO_PLUS() +

#define MACRO_COUNT(...) (MACRO_FOR_EACH(MACRO_ONE, MACRO_PLUS, __VA_ARGS__))
#define MACRO_ADD(...) (MACRO_FOR_EACH(MACRO_TOKEN, MACRO_PLUS, __VA_ARGS__))
#define MACRO_FORMAT(...) MACRO_FOR_EACH(MACRO_FORMAT_TYPE, MACRO_COMMA, __VA_ARGS__)

/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
#define MACRO_BITS(x) (sizeof(x) << 3)

#define MACRO_STR2(x) #x
#define MACRO_STR(x) MACRO_STR2(x)

#define MACRO_TO_BITS(bytes) ((bytes) << 3)
#define MACRO_TO_BYTES(bits) (((bits) + 7) >> 3)

#define MACRO_ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))

#define MACRO_MIN(a, b)	\
({						\
    TYPEOF(a) _a = a;	\
    TYPEOF(a) _b = b;	\
    _a > _b ? _b : _a;	\
})

#define MACRO_MAX(a, b)	\
({						\
    TYPEOF(a) _a = a;	\
    TYPEOF(a) _b = b;	\
    _a > _b ? _a : _b;	\
})

#define MACRO_CLAMP(a, min, max) MACRO_MIN(MACRO_MAX(a, min), max)
#define MACRO_CLAMP_SYM(a, min_max) MACRO_CLAMP(a, -(min_max), min_max)

#define MACRO_U32_TO_F32(a)	\
({							\
	union					\
	{						\
		float f32;			\
		uint32_t u32;		\
	}						\
	x =						\
	{						\
		.u32 = a			\
	};						\
							\
	x.f32;					\
})

#define MACRO_F32_TO_U32(a)	\
({							\
	union					\
	{						\
		float f32;			\
		uint32_t u32;		\
	}						\
	x =						\
	{						\
		.f32 = a			\
	};						\
							\
	x.u32;					\
})

#define MACRO_FORMAT_TYPE(x)	\
_Generic(x,						\
	bool:				"%d",	\
	char:				"%c",	\
	signed char:		"%hhd",	\
	short:				"%hd",	\
	int:				"%d",	\
	long:				"%ld",	\
	long long:			"%lld",	\
	unsigned char:		"%hhu",	\
	unsigned short:		"%hu",	\
	unsigned int:		"%u",	\
	unsigned long:		"%lu",	\
	unsigned long long:	"%llu",	\
	float:				"%f",	\
	double:				"%lf",	\
	long double:		"%Lf",	\
	char*:				"%s",	\
	const char*:		"%s",	\
	default:			"%p"	\
	)

#define MACRO_FORMAT_TYPE_CONST(x) MACRO_FORMAT_TYPE((x) 0)

#define MACRO_CONTAINER_OF(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

#define MACRO_CHOOSE(cond, true_expr, false_expr) __builtin_choose_expr(cond, true_expr, false_expr)

/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
#define MACRO_32_OR_64_BITS(x) MACRO_CHOOSE(sizeof(x) <= 4, 32, 64)

#if defined(__x86_64__) || defined(__i386__)

	#include <immintrin.h>

	/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
	#define MACRO_CLZ_NOCONST(x) MACRO_CHOOSE(sizeof(x) <= 4, _lzcnt_u32, _lzcnt_u64)(x)
	/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
	#define MACRO_CTZ_NOCONST(x) MACRO_CHOOSE(sizeof(x) <= 4, _tzcnt_u32, _tzcnt_u64)(x)
	/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
	#define MACRO_POPCOUNT_NOCONST(x) MACRO_CHOOSE(sizeof(x) <= 4, _mm_popcnt_u32, _mm_popcnt_u64)(x)

#elif defined(__aarch64__) || defined(__arm__)

	#include <arm_acle.h>

	attr_inline uint32_t _arm_ctz32(uint32_t x)
	{
		return __clz(__rbit(x));
	}

	attr_inline uint64_t _arm_ctz64(uint64_t x)
	{
		return __clzll(__rbitll(x));
	}

	/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
	#define MACRO_CLZ_NOCONST(x) MACRO_CHOOSE(sizeof(x) <= 4, __clz, __clzll)(x)
	/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
	#define MACRO_CTZ_NOCONST(x) MACRO_CHOOSE(sizeof(x) <= 4, _arm_ctz32, _arm_ctz64)(x)
	/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
	#define MACRO_POPCOUNT_NOCONST(x) MACRO_CHOOSE(sizeof(x) <= 4, __builtin_popcount, __builtin_popcountll)(x)

#endif

/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
#define MACRO_CLZ_CONST(x) MACRO_CHOOSE(sizeof(x) <= 4, !(x) ? 32 : __builtin_clz(x), !(x) ? 64 : __builtin_clzll(x))
/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
#define MACRO_CTZ_CONST(x) MACRO_CHOOSE(sizeof(x) <= 4, !(x) ? 32 : __builtin_ctz(x), !(x) ? 64 : __builtin_ctzll(x))
/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
#define MACRO_POPCOUNT_CONST(x) MACRO_CHOOSE(sizeof(x) <= 4, __builtin_popcount, __builtin_popcountll)(x)

#define MACRO_IS_CONST(expr) __builtin_constant_p(expr)
#define MACRO_CHOOSE_MACRO(name, ...)	\
MACRO_CHOOSE(MACRO_IS_CONST(MACRO_ADD(__VA_ARGS__)), name##_CONST(__VA_ARGS__), name##_NOCONST(__VA_ARGS__))

#define MACRO_CLZ(x) MACRO_CHOOSE_MACRO(MACRO_CLZ, x)
#define MACRO_CTZ(x) MACRO_CHOOSE_MACRO(MACRO_CTZ, x)
#define MACRO_POPCOUNT(x) MACRO_CHOOSE_MACRO(MACRO_POPCOUNT, x)

#define MACRO_MAKE_RUNTIME_1(name, arg1, ...)				\
attr_inline uint32_t name##__u32(uint32_t arg1) __VA_ARGS__	\
attr_inline uint64_t name##__u64(uint64_t arg1) __VA_ARGS__

#define MACRO_MAKE_RUNTIME_2(name, arg1, arg2, ...)							\
attr_inline uint32_t name##__u32(uint32_t arg1, uint32_t arg2) __VA_ARGS__	\
attr_inline uint64_t name##__u64(uint64_t arg1, uint64_t arg2) __VA_ARGS__

/* NOLINTNEXTLINE(bugprone-sizeof-expression) */
#define MACRO_RUNTIME(name, ...) MACRO_CHOOSE(sizeof(MACRO_ADD(__VA_ARGS__)) <= 4, name##__u32, name##__u64)(__VA_ARGS__)

#define MACRO_FLOOR_LOG2_COMMON(num, ver) MACRO_32_OR_64_BITS(num) - 1 - MACRO_CLZ_##ver##CONST(num)
#define MACRO_FLOOR_LOG2_CONST(num) (MACRO_FLOOR_LOG2_COMMON(num,))

MACRO_MAKE_RUNTIME_1(MACRO_FLOOR_LOG2, num,
	{
		TYPEOF(num) _fl2_num = num;
		assert_neq(_fl2_num, 0);

		return MACRO_FLOOR_LOG2_COMMON(_fl2_num, NO);
	}
	)

#define MACRO_FLOOR_LOG2_NOCONST(num) (TYPEOF(num)) MACRO_RUNTIME(MACRO_FLOOR_LOG2, num)
#define MACRO_FLOOR_LOG2(num) MACRO_CHOOSE_MACRO(MACRO_FLOOR_LOG2, num)

#define MACRO_CEIL_LOG2_COMMON(num, ver) MACRO_32_OR_64_BITS(num) - MACRO_CLZ_##ver##CONST((num) - 1)
#define MACRO_CEIL_LOG2_CONST(num) (num == 1 ? 0 : MACRO_CEIL_LOG2_COMMON(num,))

MACRO_MAKE_RUNTIME_1(MACRO_CEIL_LOG2, num,
	{
		TYPEOF(num) _cl2_num = num;
		assert_neq(_cl2_num, 0);

		return MACRO_CEIL_LOG2_COMMON(_cl2_num, NO);
	}
	)

#define MACRO_CEIL_LOG2_NOCONST(num) (TYPEOF(num)) MACRO_RUNTIME(MACRO_CEIL_LOG2, num)
#define MACRO_CEIL_LOG2(num) MACRO_CHOOSE_MACRO(MACRO_CEIL_LOG2, num)

#define MACRO_NEXT_OR_EQUAL_POWER_OF_2_COMMON(num, ver) (TYPEOF(num)) 1 << MACRO_CEIL_LOG2_##ver##CONST(num)
#define MACRO_NEXT_OR_EQUAL_POWER_OF_2_CONST(num) (MACRO_NEXT_OR_EQUAL_POWER_OF_2_COMMON(num,))

MACRO_MAKE_RUNTIME_1(MACRO_NEXT_OR_EQUAL_POWER_OF_2, num,
	{
		TYPEOF(num) _npo2_num = num;
		assert_neq(_npo2_num, 0);
		assert_true(MACRO_CEIL_LOG2(_npo2_num) < MACRO_32_OR_64_BITS(_npo2_num));

		return MACRO_NEXT_OR_EQUAL_POWER_OF_2_COMMON(_npo2_num, NO);
	}
	)

#define MACRO_NEXT_OR_EQUAL_POWER_OF_2_NOCONST(num) (TYPEOF(num)) MACRO_RUNTIME(MACRO_NEXT_OR_EQUAL_POWER_OF_2, num)
#define MACRO_NEXT_OR_EQUAL_POWER_OF_2(num) MACRO_CHOOSE_MACRO(MACRO_NEXT_OR_EQUAL_POWER_OF_2, num)

#define MACRO_POWER_OF_2_MASK_COMMON(num, ver) MACRO_NEXT_OR_EQUAL_POWER_OF_2_##ver##CONST(num) - 1
#define MACRO_POWER_OF_2_MASK_CONST(num) (MACRO_POWER_OF_2_MASK_COMMON(num,))

MACRO_MAKE_RUNTIME_1(MACRO_POWER_OF_2_MASK, num,
	{
		TYPEOF(num) _po2m_num = num;
		assert_neq(_po2m_num, 0);

		return MACRO_POWER_OF_2_MASK_COMMON(_po2m_num, NO);
	}
	)

#define MACRO_POWER_OF_2_MASK_NOCONST(num) (TYPEOF(num)) MACRO_RUNTIME(MACRO_POWER_OF_2_MASK, num)
#define MACRO_POWER_OF_2_MASK(num) MACRO_CHOOSE_MACRO(MACRO_POWER_OF_2_MASK, num)

#define MACRO_IS_POWER_OF_2_COMMON(num, ver) ((num) & ((num) - 1)) == 0
#define MACRO_IS_POWER_OF_2_CONST(num) (MACRO_IS_POWER_OF_2_COMMON(num,))

MACRO_MAKE_RUNTIME_1(MACRO_IS_POWER_OF_2, num,
	{
		TYPEOF(num) _num = num;
		assert_neq(_num, 0);

		return MACRO_IS_POWER_OF_2_COMMON(_num, NO);
	}
)

#define MACRO_IS_POWER_OF_2_NOCONST(num) (TYPEOF(num)) MACRO_RUNTIME(MACRO_IS_POWER_OF_2, num)
#define MACRO_IS_POWER_OF_2(num) MACRO_CHOOSE_MACRO(MACRO_IS_POWER_OF_2, num)

#define MACRO_ALIGN_UP_COMMON(num, mask, ver) (TYPEOF(num))(((TYPEOF(mask))(num) + (mask)) & ~(mask))
#define MACRO_ALIGN_UP_CONST(num, mask)	(MACRO_ALIGN_UP_COMMON(num, mask,))

MACRO_MAKE_RUNTIME_2(MACRO_ALIGN_UP, num, mask,
	{
		TYPEOF(mask) _mask = mask;
		assert_true(MACRO_IS_POWER_OF_2(_mask + 1));

		return MACRO_ALIGN_UP_COMMON(num, _mask, NO);
	}
)

#define MACRO_ALIGN_UP_NOCONST(num, mask) (TYPEOF(num)) MACRO_RUNTIME(MACRO_ALIGN_UP, (TYPEOF(mask)) num, mask)
#define MACRO_ALIGN_UP(num, mask) MACRO_CHOOSE_MACRO(MACRO_ALIGN_UP, num, mask)

#define MACRO_ALIGN_DOWN_COMMON(num, mask, ver) (TYPEOF(num))(((TYPEOF(mask))(num)) & ~(mask))
#define MACRO_ALIGN_DOWN_CONST(num, mask) (MACRO_ALIGN_DOWN_COMMON(num, mask,))

MACRO_MAKE_RUNTIME_2(MACRO_ALIGN_DOWN, num, mask,
	{
		TYPEOF(mask) _mask = mask;
		assert_true(MACRO_IS_POWER_OF_2(_mask + 1));

		return MACRO_ALIGN_DOWN_COMMON(num, _mask, NO);
	}
)

#define MACRO_ALIGN_DOWN_NOCONST(num, mask) (TYPEOF(num)) MACRO_RUNTIME(MACRO_ALIGN_DOWN, (TYPEOF(mask)) num, mask)
#define MACRO_ALIGN_DOWN(num, mask) MACRO_CHOOSE_MACRO(MACRO_ALIGN_DOWN, num, mask)

#define MACRO_ALIGN_UP_ANY_COMMON(num, mul, ver) (TYPEOF(num))((TYPEOF(mul))(num) + ((mul) - (TYPEOF(mul))(num) % (mul)) % (mul))
#define MACRO_ALIGN_UP_ANY_CONST(num, mul) (MACRO_ALIGN_UP_ANY_COMMON(num, mul,))

MACRO_MAKE_RUNTIME_2(MACRO_ALIGN_UP_ANY, num, mul,
	{
		TYPEOF(num) _num = num;
		TYPEOF(mul) _mul = mul;
		assert_neq(_mul, 0);

		return MACRO_ALIGN_UP_ANY_COMMON(_num, _mul, NO);
	}
)

#define MACRO_ALIGN_UP_ANY_NOCONST(num, mul) (TYPEOF(num)) MACRO_RUNTIME(MACRO_ALIGN_UP_ANY, (TYPEOF(mul)) num, mul)
#define MACRO_ALIGN_UP_ANY(num, mul) MACRO_CHOOSE_MACRO(MACRO_ALIGN_UP_ANY, num, mul)

#define MACRO_ALIGN_DOWN_ANY_COMMON(num, mul, ver) (TYPEOF(num))(((TYPEOF(mul))(num) / (mul)) * (mul))
#define MACRO_ALIGN_DOWN_ANY_CONST(num, mul) (MACRO_ALIGN_DOWN_ANY_COMMON(num, mul,))

MACRO_MAKE_RUNTIME_2(MACRO_ALIGN_DOWN_ANY, num, mul,
	{
		TYPEOF(mul) _mul = mul;
		assert_neq(_mul, 0);

		return MACRO_ALIGN_DOWN_ANY_COMMON(num, _mul, NO);
	}
)

#define MACRO_ALIGN_DOWN_ANY_NOCONST(num, mul) (TYPEOF(num)) MACRO_RUNTIME(MACRO_ALIGN_DOWN_ANY, (TYPEOF(mul)) num, mul)
#define MACRO_ALIGN_DOWN_ANY(num, mul) MACRO_CHOOSE_MACRO(MACRO_ALIGN_DOWN_ANY, num, mul)

#define MACRO_GET_BITS(num) MACRO_CEIL_LOG2(num)
#define MACRO_GET_MAX_BITS(num) MACRO_GET_BITS(MACRO_NEXT_OR_EQUAL_POWER_OF_2(num))

#define MACRO_ENUM_BITS(name)	\
	name##__COUNT,				\
	name##__BITS = MACRO_GET_BITS(name##__COUNT)

#define MACRO_ENUM_BITS_EXP(name)	\
	name##__COUNT,					\
	name##__BITS = MACRO_GET_MAX_BITS(name##__COUNT)
