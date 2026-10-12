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
                     "[--no-ls] [--no-pso] [--target M] [--log FILE]\n";
        return 2;
    }
    try {
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
            } else if (argument == "--no-ls") {
                options.use_local_search = false;
            } else if (argument == "--no-pso") {
                options.use_pso = false;
            } else if (argument == "--no-decreasing-inertia") {
                options.decreasing_inertia = false;
            } else if (argument == "--log") {
                options.log_path = next();
            } else {
                throw std::runtime_error("Option inconnue: " + argument);
            }
        }
        if (options.threads <= 0 ||
            options.particles <= 0) {
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
