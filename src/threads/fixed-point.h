#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* We are using a P.Q fixed-point format. P + Q must equal 31. */
#define P 17
#define Q 14
#define F (1 << (Q))

typedef int32_t fixed_point;

/* In the following macros, x and y denote fixed-point numbers, and
   n denotes an int. Return type is implied by the macro name. */
#define TO_FIXED_POINT(n) ((n) * F)
#define TO_INT_ROUND_ZERO(x) ((x) / F)
#define TO_INT_ROUND_TO_NEAREST(x) ((x) >= 0 ? ((x) + F / 2) / F   \
                                             : ((x) - F / 2) / F)
#define ADD_FIXED_POINTS(x, y) ((x) + (y))
#define SUB_FIXED_POINTS(x, y) ((x) - (y))
#define ADD_INT_AND_FIXED_POINT(n, x) ((x) + ((n) * F))
#define SUB_INT_FROM_FIXED_POINT(x, n) ((x) - ((n) * F))
#define MUL_FIXED_POINTS(x, y) ((((int64_t) (x)) * (y)) / F)
#define MUL_INT_AND_FIXED_POINT(n, x) ((n) * (x))
#define DIV_FIXED_POINTS(x, y) ((((int64_t) (x)) * F) / (y))
#define DIV_FIXED_POINT_BY_INT(x, n) ((x) / (n))


#endif
