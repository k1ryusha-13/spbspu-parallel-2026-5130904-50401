#include "cli.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <sys/wait.h>
#include <unistd.h>
#include "calc.hpp"

namespace lukashevich
{
  namespace detail
  {
    void require(bool condition)
    {
      if (!condition)
      {
        throw std::invalid_argument("Invalid command");
      }
    }

    bool isSpace(char value)
    {
      return value == ' ' || value == '\t' || value == '\r' || value == '\v' || value == '\f';
    }

    std::vector< std::string > split(const std::string& line)
    {
      std::vector< std::string > result;
      std::size_t position = 0;
      while (position < line.size())
      {
        if (isSpace(line[position]))
        {
          ++position;
          continue;
        }
        const std::size_t begin = position;
        while (position < line.size() && !isSpace(line[position]))
        {
          ++position;
        }
        result.push_back(line.substr(begin, position - begin));
      }
      return result;
    }

    void checkName(const std::string& name)
    {
      require(!name.empty() && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z')));
    }

    long long integer(const std::string& text)
    {
      const char* const first = text.c_str();
      require(*first == '+' || *first == '-' || (*first >= '0' && *first <= '9'));
      char* end = nullptr;
      errno = 0;
      const long long result = std::strtoll(first, &end, 10);
      require(end != first && end == first + text.size() && errno != ERANGE);
      return result;
    }

    std::size_t count(const std::string& text, bool positive)
    {
      const long long value = integer(text);
      require(positive ? value > 0 : value >= 0);
      require(static_cast< unsigned long long >(value) <= std::numeric_limits< std::size_t >::max());
      return static_cast< std::size_t >(value);
    }

    double angle(const std::string& text)
    {
      char* end = nullptr;
      errno = 0;
      const double value = std::strtod(text.c_str(), &end);
      require(end != text.c_str() && end == text.c_str() + text.size() && errno != ERANGE && std::isfinite(value));
      return value;
    }

    void printShape(std::ostream& output, const Shape& shape)
    {
      output << shape.radius_x;
      if (!shape.circle)
      {
        output << ' ' << shape.radius_y;
      }
      output << " (" << shape.center_x << ' ' << shape.center_y << ')';
      if (!shape.circle)
      {
        output << ' ' << std::fixed << std::setprecision(3) << shape.angle << std::defaultfloat;
      }
      output << '\n';
    }

    void printBounds(std::ostream& output, const Bounds& bounds)
    {
      output << std::defaultfloat << std::setprecision(std::numeric_limits< double >::max_digits10) << '('
             << bounds.min_x << ' ' << bounds.min_y << ") (" << bounds.max_x << ' ' << bounds.max_y << ")\n";
    }

    void printStatus(std::ostream& output, const Calculation& calculation)
    {
      const Response& result = calculation.result;
      require(result.state != CalculationState::failed);
      output << calculation.process << ' ';
      if (result.state == CalculationState::progress)
      {
        output << "<IN PROGRESS>\n";
      }
      else if (result.state == CalculationState::canceled)
      {
        output << "<CANCELED>\n";
      }
      else
      {
        output << std::fixed << std::setprecision(3) << result.milliseconds << ' ' << result.area << std::defaultfloat
               << '\n';
      }
    }

    void reap(pid_t pid) noexcept
    {
      while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR)
      {
      }
    }
  }
}

lukashevich::CommandLine::~CommandLine()
{
  for (auto& entry : processes_)
  {
    shutdown(entry.second);
  }
}

void lukashevich::CommandLine::run(std::istream& input, std::ostream& output)
{
  std::string line;
  while (std::getline(input, line))
  {
    try
    {
      const auto arguments = detail::split(line);
      if (!arguments.empty())
      {
        execute(arguments, output);
      }
    }
    catch (const std::exception&)
    {
      output << "<INVALID COMMAND>\n";
    }
    output.flush();
  }
}

