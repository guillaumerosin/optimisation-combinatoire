#include "instance.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
bool is_comment_or_empty(const std::string& line) {
    const auto first = line.find_first_not_of(" \t\r");
    return first == std::string::npos || line[first] == '#';
}

// Retire les espaces et le '\r' des fichiers Windows autour du nom,
// sinon le fichier de sortie s'appellerait "10_10\r.json".
std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
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
            result.name = trim(lines[0]);
            ++index;
        }
    }
    if (index >= lines.size()) {
        throw std::runtime_error("Dimensions absentes dans l'instance");
    }

    std::istringstream dimensions(lines[index++]);
    std::string extra;
    if (!(dimensions >> result.jobs >> result.machines) ||
        (dimensions >> extra) ||
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
        std::vector<bool> machine_used(
            static_cast<std::size_t>(result.machines), false);
        for (int operation = 0; operation < result.machines; ++operation) {
            Operation value;
            if (!(row >> value.machine >> value.duration) ||
                value.machine < 0 || value.machine >= result.machines ||
                value.duration < 0) {
                throw std::runtime_error("Operation invalide pour le job " +
                                         std::to_string(job));
            }
            // Dans un job shop, chaque job passe une seule fois par machine.
            if (machine_used[value.machine]) {
                throw std::runtime_error(
                    "Machine " + std::to_string(value.machine) +
                    " utilisee deux fois par le job " + std::to_string(job));
            }
            machine_used[value.machine] = true;
            result.operations[job].push_back(value);
        }
        std::string leftover;
        if (row >> leftover) {
            throw std::runtime_error("Trop de nombres pour le job " +
                                     std::to_string(job));
        }
    }
    // Des lignes en trop ne bloquent pas la resolution : on previent
    // seulement, pour ne pas perdre une instance le jour du concours.
    if (index < lines.size()) {
        std::cerr << "Attention: " << lines.size() - index
                  << " ligne(s) ignoree(s) apres le dernier job\n";
    }
    return result;
}
