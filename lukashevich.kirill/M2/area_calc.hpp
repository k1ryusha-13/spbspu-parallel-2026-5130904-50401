#ifndef AREA_CALC_HPP
#define AREA_CALC_HPP

#include <cstddef>
#include <vector>

namespace lukashevich
{
  struct Shape
  {
    long long radius_x;
    long long radius_y;
    long long center_x;
    long long center_y;
    double angle;
    bool circle;
  };

  struct Bounds
  {
    double min_x;
    double max_x;
    double min_y;
    double max_y;
  };

  bool contains(const Shape& shape, double x, double y);
  Bounds getBounds(const std::vector< Shape >& shapes);
  std::size_t sampleRange(
      const std::vector< Shape >* shapes, const Bounds* bounds, std::size_t tries, std::size_t seed);
  double calculateArea(const std::vector< Shape >& shapes, std::size_t threads, std::size_t tries, std::size_t seed);
}

#endif