void lukashevich::CommandLine::addShape(const std::vector< std::string >& arguments, bool circle)
{
  constexpr std::size_t circle_count = 5;
  constexpr std::size_t ellipse_count = 6;
  detail::require(arguments.size() == (circle ? circle_count : ellipse_count));
  const std::string& name = arguments[1];
  detail::checkName(name);
  detail::require(shapes_.count(name) == 0);
  const long long radius_x = detail::integer(arguments[2]);
  const long long radius_y = circle ? radius_x : detail::integer(arguments[3]);
  const std::size_t coordinate = circle ? 3 : 4;
  const long long x = detail::integer(arguments[coordinate]);
  const long long y = detail::integer(arguments[coordinate + 1]);
  detail::require(radius_x > 0 && radius_y > 0);
  shapes_.emplace(name, Shape{radius_x, radius_y, x, y, 0.0, circle});
}

void lukashevich::CommandLine::addSet(const std::vector< std::string >& arguments)
{
  detail::require(arguments.size() >= 3);
  const std::string& name = arguments[1];
  detail::checkName(name);
  detail::require(sets_.count(name) == 0);
  const std::size_t size = detail::count(arguments[2], true);
  detail::require(size == arguments.size() - 3);
  std::vector< std::string > members;
  members.reserve(size);
  for (std::size_t index = 3; index < arguments.size(); ++index)
  {
    detail::require(shapes_.count(arguments[index]) != 0);
    members.push_back(arguments[index]);
  }
  sets_.emplace(name, std::move(members));
}

std::vector< lukashevich::Shape > lukashevich::CommandLine::getSet(const std::string& name) const
{
  const auto& members = sets_.at(name);
  std::vector< Shape > shapes;
  shapes.reserve(members.size());
  for (const std::string& member : members)
  {
    shapes.push_back(shapes_.at(member));
  }
  return shapes;
}

void lukashevich::CommandLine::spawn(const std::string& name, std::size_t seed)
{
  detail::checkName(name);
  detail::require(processes_.count(name) == 0);
  const auto entry = processes_.emplace(name, Process{-1, -1, -1, false});
  int commands[2]{-1, -1};
  int responses[2]{-1, -1};
  try
  {
    if (pipe(commands) < 0 || pipe(responses) < 0)
    {
      throw std::system_error(errno, std::generic_category(), "pipe");
    }
    const pid_t pid = fork();
    if (pid < 0)
    {
      throw std::system_error(errno, std::generic_category(), "fork");
    }
    if (pid == 0)
    {
      closeDescriptor(commands[1]);
      closeDescriptor(responses[0]);
      for (auto& process : processes_)
      {
        closeDescriptor(process.second.input);
        closeDescriptor(process.second.output);
      }
      int code = 0;
      try
      {
        computeProcess(commands[0], responses[1], seed);
      }
      catch (...)
      {
        code = 1;
      }
      closeDescriptor(commands[0]);
      closeDescriptor(responses[1]);
      _exit(code);
    }
    closeDescriptor(commands[0]);
    closeDescriptor(responses[1]);
    entry.first->second = Process{pid, commands[1], responses[0], true};
  }
  catch (...)
  {
    for (int& descriptor : commands)
    {
      closeDescriptor(descriptor);
    }
    for (int& descriptor : responses)
    {
      closeDescriptor(descriptor);
    }
    processes_.erase(entry.first);
    throw;
  }
}

lukashevich::Response lukashevich::CommandLine::exchange(
    Process& process, const Request& request, const std::vector< Shape >& shapes)
{
  detail::require(process.alive);
  try
  {
    sendAll(process.input, &request, sizeof(request));
    if (request.kind == RequestKind::calculate)
    {
      sendAll(process.input, shapes.data(), shapes.size() * sizeof(Shape));
    }
    Response response{};
    if (!receiveAll(process.output, &response, sizeof(response)))
    {
      throw std::runtime_error("Process error");
    }
    return response;
  }
  catch (...)
  {
    shutdown(process);
    throw;
  }
}

void lukashevich::CommandLine::start(const std::vector< std::string >& arguments)
{
  constexpr std::size_t argument_count = 6;
  constexpr std::size_t tries_index = 5;
  detail::require(arguments.size() == argument_count);
  const std::string& name = arguments[2];
  detail::checkName(name);
  detail::require(calculations_.count(name) == 0);
  Process& process = processes_.at(arguments[1]);
  detail::require(process.alive);
  const std::size_t threads = detail::count(arguments[4], false);
  const std::size_t tries = detail::count(arguments[tries_index], true);
  const auto shapes = getSet(arguments[3]);
  const std::size_t id = calculations_.size();
  const auto inserted
      = calculations_.emplace(name, Calculation{arguments[1], id, {CalculationState::progress, 0.0, 0.0}});
  try
  {
    const Request request{RequestKind::calculate, id, threads, tries, shapes.size(), 0};
    const Response response = exchange(process, request, shapes);
    detail::require(response.state == CalculationState::progress);
  }
  catch (...)
  {
    calculations_.erase(inserted.first);
    throw;
  }
}

