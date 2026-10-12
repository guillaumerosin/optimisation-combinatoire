#include "pso.hpp"

#include "local_search.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <random>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;

struct Particle {
    std::vector<double> position;
    std::vector<double> velocity;
    std::vector<double> best_position;
    Schedule best_schedule;
};

std::vector<int> decode_keys(const Instance& instance,
                             const std::vector<double>& keys) {
    std::vector<int> indices(keys.size());
    for (std::size_t index = 0; index < indices.size(); ++index) {
        indices[index] = static_cast<int>(index);
    }
    std::stable_sort(indices.begin(), indices.end(),
        [&keys](int left, int right) { return keys[left] < keys[right]; });
    std::vector<int> sequence;
    sequence.reserve(indices.size());
    for (int index : indices) {
        sequence.push_back(index / instance.machines);
    }
    return sequence;
}

Schedule decode(const Instance& instance, const std::vector<double>& keys,
                const SolverOptions& options) {
    const std::vector<int> sequence = decode_keys(instance, keys);
    if (options.decoder == "gt") {
        return decode_giffler_thompson(instance, sequence, options.delta);
    }
    return decode_semi_active(instance, sequence);
}

std::vector<double> keys_from_schedule(const Instance& instance,
                                       const Schedule& schedule,
                                       std::mt19937_64& random) {
    std::vector<std::pair<int, int>> order;
    for (int job = 0; job < instance.jobs; ++job) {
        for (int operation = 0; operation < instance.machines; ++operation) {
            order.emplace_back(schedule.jobs[job][operation].start,
                               job * instance.machines + operation);
        }
    }
    std::stable_sort(order.begin(), order.end());
    std::vector<double> keys(order.size());
    std::uniform_real_distribution<double> jitter(0.0, 0.25);
    for (std::size_t rank = 0; rank < order.size(); ++rank) {
        keys[order[rank].second] = static_cast<double>(rank) + jitter(random);
    }
    return keys;
}

void write_if_due(const Instance& instance, const Schedule& schedule,
                  const SolverOptions& options, const Clock::time_point& start,
                  Clock::time_point& last_write, bool force) {
    const auto now = Clock::now();
    if (!force && now - last_write < std::chrono::seconds(2)) {
        return;
    }
    std::filesystem::create_directories(options.output_directory);
    write_schedule_json(schedule, instance,
        (std::filesystem::path(options.output_directory) /
         (instance.name + ".json")).string());
    last_write = now;
    (void)start;
}
}

Schedule solve(const Instance& instance, const SolverOptions& options) {
    const auto start = Clock::now();
    const auto deadline = start + std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(options.seconds));
    std::mutex best_mutex;
    Schedule global_best;
    global_best.makespan = std::numeric_limits<int>::max();
    Clock::time_point last_write = start - std::chrono::seconds(2);
    std::ofstream log;
    if (!options.log_path.empty()) {
        log.open(options.log_path);
        log << "temps_ms;meilleur_makespan\n";
    }

    auto worker = [&](int worker_id) {
        std::mt19937_64 random(options.seed +
                               0x9e3779b97f4a7c15ULL * (worker_id + 1));
        const int dimension = instance.jobs * instance.machines;
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        std::uniform_real_distribution<double> velocity(-1.0, 1.0);
        std::vector<Particle> swarm;
        Schedule local_best;
        local_best.makespan = std::numeric_limits<int>::max();
        std::vector<double> global_position(static_cast<std::size_t>(dimension),
                                            0.0);
        for (int particle_index = 0;
             particle_index < std::max(1, options.particles) &&
             Clock::now() < deadline; ++particle_index) {
            Particle particle;
            particle.position.resize(dimension);
            particle.velocity.resize(dimension);
            for (int index = 0; index < dimension; ++index) {
                particle.position[index] = unit(random);
                particle.velocity[index] = velocity(random);
            }
            particle.best_position = particle.position;
            particle.best_schedule = decode(instance, particle.position, options);
            swarm.push_back(std::move(particle));
        }
        if (swarm.empty()) {
            return;
        }
        for (const auto& particle : swarm) {
            if (particle.best_schedule.makespan < local_best.makespan) {
                local_best = particle.best_schedule;
                global_position = particle.best_position;
            }
        }
        {
            std::lock_guard<std::mutex> lock(best_mutex);
            if (local_best.makespan < global_best.makespan) {
                global_best = local_best;
                write_if_due(instance, global_best, options, start, last_write,
                             true);
            }
        }

        while (options.use_pso && Clock::now() < deadline) {
            const double elapsed =
                std::chrono::duration<double>(Clock::now() - start).count();
            const double ratio = std::min(1.0, elapsed / options.seconds);
            const double inertia = options.decreasing_inertia
                ? 0.9 - 0.5 * ratio : 0.7298;
            for (auto& particle : swarm) {
                if (Clock::now() >= deadline) {
                    break;
                }
                for (int index = 0; index < dimension; ++index) {
                    const double r1 = unit(random);
                    const double r2 = unit(random);
                    const double target = global_position[index];
                    particle.velocity[index] =
                        inertia * particle.velocity[index] +
                        1.49618 * r1 *
                            (particle.best_position[index] -
                             particle.position[index]) +
                        1.49618 * r2 *
                            (target - particle.position[index]);
                    particle.velocity[index] =
                        std::clamp(particle.velocity[index], -1.0, 1.0);
                    particle.position[index] =
                        std::clamp(particle.position[index] +
                                       particle.velocity[index],
                                   0.0, 1.0);
                }
                Schedule current = decode(instance, particle.position, options);
                if (current.makespan < particle.best_schedule.makespan) {
                    particle.best_schedule = current;
                    particle.best_position = particle.position;
                    if (options.use_local_search) {
                        particle.best_schedule = tabu_improve(
                            instance, particle.best_schedule, random, deadline, 25);
                        particle.best_position = keys_from_schedule(
                            instance, particle.best_schedule, random);
                    }
                }
                if (particle.best_schedule.makespan < local_best.makespan) {
                    local_best = particle.best_schedule;
                    global_position = particle.best_position;
                    std::lock_guard<std::mutex> lock(best_mutex);
                    if (local_best.makespan < global_best.makespan) {
                        global_best = local_best;
                        write_if_due(instance, global_best, options, start,
                                     last_write, false);
                        if (log) {
                            log << std::chrono::duration_cast<
                                std::chrono::milliseconds>(
                                Clock::now() - start).count()
                                << ';' << global_best.makespan << '\n';
                        }
                    }
                }
            }
        }
        if (!options.use_pso && options.use_local_search) {
            while (Clock::now() < deadline) {
                Schedule candidate = tabu_improve(
                    instance, local_best, random, deadline, 80);
                if (candidate.makespan < local_best.makespan) {
                    local_best = candidate;
                    std::lock_guard<std::mutex> lock(best_mutex);
                    if (local_best.makespan < global_best.makespan) {
                        global_best = local_best;
                        write_if_due(instance, global_best, options, start,
                                     last_write, false);
                    }
                }
            }
        }
    };
    std::vector<std::thread> workers;
    for (int worker_id = 0; worker_id < std::max(1, options.threads);
         ++worker_id) {
        workers.emplace_back(worker, worker_id);
    }
    for (auto& worker_thread : workers) {
        worker_thread.join();
    }
    write_if_due(instance, global_best, options, start, last_write, true);
    return global_best;
}
