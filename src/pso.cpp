#include "pso.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    int best_makespan = std::numeric_limits<int>::max();
    // CLPSO : exemplar[d] est la particule dont le meilleur personnel guide
    // la dimension d ; stale compte les iterations sans progres.
    std::vector<int> exemplar;
    int stale = 0;
};

std::vector<int> decode_keys(const Instance& instance,
                             const std::vector<double>& keys) {
    // Decodeur "semi" : le tri des cles donne une sequence de jobs ;
    // l'indice divise par m donne le job.
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

// Evalue une position. Le planning complet n'est construit que lorsqu'il
// est demande (nouveau record), pour garder l'evaluation rapide.
int evaluate(const Instance& instance, const std::vector<double>& keys,
             const SolverOptions& options, Schedule* schedule = nullptr) {
    if (options.decoder == "semi") {
        Schedule decoded = decode_semi_active(instance,
                                              decode_keys(instance, keys));
        const int makespan = decoded.makespan;
        if (schedule) *schedule = std::move(decoded);
        return makespan;
    }
    if (!schedule) return decode_active(instance, keys, options.delta);
    std::vector<int> starts;
    const int makespan = decode_active(instance, keys, options.delta, &starts);
    *schedule = schedule_from_starts(instance, starts);
    return makespan;
}

std::vector<double> keys_from_priority(const Instance& instance, int rule) {
    // Quelques particules partent de regles de priorite classiques
    // (0 : SPT, 1 : MWKR, 2 : MOPNR) pour donner a l'essaim des points de
    // depart structures.
    std::vector<int> next(instance.jobs, 0);
    std::vector<int> sequence;
    sequence.reserve(static_cast<std::size_t>(instance.jobs * instance.machines));
    const auto score = [&](int job) {
        if (rule == 0) return -instance.operations[job][next[job]].duration;
        int value = 0;
        for (int operation = next[job]; operation < instance.machines;
             ++operation) {
            value += rule == 2 ? 1 : instance.operations[job][operation].duration;
        }
        return value;
    };
    for (int step = 0; step < instance.jobs * instance.machines; ++step) {
        int selected = -1;
        for (int job = 0; job < instance.jobs; ++job) {
            if (next[job] >= instance.machines) continue;
            if (selected < 0 || score(job) > score(selected)) selected = job;
        }
        sequence.push_back(selected);
        ++next[selected];
    }
    std::vector<int> occurrences(instance.jobs, 0);
    std::vector<double> keys(sequence.size());
    for (std::size_t rank = 0; rank < sequence.size(); ++rank) {
        const int job = sequence[rank];
        const int operation = occurrences[job]++;
        keys[static_cast<std::size_t>(job * instance.machines + operation)] =
            static_cast<double>(rank) / static_cast<double>(sequence.size());
    }
    return keys;
}

void write_if_due(const Instance& instance, const Schedule& schedule,
                  const SolverOptions& options, Clock::time_point& last_write,
                  bool force) {
    const auto now = Clock::now();
    // Une ecriture periodique protege le meilleur resultat si le programme
    // est interrompu, sans ralentir chaque evaluation.
    if (!force && now - last_write < std::chrono::seconds(2)) {
        return;
    }
    std::filesystem::create_directories(options.output_directory);
    write_schedule_json(schedule, instance,
        (std::filesystem::path(options.output_directory) /
         (instance.name + ".json")).string());
    last_write = now;
}
}

