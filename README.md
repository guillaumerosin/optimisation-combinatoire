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

Le solveur est un PSO pur (aucune recherche locale). Les valeurs par defaut
sont celles retenues apres reglage ; `./build/jsp_pso instance` suffit le jour
du concours.

| Option | Defaut | Role |
|---|---|---|
| `--time S` | 55 | budget en secondes (0 = sans limite) |
| `--threads N` | 0 | nombre d'essaims, 0 = un par coeur |
| `--topology` | `mixed` | `ring`, `global`, `cl` (CLPSO) ou `mixed` (un essaim sur deux en `cl`) |
| `--neighbors K` | 2 | voisins de chaque cote dans l'anneau |
| `--restart N` | 200 | iterations sans progres avant relance d'un essaim `ring` (0 = jamais) |
| `--migrate N` | 50 | toutes les N iterations, le record global rejoint chaque essaim (0 = jamais) |
| `--particles N` | 30 | particules par essaim |
| `--vmax V` | 0.2 | vitesse maximale par dimension |
| `--decoder` | `gt` | `gt` (Giffler-Thompson parametre) ou `semi` (semi-actif) |
| `--delta D` | 0.2 | fenetre du decodeur `gt` (0 = sans retard, 1 = actif) |
| `--no-decreasing-inertia` | | inertie fixe 0.7298 au lieu de 0.9 -> 0.4 |
| `--seed`, `--out`, `--target M`, `--log FICHIER` | | graine, dossier de sortie, arret a un makespan, courbe de convergence |

Le budget est mesure avec une horloge monotone ; le meilleur resultat est ecrit
periodiquement puis une derniere fois a la fin.

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
