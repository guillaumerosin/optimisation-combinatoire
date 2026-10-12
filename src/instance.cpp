#include "instance.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
bool is_comment_or_empty(const std::string& line) {
    const auto first = line.find_first_not_of(" \t\r");
    return first == std::string::npos || line[first] == '#';
}
}

Instance read_instance(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Impossible d'ouvrir l'instance: " + path);
    }

    // On nettoie d'abord le fichier pour accepter les commentaires et
    // les lignes vides, y compris avec des fins de ligne Windows.
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!is_comment_or_empty(line)) {
            lines.push_back(line);
        }
    }
    if (lines.empty()) {
        throw std::runtime_error("Instance vide: " + path);
    }

    Instance result;
    std::size_t index = 0;
    {
        // Certaines instances JSPLIB ont un nom en premiere ligne, d'autres
        // commencent directement par les dimensions jobs x machines.
        std::istringstream first(lines[0]);
        int a = 0;
        int b = 0;
        std::string extra;
        if ((first >> a >> b) && !(first >> extra)) {
            result.name = std::filesystem::path(path).stem().string();
        } else {
            result.name = lines[0];
            ++index;
        }
    }
    if (index >= lines.size()) {
        throw std::runtime_error("Dimensions absentes dans l'instance");
    }

    std::istringstream dimensions(lines[index++]);
    if (!(dimensions >> result.jobs >> result.machines) ||
        result.jobs <= 0 || result.machines <= 0) {
        throw std::runtime_error("Dimensions invalides dans l'instance");
    }
    result.operations.assign(static_cast<std::size_t>(result.jobs),
                             std::vector<Operation>());
    for (int job = 0; job < result.jobs; ++job) {
        if (index >= lines.size()) {
            throw std::runtime_error("Ligne manquante pour le job " +
                                     std::to_string(job));
        }
        std::istringstream row(lines[index++]);
        for (int operation = 0; operation < result.machines; ++operation) {
            Operation value;
            if (!(row >> value.machine >> value.duration) ||
                value.machine < 0 || value.machine >= result.machines ||
                value.duration < 0) {
                throw std::runtime_error("Operation invalide pour le job " +
                                         std::to_string(job));
            }
            result.operations[job].push_back(value);
        }
    }
    return result;
}