Schedule solve(const Instance& instance, const SolverOptions& options) {
    const auto start = Clock::now();
    // --time 0 signifie "pas de limite". Dans ce cas, --target peut arreter
    // la recherche lorsqu'un makespan donne est atteint.
    const auto deadline = options.seconds == 0.0
        ? Clock::time_point::max()
        : start + std::chrono::duration_cast<Clock::duration>(
              std::chrono::duration<double>(options.seconds));
    const auto elapsed_ms = [&start]() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - start).count();
    };
    std::atomic<bool> target_reached(false);
    std::atomic<long> evaluations(0);
    // Le makespan du record est lu sans verrou a chaque evaluation ; le
    // planning et la position du record sont proteges par le mutex.
    std::atomic<int> best_makespan(std::numeric_limits<int>::max());
    std::mutex best_mutex;
    Schedule global_best;
    global_best.makespan = std::numeric_limits<int>::max();
    std::vector<double> global_best_position;
    Clock::time_point last_write = start - std::chrono::seconds(2);
    std::ofstream log;
    if (!options.log_path.empty()) {
        log.open(options.log_path);
        log << "temps_ms;meilleur_makespan\n";
    }

    const auto publish = [&](const std::vector<double>& position, int makespan,
                             int worker_id) {
        if (makespan >= best_makespan.load()) return;
        std::lock_guard<std::mutex> lock(best_mutex);
        if (makespan >= global_best.makespan) return;
        evaluate(instance, position, options, &global_best);
        global_best_position = position;
        best_makespan.store(global_best.makespan);
        std::cout << "[progression] temps_ms=" << elapsed_ms()
                  << " makespan=" << global_best.makespan
                  << " thread=" << worker_id << std::endl;
        write_if_due(instance, global_best, options, last_write, false);
        if (log) log << elapsed_ms() << ';' << global_best.makespan << '\n';
        if (options.target_makespan > 0 &&
            global_best.makespan <= options.target_makespan) {
            target_reached.store(true);
        }
    };

    // Chaque worker possede son propre essaim et son propre generateur
    // aleatoire. Seul le meilleur global est partage entre les threads.
    auto worker = [&](int worker_id) {
        std::mt19937_64 random(options.seed +
                               0x9e3779b97f4a7c15ULL * (worker_id + 1));
        const int dimension = instance.jobs * instance.machines;
        const int size = std::max(1, options.particles);
        const double max_velocity = options.max_velocity;
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        std::uniform_real_distribution<double> velocity(-max_velocity,
                                                        max_velocity);

        std::vector<Particle> swarm(static_cast<std::size_t>(size));

        const auto randomize = [&](Particle& particle) {
            for (int index = 0; index < dimension; ++index) {
                particle.position[index] = unit(random);
                particle.velocity[index] = velocity(random);
            }
            particle.best_position = particle.position;
            particle.best_makespan =
                evaluate(instance, particle.position, options);
            particle.exemplar.clear();
            particle.stale = 0;
        };

        // CLPSO (Liang et al., 2006) : chaque dimension apprend du meilleur
        // personnel d'une particule tiree par tournoi, avec une probabilite
        // propre a chaque particule. Les particules n'imitent donc jamais
        // toutes le meilleur, ce qui garde la diversite en grande dimension.
        const auto choose_exemplars = [&](int owner) {
            Particle& particle = swarm[owner];
            const double learning = size <= 1 ? 0.0 :
                0.05 + 0.45 * (std::exp(10.0 * owner / (size - 1)) - 1.0) /
                                  (std::exp(10.0) - 1.0);
            std::uniform_int_distribution<int> other(0, size - 1);
            const auto tournament = [&]() {
                int first = other(random);
                int second = other(random);
                if (size > 1) {
                    while (first == owner) first = other(random);
                    while (second == owner) second = other(random);
                }
                return swarm[first].best_makespan <=
                       swarm[second].best_makespan ? first : second;
            };
            particle.exemplar.assign(dimension, owner);
            bool learns = false;
            for (int key = 0; key < dimension; ++key) {
                if (unit(random) < learning) {
                    particle.exemplar[key] = tournament();
                    learns = true;
                }
            }
            if (!learns && size > 1) {
                std::uniform_int_distribution<int> any_key(0, dimension - 1);
                particle.exemplar[any_key(random)] = tournament();
            }
            particle.stale = 0;
        };

        for (int index = 0; index < size; ++index) {
            Particle& particle = swarm[index];
            particle.position.resize(dimension);
            particle.velocity.resize(dimension);
            randomize(particle);
            // Seul le premier essaim recoit les regles de priorite : les
            // autres restent aleatoires pour diversifier les threads.
            if (worker_id == 0 && index < 3) {
                particle.position = keys_from_priority(instance, index);
                particle.best_position = particle.position;
                particle.best_makespan =
                    evaluate(instance, particle.position, options);
            }
            publish(particle.best_position, particle.best_makespan, worker_id);
        }

        // Les guides sont calcules au debut de chaque iteration a partir des
        // meilleurs personnels : guide[i] est le meilleur voisin de i.
        std::vector<int> guide(static_cast<std::size_t>(size), 0);
        const auto compute_guides = [&]() {
            int best = 0;
            for (int index = 1; index < size; ++index) {
                if (swarm[index].best_makespan < swarm[best].best_makespan) {
                    best = index;
                }
            }
            for (int index = 0; index < size; ++index) {
                if (options.topology == "global") {
                    guide[index] = best;
                    continue;
                }
                // Anneau : l'information circule de voisin en voisin, ce qui
                // retarde la convergence de tout l'essaim vers un meme point.
                int chosen = index;
                for (int offset = -options.neighbors;
                     offset <= options.neighbors; ++offset) {
                    const int other = ((index + offset) % size + size) % size;
                    if (swarm[other].best_makespan <
                        swarm[chosen].best_makespan) {
                        chosen = other;
                    }
                }
                guide[index] = chosen;
            }
            return best;
        };

        int swarm_best = swarm[compute_guides()].best_makespan;
        int stagnation = 0;
        long iteration = 0;
        const double c = 1.49618;
        // "mixed" alterne les deux variantes d'un thread a l'autre : CLPSO
        // explore mieux les petites instances, l'anneau converge plus vite
        // sur les grandes, et la migration partage le record entre eux.
        const bool comprehensive = options.topology == "cl" ||
            (options.topology == "mixed" && worker_id % 2 == 1);
        while (Clock::now() < deadline && !target_reached.load()) {
            ++iteration;
            evaluations += size;
            const double elapsed =
                std::chrono::duration<double>(Clock::now() - start).count();
            const double ratio = options.seconds == 0.0
                ? 0.0 : std::min(1.0, elapsed / options.seconds);
            const double inertia = options.decreasing_inertia
                ? 0.9 - 0.5 * ratio : 0.7298;
            const int best_index = compute_guides();

            for (int index = 0; index < size; ++index) {
                Particle& particle = swarm[index];
                if (comprehensive) {
                    if (particle.exemplar.empty() || particle.stale >= 7) {
                        choose_exemplars(index);
                    }
                    for (int key = 0; key < dimension; ++key) {
                        const double target =
                            swarm[particle.exemplar[key]].best_position[key];
                        double speed = inertia * particle.velocity[key] +
                            1.49445 * unit(random) *
                                (target - particle.position[key]);
                        speed = std::clamp(speed, -max_velocity, max_velocity);
                        particle.velocity[key] = speed;
                        particle.position[key] += speed;
                    }
                } else {
                    const std::vector<double>& social =
                        swarm[guide[index]].best_position;
                    // Mise a jour PSO : inertie, attraction vers le meilleur
                    // personnel puis vers le meilleur du voisinage.
                    for (int key = 0; key < dimension; ++key) {
                        double speed = inertia * particle.velocity[key] +
                            c * unit(random) *
                                (particle.best_position[key] -
                                 particle.position[key]) +
                            c * unit(random) *
                                (social[key] - particle.position[key]);
                        speed = std::clamp(speed, -max_velocity, max_velocity);
                        particle.velocity[key] = speed;
                        particle.position[key] += speed;
                    }
                }
                const int makespan =
                    evaluate(instance, particle.position, options);
                if (makespan < particle.best_makespan) {
                    particle.stale = 0;
                } else {
                    ++particle.stale;
                }
                // A makespan egal, on accepte quand meme le deplacement :
                // le JSP a de larges plateaux et cela evite de s'y figer.
                if (makespan <= particle.best_makespan) {
                    particle.best_makespan = makespan;
                    particle.best_position = particle.position;
                    publish(particle.position, makespan, worker_id);
                }
            }

            const int current = swarm[best_index].best_makespan;
            if (current < swarm_best) {
                swarm_best = current;
                stagnation = 0;
            } else {
                ++stagnation;
            }

            if (options.migrate > 0 && iteration % options.migrate == 0 &&
                best_makespan.load() < swarm_best) {
                // Multi-essaims : la pire particule adopte le record global.
                int worst = 0;
                for (int index = 1; index < size; ++index) {
                    if (swarm[index].best_makespan > swarm[worst].best_makespan) {
                        worst = index;
                    }
                }
                std::lock_guard<std::mutex> lock(best_mutex);
                swarm[worst].position = global_best_position;
                swarm[worst].best_position = global_best_position;
                swarm[worst].best_makespan = global_best.makespan;
            }

            // CLPSO entretient deja sa diversite : seul l'anneau redemarre.
            if (!comprehensive && options.restart > 0 &&
                stagnation >= options.restart) {
                // Redemarrage partiel : l'essaim a converge, on garde sa
                // meilleure particule et on relance toutes les autres.
                const int keep = compute_guides();
                for (int index = 0; index < size; ++index) {
                    if (index != keep) randomize(swarm[index]);
                }
                stagnation = 0;
            }
        }
    };
    // Un essaim par coeur : plus d'essaims, c'est plus de diversite pour le
    // meme temps de calcul.
    const int threads = options.threads > 0 ? options.threads
        : std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    std::vector<std::thread> workers;
    for (int worker_id = 0; worker_id < threads; ++worker_id) {
        workers.emplace_back(worker, worker_id);
    }
    for (auto& worker_thread : workers) {
        worker_thread.join();
    }
    write_if_due(instance, global_best, options, last_write, true);
    std::cout << "[fin] evaluations=" << evaluations.load() << std::endl;
    return global_best;
}
