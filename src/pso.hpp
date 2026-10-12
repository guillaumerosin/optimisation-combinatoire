#pragma once

#include "instance.hpp"
#include "schedule.hpp"

#include <cstdint>
#include <string>

struct SolverOptions {
    double seconds = 55.0;
    std::uint64_t seed = 0;
    int particles = 30;
    int threads = 1;
    bool use_pso = true;
    bool use_local_search = true;
    bool decreasing_inertia = true;
    std::string decoder = "semi";
    double delta = 0.4;
    std::string output_directory = ".";
    std::string log_path;
};

Schedule solve(const Instance& instance, const SolverOptions& options);
