#pragma once

#include "instance.hpp"
#include "schedule.hpp"

#include <chrono>
#include <random>

// Ameliore un planning par mouvements N5 sur son chemin critique.
Schedule tabu_improve(const Instance& instance, const Schedule& initial,
                      std::mt19937_64& random,
                      std::chrono::steady_clock::time_point deadline,
                      int max_iterations = 80);
