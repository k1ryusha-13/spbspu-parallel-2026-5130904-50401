#include "interprocess.hpp"

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

void lukashevich::sendAll(int descriptor, const void *data, std::size_t size)
{
  const char *current = static_cast< const char * >(data);
  while (size != 0)
  {
    const ssize_t sent = write(descriptor, current, size);
    if (sent < 0 && errno == EINTR)
    {
      continue;
    }
    if (sent <= 0)
    {
      throw std::system_error(errno, std::generic_category(), "write");
    }
    current += sent;
    size -= static_cast< std::size_t >(sent);
  }
}

bool lukashevich::receiveAll(int descriptor, void *data, std::size_t size)
{
  char *current = static_cast< char * >(data);
  const std::size_t expected = size;
  while (size != 0)
  {
    const ssize_t received = read(descriptor, current, size);
    if (received < 0 && errno == EINTR)
    {
      continue;
    }
    if (received < 0)
    {
      throw std::system_error(errno, std::generic_category(), "read");
    }
    if (received == 0)
    {
      if (size != expected)
      {
        throw std::runtime_error("Incomplete message");
      }
      return false;
    }
    current += received;
    size -= static_cast< std::size_t >(received);
  }
  return true;
}

void lukashevich::closeDescriptor(int &descriptor) noexcept
{
  if (descriptor >= 0)
  {
    close(descriptor);
    descriptor = -1;
  }
}
