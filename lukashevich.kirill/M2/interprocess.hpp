#ifndef INTERPROCESS_HPP
#define INTERPROCESS_HPP

#include <cstddef>

namespace lukashevich
{
  enum class RequestKind
  {
    calculate,
    status,
    wait,
    timeout,
    stop,
    shutdown
  };
  enum class CalculationState
  {
    progress,
    done,
    canceled,
    failed
  };

  struct Request
  {
    RequestKind kind;
    std::size_t id;
    std::size_t threads;
    std::size_t tries;
    std::size_t count;
    long long milliseconds;
  };

  struct Response
  {
    CalculationState state;
    double milliseconds;
    double area;
  };

  void sendAll(int descriptor, const void * data, std::size_t size);
  bool receiveAll(int descriptor, void * data, std::size_t size);
  void closeDescriptor(int & descriptor) noexcept;
}

#endif
