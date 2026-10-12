#pragma once

#include "instance.hpp"
#include "schedule.hpp"

#include <chrono>
#include <random>

Schedule tabu_improve(const Instance& instance, const Schedule& initial,
                      std::mt19937_64& random,
                      std::chrono::steady_clock::time_point deadline,
                      int max_iterations = 80);
