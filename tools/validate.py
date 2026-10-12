#!/usr/bin/env python3
"""Vérifie indépendamment un résultat JSP produit par le solveur."""
import json
import sys


def read_instance(path):
    with open(path, encoding="utf-8") as stream:
        lines = [line.strip() for line in stream
                 if line.strip() and not line.lstrip().startswith("#")]
    first = lines[0].split()
    if len(first) == 2 and all(token.lstrip("-").isdigit() for token in first):
        name = path.rsplit("/", 1)[-1].rsplit(".", 1)[0]
        pos = 0
    else:
        name = lines[0]
        pos = 1
    jobs, machines = map(int, lines[pos].split())
    pos += 1
    operations = []
    for _ in range(jobs):
        values = list(map(int, lines[pos].split()))
        pos += 1
        operations.append(list(zip(values[::2], values[1::2])))
    return name, jobs, machines, operations


def validate(instance_path, result_path):
    _, jobs, machines, operations = read_instance(instance_path)
    with open(result_path, encoding="utf-8") as stream:
        result = json.load(stream)
    assert set(result) == {"makespan", "jobs_schedule"}
    schedules = result["jobs_schedule"]
    assert len(schedules) == jobs
    maximum = 0
    machine_intervals = [[] for _ in range(machines)]
    for job in range(jobs):
        entries = schedules[f"Job_{job}"]
        assert len(entries) == machines
        previous_end = 0
        for index, entry in enumerate(entries):
            machine, duration = operations[job][index]
            assert entry["machine"] == machine
            assert entry["start"] >= previous_end
            assert entry["end"] - entry["start"] == duration
            assert entry["start"] >= 0
            previous_end = entry["end"]
            maximum = max(maximum, entry["end"])
            machine_intervals[machine].append((entry["start"], entry["end"]))
    for intervals in machine_intervals:
        intervals.sort()
        assert all(a[1] <= b[0] for a, b in zip(intervals, intervals[1:]))
    assert result["makespan"] == maximum


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("usage: validate.py INSTANCE RESULT", file=sys.stderr)
        sys.exit(2)
    try:
        validate(sys.argv[1], sys.argv[2])
    except (AssertionError, KeyError, ValueError, json.JSONDecodeError) as error:
        print(f"Résultat invalide: {error}", file=sys.stderr)
        sys.exit(1)
    print("OK")
