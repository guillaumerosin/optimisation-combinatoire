#include "instance.hpp"
#include "schedule.hpp"
#include "local_search.hpp"

#include <cassert>
#include <chrono>
#include <fstream>
#include <random>
#include <string>

int main() {
    const std::string path = "test_instance.txt";
    {
        std::ofstream file(path);
        file << "cours\n3 3\n"
             << "0 3 1 2 2 4\n"
             << "1 4 2 3 0 2\n"
             << "2 2 0 5 1 3\n";
    }
    const Instance instance = read_instance(path);
    assert(instance.name == "cours");
    const std::vector<int> sequence = {1, 0, 2, 2, 0, 1, 1, 2, 0};
    const Schedule schedule = decode_semi_active(instance, sequence);
    assert(schedule.makespan == 11);
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(1);
    std::mt19937_64 random(1);
    const Schedule improved = tabu_improve(
        instance, schedule, random, deadline, 10);
    assert(improved.makespan <= schedule.makespan);
    write_schedule_json(schedule, instance, "test_result.json");
    {
        std::ofstream unnamed("unnamed_instance.txt");
        unnamed << "3 3\n"
                << "0 3 1 2 2 4\n"
                << "1 4 2 3 0 2\n"
                << "2 2 0 5 1 3\n";
    }
    const Instance without_name = read_instance("unnamed_instance.txt");
    assert(without_name.name == "unnamed_instance");
    return 0;
}
