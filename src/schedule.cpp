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
    // next[job] indique quelle operation de la recette doit etre planifiee.
    // Les deux tableaux ready memorisent les contraintes deja rencontrees.
    std::vector<int> next(instance.jobs, 0);
    std::vector<int> job_ready(instance.jobs, 0);
    std::vector<int> machine_ready(instance.machines, 0);
    for (int job : sequence) {
        if (job < 0 || job >= instance.jobs || next[job] >= instance.machines) {
            throw std::runtime_error("Sequence invalide");
        }
        const int operation = next[job]++;
        const auto& task = instance.operations[job][operation];
        // Une operation attend a la fois la fin du job precedent et la
        // liberation de sa machine.
        const int start = std::max(job_ready[job], machine_ready[task.machine]);
        const int end = start + task.duration;
        result.jobs[job].push_back({task.machine, start, end});
        job_ready[job] = end;
        machine_ready[task.machine] = end;
        result.makespan = std::max(result.makespan, end);
    }
    return result;
}

int decode_active(const Instance& instance,
                  const std::vector<double>& priorities, double delta,
                  std::vector<int>* starts) {
    const int jobs = instance.jobs;
    const int machines = instance.machines;
    if (priorities.size() != static_cast<std::size_t>(jobs * machines)) {
        throw std::runtime_error("Vecteur de priorites de taille invalide");
    }
    delta = std::clamp(delta, 0.0, 1.0);
    if (starts) starts->assign(priorities.size(), 0);
    // Tampons reutilises d'un appel a l'autre : le decodeur est appele des
    // millions de fois, eviter les allocations compte.
    thread_local std::vector<int> next;
    thread_local std::vector<int> job_ready;
    thread_local std::vector<int> machine_ready;
    thread_local std::vector<int> earliest;
    thread_local std::vector<int> machine_earliest;
    // current_machine[job] vaut -1 quand le job est termine.
    thread_local std::vector<int> current_machine;
    next.assign(jobs, 0);
    job_ready.assign(jobs, 0);
    machine_ready.assign(machines, 0);
    earliest.assign(jobs, 0);
    machine_earliest.resize(machines);
    current_machine.resize(jobs);
    for (int job = 0; job < jobs; ++job) {
        current_machine[job] = instance.operations[job][0].machine;
    }
    int makespan = 0;
    for (int step = 0; step < jobs * machines; ++step) {
        // phi : plus petite date de fin parmi les operations disponibles.
        // Sa machine est celle ou se joue le prochain conflit. On note au
        // passage le plus petit debut possible sur chaque machine (sigma).
        std::fill(machine_earliest.begin(), machine_earliest.end(),
                  std::numeric_limits<int>::max());
        int phi = std::numeric_limits<int>::max();
        int conflict_machine = -1;
        for (int job = 0; job < jobs; ++job) {
            const int machine = current_machine[job];
            if (machine < 0) continue;
            const int begin = std::max(job_ready[job], machine_ready[machine]);
            earliest[job] = begin;
            machine_earliest[machine] = std::min(machine_earliest[machine], begin);
            const int end = begin + instance.operations[job][next[job]].duration;
            if (end < phi) {
                phi = end;
                conflict_machine = machine;
            }
        }
        // delta regle la fenetre [sigma, sigma + delta (phi - sigma)] des
        // candidats ; parmi eux, la particule decide : cle la plus petite.
        const int sigma = machine_earliest[conflict_machine];
        const double limit = sigma + delta * (phi - sigma);
        int chosen = -1;
        double chosen_priority = 0.0;
        for (int job = 0; job < jobs; ++job) {
            if (current_machine[job] != conflict_machine ||
                earliest[job] > limit) {
                continue;
            }
            const double priority = priorities[job * machines + next[job]];
            if (chosen < 0 || priority < chosen_priority) {
                chosen = job;
                chosen_priority = priority;
            }
        }
        const int operation = next[chosen]++;
        const int end = earliest[chosen] +
                        instance.operations[chosen][operation].duration;
        if (starts) (*starts)[chosen * machines + operation] = earliest[chosen];
        job_ready[chosen] = end;
        machine_ready[conflict_machine] = end;
        current_machine[chosen] = next[chosen] < machines
            ? instance.operations[chosen][next[chosen]].machine : -1;
        makespan = std::max(makespan, end);
    }
    return makespan;
}

Schedule schedule_from_starts(const Instance& instance,
                              const std::vector<int>& starts) {
    Schedule result;
    result.jobs.assign(static_cast<std::size_t>(instance.jobs), {});
    for (int job = 0; job < instance.jobs; ++job) {
        for (int operation = 0; operation < instance.machines; ++operation) {
            const auto& task = instance.operations[job][operation];
            const int start = starts[job * instance.machines + operation];
            result.jobs[job].push_back({task.machine, start, start + task.duration});
            result.makespan = std::max(result.makespan, start + task.duration);
        }
    }
    return result;
}

void write_schedule_json(const Schedule& schedule, const Instance& instance,
                         const std::string& path) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Impossible d'ecrire le resultat: " + path);
    }
    // Le JSON est volontairement ecrit a la main pour ne dependre d'aucune
    // bibliotheque externe.
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
