# JSP par essaims particulaires

Projet C++17 pour le Job Shop Scheduling. La construction du projet se fait avec
CMake :

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Le programme et les modules seront complétés par étapes. Le vérificateur
indépendant s'utilise ainsi :

```sh
python3 tools/validate.py instance.txt resultat.json
```

Options principales : `--time 55`, `--seed 0`, `--out resultats/`,
`--threads 1`, `--decoder semi|gt`, `--delta 0.4`, `--no-ls`, `--no-pso` et
`--log convergence.csv`. Le budget est mesure avec une horloge monotone ; le
meilleur resultat est ecrit periodiquement puis une derniere fois a la fin.

Verification memoire sous Linux :

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DJSP_SANITIZE=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

Le script `tools/bench.py` lance une serie d'instances, valide chaque JSON et
ecrit un CSV de temps :

```sh
python3 tools/bench.py build/jsp_pso instances/ --time 55 --out resultats
```
