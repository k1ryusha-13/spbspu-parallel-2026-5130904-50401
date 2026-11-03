#include <iostream>
#include <signal.h>
#include "cli.hpp"

int main()
{
  struct sigaction action{};
  action.sa_handler = SIG_IGN;
  sigemptyset(&action.sa_mask);
  if (sigaction(SIGPIPE, &action, nullptr) < 0)
  {
    std::cerr << "Unable to ignore SIGPIPE\n";
    return 1;
  }
  lukashevich::CommandLine application;
  application.run(std::cin, std::cout);
  return 0;
}
