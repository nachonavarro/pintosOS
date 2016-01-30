#include "threads/fixed-point.h"

fixed_point to_fixed_point(int n)
{
  return n * F;
}

fixed_point to_int_round_zero(fixed_point x)
{
  return x / F;
}

fixed_point to_int_round_to_nearest(fixed_point x)
{
  return (x >= 0) ? (x + F / 2) / F : (x - F / 2) / F;
}

fixed_point add_fixed_points(fixed_point x, fixed_point y)
{
  return x + y; 
}

fixed_point sub_fixed_points(fixed_point x, fixed_point y)
{
  return x - y;
}

fixed_point add_int_and_fixed_point(int n, fixed_point x)
{
  return x + (n * F);
}

fixed_point sub_int_from_fixed_point(fixed_point x, int n)
{
  return x - (n * F);
}

fixed_point mul_fixed_points(fixed_point x, fixed_point y)
{
  return (((int64_t) x) * y) / F;
}

fixed_point mul_int_and_fixed_point(fixed_point x, int n)
{
  return x * n;
}

fixed_point div_fixed_points(fixed_point x, fixed_point y)
{
  return (((int64_t) x) * F) / y;
}

fixed_point div_fixed_point_by_int(fixed_point x, int n)
{
  return x / n;
}






