#include "threads/fixed-point.h"

fixed_point TO_FIXED_POINT(int n)
{
  return n * F;
}

fixed_point TO_INT_ROUND_ZERO(fixed_point x)
{
  return x / F;
}

fixed_point TO_INT_ROUND_TO_NEAREST(fixed_point x)
{
  return (x >= 0) ? (x + F / 2) / F : (x - F / 2) / F;
}

fixed_point ADD_FIXED_POINTS(fixed_point x, fixed_point y)
{
  return x + y;
}

fixed_point SUB_FIXED_POINTS(fixed_point x, fixed_point y)
{
  return x - y;
}

fixed_point ADD_INT_AND_FIXED_POINT(int n, fixed_point x)
{
  return x + (n * F);
}

fixed_point SUB_INT_FROM_FIXED_POINT(fixed_point x, int n)
{
  return x - (n * F);
}

fixed_point MUL_FIXED_POINTS(fixed_point x, fixed_point y)
{
  return (((int64_t) x) * y) / F;
}

fixed_point MUL_INT_AND_FIXED_POINT(fixed_point x, int n)
{
  return x * n;
}

fixed_point DIV_FIXED_POINTS(fixed_point x, fixed_point y)
{
  return (((int64_t) x) * F) / y;
}

fixed_point DIV_FIXED_POINT_BY_INT(fixed_point x, int n)
{
  return x / n;
}

