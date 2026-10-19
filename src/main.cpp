#include "instance.hpp"
#include "pso.hpp"

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: jsp_pso <instance> [--time S] [--seed N] "
                     "[--out DIR] [--threads N] [--decoder semi|gt] "
                     "[--delta D] [--particles N] [--topology ring|global|cl|mixed] "
                     "[--neighbors K] [--restart N] [--migrate N] [--vmax V] "
                     "[--no-decreasing-inertia] [--target M] [--log FILE]\n";
        return 2;
    }
    try {
        // Les options restent simples pour pouvoir etre expliquees et
        // modifiees facilement pendant les essais du concours.
        SolverOptions options;
        std::string input = argv[1];
        for (int index = 2; index < argc; ++index) {
            const std::string argument = argv[index];
            auto next = [&]() -> std::string {
                if (++index >= argc) {
                    throw std::runtime_error("Option sans valeur: " + argument);
                }
                return argv[index];
            };
            if (argument == "--time") {
                options.seconds = std::stod(next());
                if (options.seconds < 0.0) {
                    throw std::runtime_error("--time doit etre positif ou nul");
                }
            } else if (argument == "--target") {
                options.target_makespan = std::stoi(next());
            } else if (argument == "--seed") {
                options.seed = std::stoull(next());
            } else if (argument == "--out") {
                options.output_directory = next();
            } else if (argument == "--threads") {
                options.threads = std::stoi(next());
            } else if (argument == "--particles") {
                options.particles = std::stoi(next());
            } else if (argument == "--decoder") {
                options.decoder = next();
                if (options.decoder != "semi" && options.decoder != "gt") {
                    throw std::runtime_error("Decodeur inconnu");
                }
            } else if (argument == "--delta") {
                options.delta = std::stod(next());
            } else if (argument == "--topology") {
                options.topology = next();
                if (options.topology != "ring" && options.topology != "global" &&
                    options.topology != "cl" && options.topology != "mixed") {
                    throw std::runtime_error("Topologie inconnue");
                }
            } else if (argument == "--neighbors") {
                options.neighbors = std::stoi(next());
            } else if (argument == "--restart") {
                options.restart = std::stoi(next());
            } else if (argument == "--migrate") {
                options.migrate = std::stoi(next());
            } else if (argument == "--vmax") {
                options.max_velocity = std::stod(next());
            } else if (argument == "--no-decreasing-inertia") {
                options.decreasing_inertia = false;
            } else if (argument == "--log") {
                options.log_path = next();
            } else {
                throw std::runtime_error("Option inconnue: " + argument);
            }
        }
        if (options.threads < 0 || options.particles <= 0 ||
            options.neighbors < 0 || options.restart < 0 ||
            options.migrate < 0 || options.max_velocity <= 0.0) {
            throw std::runtime_error("Les parametres numeriques doivent etre positifs");
        }
        const Instance instance = read_instance(input);
        const auto started = std::chrono::steady_clock::now();
        const Schedule schedule = solve(instance, options);
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        std::cout << "makespan=" << schedule.makespan
                  << " temps_secondes=" << elapsed << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Erreur: " << error.what() << '\n';
        return 1;
    }
}
