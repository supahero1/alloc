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

#define MACRO_POWER_OF_2(bit) (1U << (bit))

#define MACRO_LOG2(num)					\
({										\
	typeof(num) _num = (num);			\
										\
	if(num <= 1)						\
	{									\
		_num = 0;						\
	}									\
	else								\
	{									\
		_num = __builtin_ctzll(num);	\
	}									\
										\
	_num;								\
})

#define MACRO_LOG2_CONST(num)	\
__builtin_choose_expr((num) <= 1, 0, __builtin_ctzll(num))

#define MACRO_NEXT_OR_EQUAL_POWER_OF_2(num)				\
({														\
	typeof(num) _num = (num);							\
														\
	if(_num > 2)										\
	{													\
		_num = 1U << (32 - __builtin_clz(_num - 1));	\
	}													\
														\
	_num;												\
})

#define MACRO_NEXT_OR_EQUAL_POWER_OF_2_CONST(num)	\
__builtin_choose_expr((num) <= 2, (num), 1U << (32 - __builtin_clz((num) - 1)))

#define MACRO_POWER_OF_2_MASK(num)	\
(MACRO_NEXT_OR_EQUAL_POWER_OF_2(num) - 1)

#define MACRO_POWER_OF_2_MASK_CONST(num)	\
(MACRO_NEXT_OR_EQUAL_POWER_OF_2_CONST(num) - 1)

#define MACRO_IS_POWER_OF_2(x)	\
({								\
	typeof(x) _x = (x);	\
	(_x & (_x - 1)) == 0;		\
})

#define MACRO_GET_BITS(num)						\
({												\
	typeof(num) _num = (num);					\
												\
	if(_num <= 1)								\
	{											\
		_num = 0;								\
	}											\
	else										\
	{											\
		_num = 32 - __builtin_clz(_num - 1);	\
	}											\
												\
	_num;										\
})

#define MACRO_GET_BITS_CONST(num)	\
__builtin_choose_expr((num) <= 1, 0, 32 - __builtin_clz((num) - 1))

#define MACRO_ALIGN_UP(num, mask)				\
({												\
	typeof(mask) _mask = (mask);				\
	typeof(num) result = (typeof(num))(			\
		((typeof(mask)) num + _mask) & ~_mask);	\
	result;										\
})

#define MACRO_ALIGN_UP_CONST(num, mask)	\
((typeof(num)) (((typeof(mask)) num + (mask)) & ~(mask)))

#define MACRO_ALIGN_DOWN(num, mask)		\
({										\
	typeof(mask) _mask = (mask);		\
	typeof(num) result = (typeof(num))(	\
		((typeof(mask)) num) & ~_mask);	\
	result;								\
})

#define MACRO_ALIGN_DOWN_CONST(num, mask)	\
((typeof(num)) (((typeof(mask)) num) & ~(mask)))

#define MACRO_STR2(x) #x
#define MACRO_STR(x) MACRO_STR2(x)

#define MACRO_ENUM_BITS(name)	\
name##__COUNT,					\
name##__BITS = MACRO_GET_BITS_CONST( name##__COUNT )

#define MACRO_ENUM_BITS_EXP(name)	\
name##__COUNT,						\
name##__BITS = MACRO_GET_BITS_CONST(MACRO_NEXT_OR_EQUAL_POWER_OF_2_CONST( name##__COUNT ))

#define MACRO_TO_BITS(bytes) ((bytes) << 3)

#define MACRO_TO_BYTES(bits) (((bits) + 7) >> 3)

#define MACRO_ARRAY_LEN(a) (sizeof(a)/sizeof((a)[0]))

#define MACRO_MIN(a, b)	\
({						\
    typeof(a) _a = (a);	\
    typeof(b) _b = (b);	\
    _a > _b ? _b : _a;	\
})

#define MACRO_MAX(a, b)	\
({						\
    typeof(a) _a = (a);	\
    typeof(b) _b = (b);	\
    _a > _b ? _a : _b;	\
})

#define MACRO_CLAMP(a, min, max) MACRO_MIN(MACRO_MAX((a), (min)), (max))
#define MACRO_CLAMP_SYM(a, min_max) MACRO_CLAMP((a), -(min_max), (min_max))

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
_Generic((x),					\
	bool:				"%d",	\
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
	default:			"%p"	\
	)

#define MACRO_FORMAT_TYPE_CONST(x)	\
MACRO_FORMAT_TYPE((x) 0)

#define MACRO_CONTAINER_OF(ptr, type, member)	\
((type*)((char*)(ptr) - offsetof(type, member)))


#ifdef __cplusplus
}
#endif
