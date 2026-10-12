#pragma once

#include "instance.hpp"

#include <string>
#include <vector>

struct ScheduledOperation {
    int machine = 0;
    int start = 0;
    int end = 0;
};

struct Schedule {
    int makespan = 0;
    std::vector<std::vector<ScheduledOperation>> jobs;
};

Schedule decode_semi_active(const Instance& instance,
                            const std::vector<int>& sequence);
Schedule decode_giffler_thompson(const Instance& instance,
                                 const std::vector<int>& sequence,
                                 double delta);
void write_schedule_json(const Schedule& schedule, const Instance& instance,
                         const std::string& path);
