#include "area_calc.hpp"

#include <algorithm>
#include <cmath>
#include <future>
#include <random>
#include <stdexcept>

bool lukashevich::contains(const Shape& shape, double x, double y)
{
  const double delta_x = x - shape.center_x;
  const double delta_y = y - shape.center_y;
  if (shape.circle)
  {
    const double radius = static_cast< double >(shape.radius_x);
    return delta_x * delta_x + delta_y * delta_y <= radius * radius;
  }
  const double cosine = std::cos(shape.angle);
  const double sine = std::sin(shape.angle);
  const double local_x = (delta_x * cosine + delta_y * sine) / shape.radius_x;
  const double local_y = (delta_y * cosine - delta_x * sine) / shape.radius_y;
  return local_x * local_x + local_y * local_y <= 1.0;
}

lukashevich::Bounds lukashevich::getBounds(const std::vector< Shape >& shapes)
{
  if (shapes.empty())
  {
    return {0.0, 0.0, 0.0, 0.0};
  }
  Bounds bounds{};
  bool first = true;
  for (const Shape& shape : shapes)
  {
    const double cosine = std::cos(shape.angle);
    const double sine = std::sin(shape.angle);
    const double half_width = std::hypot(shape.radius_x * cosine, shape.radius_y * sine);
    const double half_height = std::hypot(shape.radius_x * sine, shape.radius_y * cosine);
    const double x = static_cast< double >(shape.center_x);
    const double y = static_cast< double >(shape.center_y);
    const Bounds current{x - half_width, x + half_width, y - half_height, y + half_height};
    if (first)
    {
      bounds = current;
      first = false;
    }
    else
    {
      bounds.min_x = std::min(bounds.min_x, current.min_x);
      bounds.max_x = std::max(bounds.max_x, current.max_x);
      bounds.min_y = std::min(bounds.min_y, current.min_y);
      bounds.max_y = std::max(bounds.max_y, current.max_y);
    }
  }
  return bounds;
}

std::size_t lukashevich::sampleRange(
    const std::vector< Shape >* shapes, const Bounds* bounds, std::size_t tries, std::size_t seed)
{
  std::minstd_rand generator(seed % std::minstd_rand::max() + 1);
  const double generator_range = static_cast< double >(generator.max()) - static_cast< double >(generator.min()) + 1.0;
  const double x_scale = (bounds->max_x - bounds->min_x) / generator_range;
  const double y_scale = (bounds->max_y - bounds->min_y) / generator_range;
  const Shape* const first = shapes->data();
  const Shape* const last = first + shapes->size();
  std::size_t covered = 0;
  for (std::size_t index = 0; index < tries; ++index)
  {
    const double x = bounds->min_x + static_cast< double >(generator() - generator.min()) * x_scale;
    const double y = bounds->min_y + static_cast< double >(generator() - generator.min()) * y_scale;
    for (const Shape* shape = first; shape != last; ++shape)
    {
      if (contains(*shape, x, y))
      {
        ++covered;
        break;
      }
    }
  }
  return covered;
}

double lukashevich::calculateArea(
    const std::vector< Shape >& shapes, std::size_t threads, std::size_t tries, std::size_t seed)
{
  if (tries == 0)
  {
    throw std::invalid_argument("Tries must be positive");
  }
  if (shapes.empty())
  {
    return 0.0;
  }
  threads = std::min(threads == 0 ? std::size_t{1} : threads, tries);
  const Bounds bounds = getBounds(shapes);
  const std::size_t tries_per_task = tries / threads;
  const std::size_t remaining_tries = tries % threads;
  std::vector< std::future< std::size_t > > tasks;
  tasks.reserve(threads);
  for (std::size_t index = 0; index < threads; ++index)
  {
    const std::size_t task_tries = tries_per_task + (index < remaining_tries ? 1 : 0);
    tasks.emplace_back(std::async(std::launch::async, sampleRange, &shapes, &bounds, task_tries, seed + index));
  }
  std::size_t covered = 0;
  for (std::future< std::size_t >& task : tasks)
  {
    covered += task.get();
  }
  const double rectangle_area = (bounds.max_x - bounds.min_x) * (bounds.max_y - bounds.min_y);
  return rectangle_area * static_cast< double >(covered) / static_cast< double >(tries);
}
