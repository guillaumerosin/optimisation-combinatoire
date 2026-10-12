#pragma once

#include <string>
#include <vector>

struct Operation {
    int machine = 0;
    int duration = 0;
};

struct Instance {
    std::string name;
    int jobs = 0;
    int machines = 0;
    std::vector<std::vector<Operation>> operations;
};

// Lit une instance texte dans le format jobs, machines, puis recettes.
Instance read_instance(const std::string& path);
