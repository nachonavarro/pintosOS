#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

#define P 17
#define Q 14
#define F (1 << 14)

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
