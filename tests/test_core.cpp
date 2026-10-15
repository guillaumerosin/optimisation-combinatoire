#include "instance.hpp"
#include "schedule.hpp"
#include "local_search.hpp"

// En Release, CMake definit NDEBUG, ce qui desactive assert : les tests
// passeraient toujours. On le retire avant d'inclure <cassert>.
#undef NDEBUG
#include <cassert>
#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

namespace {
void write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    file << content;
}

bool read_fails(const std::string& content) {
    write_file("bad_instance.txt", content);
    try {
        read_instance("bad_instance.txt");
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void test_read_instance_edge_cases() {
    // Fichier Windows avec une ligne vide : le nom ne garde pas le '\r'.
    write_file("crlf_instance.txt",
               "exemple\r\n3 3\r\n\r\n0 3 1 2 2 4\r\n"
               "1 4 2 3 0 2\r\n2 2 0 5 1 3\r\n");
    const Instance crlf = read_instance("crlf_instance.txt");
    assert(crlf.name == "exemple");
    assert(crlf.jobs == 3 && crlf.machines == 3);
    assert(crlf.operations[1][0].machine == 1);
    assert(crlf.operations[1][0].duration == 4);
    assert(crlf.operations[2][2].machine == 1);
    assert(crlf.operations[2][2].duration == 3);

    const std::string jobs = "1 4 2 3 0 2\n2 2 0 5 1 3\n";
    assert(read_fails("x\n3 3\n0 3 1 2\n" + jobs));          // trop court
    assert(read_fails("x\n3 3\n0 3 1 2 2 4 1 1\n" + jobs));  // trop long
    assert(read_fails("x\n3 3\n0 3 0 2 2 4\n" + jobs));      // machine 0 deux fois
    assert(read_fails("x\n3 3\n0 3 1 2 5 4\n" + jobs));      // machine 5 inexistante
    assert(read_fails("x\n3 3\n0 3 1 2 2 4\n1 4 2 3 0 2\n")); // job manquant
    assert(read_fails("x\n3 3 7\n0 3 1 2 2 4\n" + jobs));    // dimensions
    assert(read_fails("x\n3 3\n0 3 1 a 2 4\n" + jobs));      // pas un nombre
    assert(read_fails(""));                                   // vide
}
}

int main() {
    test_read_instance_edge_cases();
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
