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

// Decode une sequence de jobs en respectant les dates de disponibilite.
Schedule decode_semi_active(const Instance& instance,
                            const std::vector<int>& sequence);
// Giffler-Thompson parametre : priorities[job * m + operation] departage les
// operations en conflit (plus petite cle = prioritaire). delta = 1 donne un
// ordonnancement actif, delta = 0 un ordonnancement sans retard.
// Si starts n'est pas nul, il recoit la date de debut de chaque operation.
int decode_active(const Instance& instance,
                  const std::vector<double>& priorities, double delta,
                  std::vector<int>* starts = nullptr);
// Construit le planning complet a partir des dates de debut.
Schedule schedule_from_starts(const Instance& instance,
                              const std::vector<int>& starts);
// Ecrit uniquement le format JSON attendu par le concours.
void write_schedule_json(const Schedule& schedule, const Instance& instance,
                         const std::string& path);
