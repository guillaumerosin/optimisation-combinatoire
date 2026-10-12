#include "local_search.hpp"

#include <algorithm>
#include <limits>

namespace {
std::vector<int> sequence_from_schedule(const Instance& instance,
                                         const Schedule& schedule) {
    std::vector<std::pair<int, int>> ordered;
    for (int job = 0; job < instance.jobs; ++job) {
        for (int operation = 0; operation < instance.machines; ++operation) {
            ordered.emplace_back(
                schedule.jobs[job][operation].start, job);
        }
    }
    std::stable_sort(ordered.begin(), ordered.end());
    std::vector<int> sequence;
    for (const auto& item : ordered) {
        sequence.push_back(item.second);
    }
    return sequence;
}
}

Schedule tabu_improve(const Instance& instance, const Schedule& initial,
                      std::mt19937_64& random,
                      std::chrono::steady_clock::time_point deadline,
                      int max_iterations) {
    Schedule best = initial;
    std::vector<int> sequence = sequence_from_schedule(instance, initial);
    std::uniform_int_distribution<int> position(0,
        static_cast<int>(sequence.size()) - 1);
    int stagnant = 0;
    for (int iteration = 0; iteration < max_iterations && stagnant < 20;
         ++iteration) {
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
        Schedule candidate = best;
        std::vector<int> candidate_sequence = sequence;
        int left = position(random);
        int right = position(random);
        if (left == right) {
            continue;
        }
        if (left > right) {
            std::swap(left, right);
        }
        std::swap(candidate_sequence[left], candidate_sequence[right]);
        candidate = decode_semi_active(instance, candidate_sequence);
        if (candidate.makespan < best.makespan) {
            best = candidate;
            sequence = std::move(candidate_sequence);
            stagnant = 0;
        } else {
            ++stagnant;
        }
    }
    return best;
}
