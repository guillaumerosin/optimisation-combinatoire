#include "schedule.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

Schedule decode_semi_active(const Instance& instance,
                            const std::vector<int>& sequence) {
    const std::size_t expected =
        static_cast<std::size_t>(instance.jobs * instance.machines);
    if (sequence.size() != expected) {
        throw std::runtime_error("Sequence de taille invalide");
    }
    Schedule result;
    result.jobs.assign(static_cast<std::size_t>(instance.jobs),
                       std::vector<ScheduledOperation>());
    std::vector<int> next(instance.jobs, 0);
    std::vector<int> job_ready(instance.jobs, 0);
    std::vector<int> machine_ready(instance.machines, 0);
    for (int job : sequence) {
        if (job < 0 || job >= instance.jobs || next[job] >= instance.machines) {
            throw std::runtime_error("Sequence invalide");
        }
        const int operation = next[job]++;
        const auto& task = instance.operations[job][operation];
        const int start = std::max(job_ready[job], machine_ready[task.machine]);
        const int end = start + task.duration;
        result.jobs[job].push_back({task.machine, start, end});
        job_ready[job] = end;
        machine_ready[task.machine] = end;
        result.makespan = std::max(result.makespan, end);
    }
    return result;
}

void write_schedule_json(const Schedule& schedule, const Instance& instance,
                         const std::string& path) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Impossible d'ecrire le resultat: " + path);
    }
    output << "{\n  \"makespan\": " << schedule.makespan
           << ",\n  \"jobs_schedule\": {\n";
    for (int job = 0; job < instance.jobs; ++job) {
        output << "    \"Job_" << job << "\": [";
        const auto& operations = schedule.jobs.at(static_cast<std::size_t>(job));
        for (std::size_t operation = 0; operation < operations.size();
             ++operation) {
            const auto& value = operations[operation];
            if (operation != 0) {
                output << ", ";
            }
            output << "{\"machine\": " << value.machine
                   << ", \"start\": " << value.start
                   << ", \"end\": " << value.end << "}";
        }
        output << "]" << (job + 1 == instance.jobs ? "\n" : ",\n");
    }
    output << "  }\n}\n";
}
