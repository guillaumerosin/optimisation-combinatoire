#include "local_search.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace {
struct Node {
    int job;
    int operation;
};

int id(const Instance& instance, Node node) {
    return node.job * instance.machines + node.operation;
}

Schedule rebuild(const Instance& instance,
                 const std::vector<std::vector<Node>>& machine_order) {
    // machine_order fixe les arcs disjonctifs du graphe : l'ordre choisi
    // sur chaque machine. On reconstruit ensuite les dates par propagation.
    const int total = instance.jobs * instance.machines;
    std::vector<int> job_done(instance.jobs, 0);
    std::vector<int> machine_done(instance.machines, 0);
    std::vector<int> machine_position(instance.machines, 0);
    Schedule result;
    result.jobs.assign(static_cast<std::size_t>(instance.jobs), {});
    for (int count = 0; count < total; ++count) {
        int chosen_machine = -1;
        int chosen_position = std::numeric_limits<int>::max();
        for (int machine = 0; machine < instance.machines; ++machine) {
            const int position = machine_position[machine];
            if (position >= static_cast<int>(machine_order[machine].size())) {
                continue;
            }
            const Node node = machine_order[machine][position];
            if (node.operation == job_done[node.job] &&
                position < chosen_position) {
                chosen_machine = machine;
                chosen_position = position;
            }
        }
        if (chosen_machine < 0) {
            return {};
        }
        const Node node = machine_order[chosen_machine][chosen_position];
        const auto& task = instance.operations[node.job][node.operation];
        const int start = std::max(job_done[node.job],
                                   machine_done[chosen_machine]);
        const int end = start + task.duration;
        result.jobs[node.job].push_back({task.machine, start, end});
        result.makespan = std::max(result.makespan, end);
        job_done[node.job] = end;
        machine_done[chosen_machine] = end;
        machine_position[chosen_machine]++;
    }
    for (const auto& operations : result.jobs) {
        if (static_cast<int>(operations.size()) != instance.machines) {
            return {};
        }
    }
    return result;
}

std::vector<std::vector<Node>> machine_order(const Instance& instance,
                                             const Schedule& schedule) {
    // On transforme les dates du planning en ordre de passage par machine.
    std::vector<std::vector<Node>> result(
        static_cast<std::size_t>(instance.machines));
    for (int job = 0; job < instance.jobs; ++job) {
        for (int operation = 0; operation < instance.machines; ++operation) {
            result[schedule.jobs[job][operation].machine].push_back(
                {job, operation});
        }
    }
    for (auto& order : result) {
        std::stable_sort(order.begin(), order.end(),
            [&schedule](Node left, Node right) {
                return schedule.jobs[left.job][left.operation].start <
                       schedule.jobs[right.job][right.operation].start;
            });
    }
    return result;
}

std::vector<Node> critical_path(const Instance& instance,
                                const Schedule& schedule) {
    // Le chemin critique est le plus long chemin du graphe des contraintes :
    // retarder une de ses operations peut retarder tout le makespan.
    const int total = instance.jobs * instance.machines;
    std::vector<int> distance(static_cast<std::size_t>(total), 0);
    std::vector<int> predecessor(static_cast<std::size_t>(total), -1);
    auto consider = [&](Node node, Node previous) {
        const int node_id = id(instance, node);
        const int previous_id = id(instance, previous);
        const int length = distance[previous_id] +
            instance.operations[node.job][node.operation].duration;
        if (length > distance[node_id]) {
            distance[node_id] = length;
            predecessor[node_id] = previous_id;
        }
    };
    for (int job = 0; job < instance.jobs; ++job) {
        for (int operation = 0; operation < instance.machines; ++operation) {
            const Node node{job, operation};
            distance[id(instance, node)] =
                instance.operations[job][operation].duration;
            if (operation > 0) consider(node, {job, operation - 1});
        }
    }
    const auto orders = machine_order(instance, schedule);
    for (const auto& order : orders) {
        for (std::size_t index = 1; index < order.size(); ++index) {
            consider(order[index], order[index - 1]);
        }
    }
    Node last{0, 0};
    for (int job = 0; job < instance.jobs; ++job) {
        for (int operation = 0; operation < instance.machines; ++operation) {
            if (schedule.jobs[job][operation].end >
                schedule.jobs[last.job][last.operation].end) {
                last = {job, operation};
            }
        }
    }
    std::vector<Node> path;
    int current = id(instance, last);
    while (current >= 0) {
        path.push_back({current / instance.machines, current % instance.machines});
        current = predecessor[current];
    }
    std::reverse(path.begin(), path.end());
    return path;
}
}

Schedule tabu_improve(const Instance& instance, const Schedule& initial,
                      std::mt19937_64&,
                      std::chrono::steady_clock::time_point deadline,
                      int max_iterations) {
    // best est la solution courante, tandis que best_known protege la
    // meilleure solution meme si la recherche accepte un mouvement degrade.
    Schedule best = initial;
    Schedule best_known = initial;
    auto order = machine_order(instance, best);
    std::unordered_map<std::string, int> tabu;
    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        if (std::chrono::steady_clock::now() >= deadline) break;
        const auto path = critical_path(instance, best);
        std::vector<std::pair<int, int>> candidates;
        // N5 : on ne considere que les paires voisines d'un meme bloc
        // critique sur une machine.
        for (std::size_t index = 1; index < path.size(); ++index) {
            const Node left = path[index - 1];
            const Node right = path[index];
            if (best.jobs[left.job][left.operation].machine !=
                best.jobs[right.job][right.operation].machine) {
                continue;
            }
            const int machine = best.jobs[left.job][left.operation].machine;
            auto& machine_operations = order[machine];
            auto position = std::find_if(machine_operations.begin(),
                machine_operations.end(), [&](Node node) {
                    return node.job == left.job && node.operation == left.operation;
                });
            if (position != machine_operations.end() &&
                position + 1 != machine_operations.end()) {
                candidates.emplace_back(machine,
                    static_cast<int>(position - machine_operations.begin()));
            }
        }
        if (candidates.empty()) break;
        Schedule iteration_best;
        iteration_best.makespan = std::numeric_limits<int>::max();
        std::vector<std::vector<Node>> best_order;
        std::string best_move;
        for (const auto& candidate : candidates) {
            if (std::chrono::steady_clock::now() >= deadline) break;
            auto trial_order = order;
            auto& operations = trial_order[candidate.first];
            std::swap(operations[candidate.second],
                      operations[candidate.second + 1]);
            const auto trial = rebuild(instance, trial_order);
            if (trial.jobs.empty()) continue;
            const std::string move = std::to_string(candidate.first) + ":" +
                                     std::to_string(candidate.second);
            const bool is_tabu = tabu.count(move) != 0 &&
                                 tabu[move] > iteration;
            // L'aspiration autorise un mouvement tabou s'il bat le meilleur
            // connu, ce qui evite de bloquer une amelioration importante.
            if ((!is_tabu || trial.makespan < best.makespan) &&
                trial.makespan < iteration_best.makespan) {
                iteration_best = trial;
                best_order = std::move(trial_order);
                best_move = move;
            }
        }
        if (best_order.empty()) break;
        order = std::move(best_order);
        best = iteration_best;
        if (best.makespan < best_known.makespan) {
            best_known = best;
        }
        tabu[best_move] = iteration + 10;
    }
    return best_known;
}
