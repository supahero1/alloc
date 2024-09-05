#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <inttypes.h>


__attribute__((noreturn))
extern void
AssertFailed(
	const char* Msg1,
	const char* TypeA,
	const char* Msg2,
	const char* TypeB,
	const char* Msg3,
	...
	);


__attribute__((noreturn))
extern void
UnreachableAssertFailed(
	const char* Msg
	);


extern void
LocationLogger(
	const char* Msg,
	...
	);


#define GetPrintfType(X)		\
_Generic((X),					\
	int8_t:			"%" PRId8,	\
	int16_t:		"%" PRId16,	\
	int32_t:		"%" PRId32,	\
	int64_t:		"%" PRId64,	\
	uint8_t:		"%" PRIu8,	\
	uint16_t:		"%" PRIu16,	\
	uint32_t:		"%" PRIu32,	\
	uint64_t:		"%" PRIu64,	\
	float:			"%f",		\
	double:			"%lf",		\
	long double:	"%Lf",		\
	default:		"%p"		\
)

#define Stringify2(X) #X
#define Stringify(X) Stringify2(X)

#define AssertFail(A, B, Op, ROp)					\
AssertFailed(										\
	"Assertion \"" #A " " Op " " #B "\" failed: '",	\
	GetPrintfType(A),								\
	"' " ROp " '",									\
	GetPrintfType(B),								\
	"', at " __FILE__ ":" Stringify(__LINE__) "\n",	\
	A,												\
	B												\
	)												\

#define HardenedAssertEQ(A, B) if(!__builtin_expect(A == B, 1)) AssertFail(A, B, "==", "!=")
#define HardenedAssertNEQ(A, B) if(!__builtin_expect(A != B, 1)) AssertFail(A, B, "!=", "==")
#define HardenedAssertLT(A, B) if(!__builtin_expect(A < B, 1)) AssertFail(A, B, "<", ">=")
#define HardenedAssertLE(A, B) if(!__builtin_expect(A <= B, 1)) AssertFail(A, B, "<=", ">")
#define HardenedAssertGT(A, B) if(!__builtin_expect(A > B, 1)) AssertFail(A, B, ">", "<=")
#define HardenedAssertGE(A, B) if(!__builtin_expect(A >= B, 1)) AssertFail(A, B, ">=", "<")
#define HardenedAssertUnreachable()	\
UnreachableAssertFailed("Unreachable assertion failed, at " __FILE__ ":" Stringify(__LINE__) "\n")
#define HardenedLogLocation(...) LocationLogger("At " __FILE__ ":" Stringify(__LINE__) __VA_OPT__(":") "\n" __VA_ARGS__)

#define EmptyAssertEQ(A, B) if(!__builtin_expect(A == B, 1)) __builtin_unreachable()
#define EmptyAssertNEQ(A, B) if(!__builtin_expect(A != B, 1)) __builtin_unreachable()
#define EmptyAssertLT(A, B) if(!__builtin_expect(A < B, 1)) __builtin_unreachable()
#define EmptyAssertLE(A, B) if(!__builtin_expect(A <= B, 1)) __builtin_unreachable()
#define EmptyAssertGT(A, B) if(!__builtin_expect(A > B, 1)) __builtin_unreachable()
#define EmptyAssertGE(A, B) if(!__builtin_expect(A >= B, 1)) __builtin_unreachable()
#define EmptyAssertUnreachable() __builtin_unreachable()
#define EmptyLogLocation()

#ifndef NDEBUG
	#define AssertEQ(A, B) HardenedAssertEQ(A, B)
	#define AssertNEQ(A, B) HardenedAssertNEQ(A, B)
	#define AssertLT(A, B) HardenedAssertLT(A, B)
	#define AssertLE(A, B) HardenedAssertLE(A, B)
	#define AssertGT(A, B) HardenedAssertGT(A, B)
	#define AssertGE(A, B) HardenedAssertGE(A, B)
	#define AssertUnreachable() HardenedAssertUnreachable()
	#define LogLocation(...) HardenedLogLocation(__VA_ARGS__)
	#define Private
#else
	#define AssertEQ(A, B) EmptyAssertEQ(A, B)
	#define AssertNEQ(A, B) EmptyAssertNEQ(A, B)
	#define AssertLT(A, B) EmptyAssertLT(A, B)
	#define AssertLE(A, B) EmptyAssertLE(A, B)
	#define AssertGT(A, B) EmptyAssertGT(A, B)
	#define AssertGE(A, B) EmptyAssertGE(A, B)
	#define AssertUnreachable() EmptyAssertUnreachable()
	#define LogLocation(...) EmptyLogLocation()
	#define Private static
#endif

#define Fallthrough() __attribute__((fallthrough))


#ifdef __cplusplus
}
#endif