void lukashevich::CommandLine::query(Calculation& calculation, RequestKind kind, long long milliseconds)
{
  if (calculation.result.state == CalculationState::progress)
  {
    Process& process = processes_.at(calculation.process);
    if (!process.alive)
    {
      calculation.result.state = CalculationState::failed;
      return;
    }
    const Request request{kind, calculation.id, 0, 0, 0, milliseconds};
    calculation.result = exchange(process, request);
  }
}

void lukashevich::CommandLine::stop(const std::string& name)
{
  Process& process = processes_.at(name);
  detail::require(process.alive);
  const Request request{RequestKind::stop, 0, 0, 0, 0, 0};
  exchange(process, request);
  for (auto& entry : calculations_)
  {
    Calculation& calculation = entry.second;
    if (calculation.process == name)
    {
      query(calculation, RequestKind::status, 0);
    }
  }
  shutdown(process);
}

void lukashevich::CommandLine::shutdown(Process& process) noexcept
{
  if (process.pid <= 0)
  {
    return;
  }
  if (process.alive)
  {
    try
    {
      const Request request{RequestKind::shutdown, 0, 0, 0, 0, 0};
      sendAll(process.input, &request, sizeof(request));
    }
    catch (...)
    {}
  }
  closeDescriptor(process.input);
  closeDescriptor(process.output);
  detail::reap(process.pid);
  process.pid = -1;
  process.alive = false;
}

void lukashevich::CommandLine::execute(const std::vector< std::string >& arguments, std::ostream& output)
{
  const std::string& command = arguments[0];
  if (command == "circle" || command == "ellipse")
  {
    addShape(arguments, command == "circle");
  }
  else if (command == "rotate")
  {
    detail::require(arguments.size() == 3);
    Shape& shape = shapes_.at(arguments[1]);
    const double rotation = detail::angle(arguments[2]);
    const double period = 2.0 * std::acos(-1.0);
    if (!shape.circle)
    {
      shape.angle = std::remainder(shape.angle + std::remainder(rotation, period), period);
    }
  }
  else if (command == "set")
  {
    addSet(arguments);
  }
  else if (command == "show" || command == "showset" || command == "frame" || command == "frameset")
  {
    detail::require(arguments.size() == 2);
    if (command == "show")
    {
      detail::printShape(output, shapes_.at(arguments[1]));
    }
    else if (command == "showset")
    {
      for (const Shape& shape : getSet(arguments[1]))
      {
        detail::printShape(output, shape);
      }
    }
    else
    {
      const auto shapes = command == "frame" ? std::vector< Shape >{shapes_.at(arguments[1])} : getSet(arguments[1]);
      detail::printBounds(output, getBounds(shapes));
    }
  }
  else if (command == "spawn")
  {
    detail::require(arguments.size() == 3);
    spawn(arguments[1], detail::count(arguments[2], false));
  }
  else if (command == "area-on")
  {
    start(arguments);
  }
  else if (command == "status" || command == "wait" || command == "timeout")
  {
    detail::require(arguments.size() == (command == "timeout" ? 3 : 2));
    const long long milliseconds = command == "timeout" ? detail::integer(arguments[2]) : 0;
    detail::require(milliseconds >= 0);
    Calculation& calculation = calculations_.at(arguments[1]);
    const RequestKind kind
        = command == "wait" ? RequestKind::wait : (command == "timeout" ? RequestKind::timeout : RequestKind::status);
    query(calculation, kind, milliseconds);
    detail::printStatus(output, calculation);
  }
  else if (command == "kill")
  {
    detail::require(arguments.size() == 2);
    stop(arguments[1]);
  }
  else
  {
    output << "<UNKNOWN COMMAND>\n";
  }
}
