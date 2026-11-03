#include "calc.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#include "area_calc.hpp"
#include "interprocess.hpp"

namespace lukashevich
{
  namespace detail
  {
    struct Job
    {
      Request request;
      std::vector< Shape > shapes;
    };

    class Calculator
    {
    public:
      explicit Calculator(std::size_t seed):
        seed_(seed),
        stopping_(false),
        worker_(&Calculator::run, this)
      {}

      Calculator(const Calculator &) = delete;
      Calculator &operator=(const Calculator &) = delete;

      ~Calculator()
      {
        finish(false);
      }

      void add(Job job)
      {
        const std::lock_guard< std::mutex > lock(mutex_);
        if (stopping_)
        {
          throw std::invalid_argument("Process is stopped");
        }
        const auto inserted = results_.emplace(job.request.id, Response{CalculationState::progress, 0.0, 0.0});
        try
        {
          jobs_.push_back(std::move(job));
        }
        catch (...)
        {
          results_.erase(inserted.first);
          throw;
        }
        changed_.notify_all();
      }

      Response query(const Request &request)
      {
        std::unique_lock< std::mutex > lock(mutex_);
        Response &result = results_.at(request.id);
        const auto ready = [&result]()
        {
          return result.state != CalculationState::progress;
        };
        if (request.kind == RequestKind::wait)
        {
          changed_.wait(lock, ready);
        }
        else if (request.kind == RequestKind::timeout)
        {
          const auto now = std::chrono::steady_clock::now();
          const auto available = std::chrono::duration_cast< std::chrono::milliseconds >(
              std::chrono::steady_clock::time_point::max() - now)
                                     .count();
          const auto deadline = request.milliseconds >= available
              ? std::chrono::steady_clock::time_point::max()
              : now + std::chrono::milliseconds(request.milliseconds);
          changed_.wait_until(lock, deadline, ready);
        }
        return result;
      }

      void finish(bool cancel)
      {
        {
          const std::lock_guard< std::mutex > lock(mutex_);
          stopping_ = true;
          if (cancel)
          {
            for (const Job &job : jobs_)
            {
              results_.at(job.request.id).state = CalculationState::canceled;
            }
            jobs_.clear();
          }
        }
        changed_.notify_all();
        if (worker_.joinable())
        {
          worker_.join();
        }
      }

    private:
      std::size_t seed_;
      bool stopping_;
      std::mutex mutex_;
      std::condition_variable changed_;
      std::deque< Job > jobs_;
      std::map< std::size_t, Response > results_;
      std::thread worker_;

      void run()
      {
        while (true)
        {
          Job job;
          {
            std::unique_lock< std::mutex > lock(mutex_);
            changed_.wait(lock,
                [this]()
                {
                  return stopping_ || !jobs_.empty();
                });
            if (jobs_.empty())
            {
              return;
            }
            job = std::move(jobs_.front());
            jobs_.pop_front();
          }
          Response result{CalculationState::failed, 0.0, 0.0};
          const auto start = std::chrono::steady_clock::now();
          try
          {
            result.area = calculateArea(job.shapes, job.request.threads, job.request.tries, seed_);
            result.milliseconds
                = std::chrono::duration< double, std::milli >(std::chrono::steady_clock::now() - start).count();
            result.state = CalculationState::done;
          }
          catch (...)
          {}
          {
            const std::lock_guard< std::mutex > lock(mutex_);
            results_.at(job.request.id) = result;
          }
          changed_.notify_all();
        }
      }
    };
  }
}

void lukashevich::computeProcess(int input, int output, std::size_t seed)
{
  detail::Calculator calculator(seed);
  Request request{};
  while (receiveAll(input, &request, sizeof(request)))
  {
    Response response{CalculationState::done, 0.0, 0.0};
    if (request.kind == RequestKind::shutdown)
    {
      break;
    }
    if (request.kind == RequestKind::calculate)
    {
      detail::Job job{request, std::vector< Shape >(request.count)};
      if (!receiveAll(input, job.shapes.data(), job.shapes.size() * sizeof(Shape)))
      {
        throw std::runtime_error("Missing shapes");
      }
      try
      {
        calculator.add(std::move(job));
        response.state = CalculationState::progress;
      }
      catch (...)
      {
        response.state = CalculationState::failed;
      }
    }
    else if (request.kind == RequestKind::stop)
    {
      calculator.finish(true);
    }
    else
    {
      response = calculator.query(request);
    }
    sendAll(output, &response, sizeof(response));
  }
}
