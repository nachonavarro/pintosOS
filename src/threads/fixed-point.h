#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* We are using a P.Q fixed-point format. */
#define P 17
#define Q (31 - P)
#define F (1 << Q)

/* In the following macros, x and y denote fixed-point numbers, and
   n denotes an int. */
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

typedef int32_t fixed_point;

fixed_point to_fixed_point(int n);
int to_int_round_zero(fixed_point x);
int to_int_round_to_nearest(fixed_point x);
fixed_point add_fixed_points(fixed_point x, fixed_point y);
fixed_point sub_fixed_points(fixed_point x, fixed_point y);
fixed_point add_int_and_fixed_point(int n, fixed_point x);
fixed_point sub_int_from_fixed_point(fixed_point x, int n);
fixed_point mul_fixed_points(fixed_point x, fixed_point y);
fixed_point mul_int_and_fixed_point(fixed_point x, int n);
fixed_point div_fixed_points(fixed_point x, fixed_point y);
fixed_point div_fixed_point_by_int(fixed_point x, int n);

#endif
