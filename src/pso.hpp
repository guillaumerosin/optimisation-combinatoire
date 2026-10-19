#pragma once

#include "instance.hpp"
#include "schedule.hpp"

#include <cstdint>
#include <string>

struct SolverOptions {
    double seconds = 55.0;
    int target_makespan = -1;
    std::uint64_t seed = 0;
    int particles = 30;
    // 0 = autant de threads que de coeurs disponibles.
    int threads = 0;
    bool decreasing_inertia = true;
    std::string decoder = "gt";
    double delta = 0.2;
    // "ring" : chaque particule suit le meilleur de ses voisins ;
    // "global" : toutes suivent le meilleur de l'essaim ;
    // "cl" : CLPSO, chaque dimension apprend d'une particule differente ;
    // "mixed" : un thread sur deux en "ring", les autres en "cl".
    std::string topology = "mixed";
    int neighbors = 2;
    // Nombre d'iterations sans progres avant de relancer l'essaim (0 = jamais).
    int restart = 200;
    // Toutes les N iterations, un essaim recoit le meilleur des autres
    // threads (0 = essaims independants).
    int migrate = 50;
    double max_velocity = 0.2;
    std::string output_directory = ".";
    std::string log_path;
};

// Lance un ou plusieurs essaims jusqu'a la limite ou la cible demandee.
Schedule solve(const Instance& instance, const SolverOptions& options);
