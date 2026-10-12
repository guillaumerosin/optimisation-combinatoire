#include "schedule.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
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

Schedule decode_giffler_thompson(const Instance& instance,
                                 const std::vector<int>& sequence,
                                 double delta) {
    if (sequence.size() !=
        static_cast<std::size_t>(instance.jobs * instance.machines)) {
        throw std::runtime_error("Sequence de taille invalide");
    }
    delta = std::clamp(delta, 0.0, 1.0);
    Schedule result;
    result.jobs.assign(static_cast<std::size_t>(instance.jobs), {});
    std::vector<int> next(instance.jobs, 0);
    std::vector<int> job_ready(instance.jobs, 0);
    std::vector<std::vector<std::pair<int, int>>> machine_intervals(
        static_cast<std::size_t>(instance.machines));
    std::vector<bool> used(sequence.size(), false);
    for (std::size_t scheduled = 0; scheduled < sequence.size(); ++scheduled) {
        int selected_job = -1;
        int selected_end = std::numeric_limits<int>::max();
        std::size_t selected_rank = sequence.size();
        for (std::size_t rank = 0; rank < sequence.size(); ++rank) {
            if (used[rank]) continue;
            const int job = sequence[rank];
            if (next[job] >= instance.machines) continue;
            const auto& operation = instance.operations[job][next[job]];
            const int completion = job_ready[job] + operation.duration;
            if (selected_job < 0 || completion < selected_end) {
                selected_job = job;
                selected_end = completion;
                selected_rank = rank;
            }
        }
        if (selected_job < 0) throw std::runtime_error("Sequence invalide");
        const int threshold = static_cast<int>(selected_end * delta);
        for (std::size_t rank = 0; rank < sequence.size(); ++rank) {
            if (used[rank]) continue;
            const int job = sequence[rank];
            if (next[job] >= instance.machines) continue;
            const auto& operation = instance.operations[job][next[job]];
            if (rank <= selected_rank &&
                job_ready[job] + operation.duration <= selected_end + threshold) {
                selected_job = job;
                selected_rank = rank;
                break;
            }
        }
        used[selected_rank] = true;
        const int operation_index = next[selected_job]++;
        const auto& operation = instance.operations[selected_job][operation_index];
        int start = job_ready[selected_job];
        auto& intervals = machine_intervals[operation.machine];
        std::sort(intervals.begin(), intervals.end());
        for (const auto& interval : intervals) {
            if (start + operation.duration <= interval.first) break;
            start = std::max(start, interval.second);
        }
        const int end = start + operation.duration;
        result.jobs[selected_job].push_back({operation.machine, start, end});
        intervals.emplace_back(start, end);
        job_ready[selected_job] = end;
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
