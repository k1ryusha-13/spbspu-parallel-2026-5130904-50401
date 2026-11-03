#ifndef CLI_HPP
#define CLI_HPP

#include <cstddef>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>
#include <sys/types.h>
#include "area_calc.hpp"
#include "interprocess.hpp"

namespace lukashevich
{
  struct Process
  {
    pid_t pid;
    int input;
    int output;
    bool alive;
  };

  struct Calculation
  {
    std::string process;
    std::size_t id;
    Response result;
  };

  class CommandLine
  {
  public:
    CommandLine() = default;
    CommandLine(const CommandLine&) = delete;
    CommandLine& operator=(const CommandLine&) = delete;
    ~CommandLine();
    void run(std::istream& input, std::ostream& output);

  private:
    std::map< std::string, Shape > shapes_;
    std::map< std::string, std::vector< std::string > > sets_;
    std::map< std::string, Process > processes_;
    std::map< std::string, Calculation > calculations_;

    void execute(const std::vector< std::string >& arguments, std::ostream& output);
    void addShape(const std::vector< std::string >& arguments, bool circle);
    void addSet(const std::vector< std::string >& arguments);
    std::vector< Shape > getSet(const std::string& name) const;
    void spawn(const std::string& name, std::size_t seed);
    void start(const std::vector< std::string >& arguments);
    Response exchange(Process& process, const Request& request, const std::vector< Shape >& shapes = {});
    void query(Calculation& calculation, RequestKind kind, long long milliseconds);
    void stop(const std::string& name);
    void shutdown(Process& process) noexcept;
  };
}

#endif
