#!/usr/bin/env python3
"""Lance le solveur sur un dossier et produit un CSV minimal de mesures."""
import argparse
import csv
import pathlib
import subprocess
import time

from validate import validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    parser.add_argument("directory")
    parser.add_argument("--time", type=float, default=55.0)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--out", default="resultats")
    parser.add_argument("--csv", default="resultats/bench.csv")
    args = parser.parse_args()
    output = pathlib.Path(args.out)
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for instance in sorted(pathlib.Path(args.directory).iterdir()):
        if not instance.is_file():
            continue
        started = time.monotonic()
        subprocess.run(
            [args.executable, str(instance), "--time", str(args.time),
             "--seed", str(args.seed), "--out", str(output)],
            check=True,
        )
        result = output / (instance.stem + ".json")
        validate(str(instance), str(result))
        rows.append((instance.name, args.seed, time.monotonic() - started))
    csv_path = pathlib.Path(args.csv)
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("instance", "seed", "time_seconds"))
        writer.writerows(rows)


if __name__ == "__main__":
    main()
