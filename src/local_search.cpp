#include "local_search.hpp"

#include <algorithm>
#include <limits>
#include <queue>
#include <string>
#include <unordered_map>

namespace {
struct Node {
    int job;
    int operation;
};

bool operator==(Node left, Node right) {
    return left.job == right.job && left.operation == right.operation;
}

bool operator!=(Node left, Node right) {
    return !(left == right);
}

int node_id(const Instance& instance, Node node) {
    return node.job * instance.machines + node.operation;
}

struct RebuildResult {
    Schedule schedule;
    std::vector<int> predecessor;
    std::vector<Node> nodes;
};

// Recalcule les dates en respectant les arcs de recette et l'ordre machine.
RebuildResult rebuild(const Instance& instance,
                      const std::vector<std::vector<Node>>& machine_order) {
    const int total = instance.jobs * instance.machines;
    std::vector<Node> nodes(static_cast<std::size_t>(total));
    for (int job = 0; job < instance.jobs; ++job) {
        for (int operation = 0; operation < instance.machines; ++operation) {
            nodes[node_id(instance, {job, operation})] = {job, operation};
        }
    }
    std::vector<std::vector<int>> successors(static_cast<std::size_t>(total));
    std::vector<int> indegree(static_cast<std::size_t>(total), 0);
    auto add_arc = [&](Node from, Node to) {
        successors[node_id(instance, from)].push_back(node_id(instance, to));
        ++indegree[node_id(instance, to)];
    };
    for (int job = 0; job < instance.jobs; ++job) {
        for (int operation = 1; operation < instance.machines; ++operation) {
            add_arc({job, operation - 1}, {job, operation});
        }
    }
    for (const auto& order : machine_order) {
        for (std::size_t index = 1; index < order.size(); ++index) {
            add_arc(order[index - 1], order[index]);
        }
    }

    std::queue<int> ready;
    for (int index = 0; index < total; ++index) {
        if (indegree[index] == 0) ready.push(index);
    }
    std::vector<int> topological;
    std::vector<int> predecessor(static_cast<std::size_t>(total), -1);
    std::vector<int> starts(static_cast<std::size_t>(total), 0);
    while (!ready.empty()) {
        const int current = ready.front();
        ready.pop();
        topological.push_back(current);
        for (const int successor : successors[current]) {
            const int end = starts[current] +
                instance.operations[nodes[current].job][nodes[current].operation]
                    .duration;
            if (end > starts[successor]) {
                starts[successor] = end;
                predecessor[successor] = current;
            }
            if (--indegree[successor] == 0) ready.push(successor);
        }
    }
    if (static_cast<int>(topological.size()) != total) {
        return {};
    }

    RebuildResult result;
    result.nodes = std::move(nodes);
    result.predecessor = std::move(predecessor);
    result.schedule.jobs.assign(static_cast<std::size_t>(instance.jobs), {});
    for (int job = 0; job < instance.jobs; ++job) {
        result.schedule.jobs[job].resize(static_cast<std::size_t>(
            instance.machines));
    }
    for (const int current : topological) {
        const Node node = result.nodes[current];
        const auto& task = instance.operations[node.job][node.operation];
        const int end = starts[current] + task.duration;
        result.schedule.jobs[node.job][node.operation] =
            {task.machine, starts[current], end};
        result.schedule.makespan = std::max(result.schedule.makespan, end);
    }
    return result;
}

std::vector<std::vector<Node>> machine_order(const Instance& instance,
                                             const Schedule& schedule) {
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
    const auto order = machine_order(instance, schedule);
    const auto graph = rebuild(instance, order);
    if (graph.schedule.jobs.empty()) return {};
    int last = 0;
    for (int index = 1; index < instance.jobs * instance.machines; ++index) {
        const Node left = graph.nodes[last];
        const Node right = graph.nodes[index];
        if (schedule.jobs[right.job][right.operation].end >
            schedule.jobs[left.job][left.operation].end) {
            last = index;
        }
    }
    std::vector<Node> path;
    while (last >= 0) {
        path.push_back(graph.nodes[last]);
        last = graph.predecessor[last];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

std::string move_key(Node first, Node second) {
    return std::to_string(first.job) + ":" + std::to_string(first.operation) +
           "|" + std::to_string(second.job) + ":" +
           std::to_string(second.operation);
}
}

Schedule tabu_improve(const Instance& instance, const Schedule& initial,
                      std::mt19937_64&,
                      std::chrono::steady_clock::time_point deadline,
                      int max_iterations) {
    Schedule current = initial;
    Schedule best = initial;
    auto order = machine_order(instance, current);
    std::unordered_map<std::string, int> tabu_until;

    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        if (std::chrono::steady_clock::now() >= deadline) break;
        const auto path = critical_path(instance, current);
        std::vector<std::pair<Node, Node>> moves;
        for (std::size_t index = 0; index < path.size();) {
            std::size_t end = index + 1;
            const int machine = current.jobs[path[index].job][path[index].operation]
                                    .machine;
            while (end < path.size() &&
                   current.jobs[path[end].job][path[end].operation].machine ==
                       machine) {
                ++end;
            }
            const std::size_t length = end - index;
            if (length >= 2) {
                moves.emplace_back(path[index], path[index + 1]);
                if (length >= 3) {
                    moves.emplace_back(path[end - 2], path[end - 1]);
                }
            }
            index = end;
        }
        if (moves.empty()) break;

        Schedule chosen;
        chosen.makespan = std::numeric_limits<int>::max();
        std::vector<std::vector<Node>> chosen_order;
        std::string chosen_key;
        for (const auto& move : moves) {
            if (std::chrono::steady_clock::now() >= deadline) break;
            auto candidate_order = order;
            const int machine = current.jobs[move.first.job][move.first.operation]
                                    .machine;
            auto& machine_nodes = candidate_order[machine];
            auto first = std::find(machine_nodes.begin(), machine_nodes.end(),
                                   move.first);
            if (first == machine_nodes.end() ||
                first + 1 == machine_nodes.end() ||
                *(first + 1) != move.second) {
                continue;
            }
            std::iter_swap(first, first + 1);
            const auto candidate = rebuild(instance, candidate_order);
            if (candidate.schedule.jobs.empty()) continue;
            const std::string key = move_key(move.first, move.second);
            const bool tabu = tabu_until.count(key) != 0 &&
                              tabu_until[key] > iteration;
            const bool aspiration = candidate.schedule.makespan < best.makespan;
            if ((!tabu || aspiration) &&
                candidate.schedule.makespan < chosen.makespan) {
                chosen = candidate.schedule;
                chosen_order = std::move(candidate_order);
                chosen_key = move_key(move.second, move.first);
            }
        }
        if (chosen_order.empty()) break;
        current = chosen;
        order = std::move(chosen_order);
        tabu_until[chosen_key] = iteration + 8 + (iteration % 8);
        if (current.makespan < best.makespan) best = current;
    }
    return best;
}
