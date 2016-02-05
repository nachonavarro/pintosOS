#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* We are using a P.Q fixed-point format. */
#define P 17
#define Q 14
#define F (1 << (Q))

typedef int32_t fixed_point;

fixed_point TO_FIXED_POINT(int n);
int TO_INT_ROUND_ZERO(fixed_point x);
int TO_INT_ROUND_TO_NEAREST(fixed_point x);
fixed_point ADD_FIXED_POINTS(fixed_point x, fixed_point y);
fixed_point SUB_FIXED_POINTS(fixed_point x, fixed_point y);
fixed_point ADD_INT_AND_FIXED_POINT(int n, fixed_point x);
fixed_point SUB_INT_FROM_FIXED_POINT(fixed_point x, int n);
fixed_point MUL_FIXED_POINTS(fixed_point x, fixed_point y);
fixed_point MUL_INT_AND_FIXED_POINT(fixed_point x, int n);
fixed_point DIV_FIXED_POINTS(fixed_point x, fixed_point y);
fixed_point DIV_FIXED_POINT_BY_INT(fixed_point x, int n);

#endif
