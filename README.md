# Job Shop Scheduling par essaims particulaires

Solveur C++17 pour le problème d'ordonnancement d'atelier (Job Shop Scheduling,
JSP) basé sur l'optimisation par essaims particulaires (PSO). Le but est de
minimiser le makespan dans un budget de temps fixé.

## Compilation et utilisation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

./build/jsp_pso instances/abz5
```

Les valeurs par défaut sont celles retenues après réglage :
`./build/jsp_pso <instance>` suffit le jour du concours. Le meilleur
ordonnancement est écrit en JSON (`<nom_instance>.json`), régulièrement pendant
la recherche puis une dernière fois à la fin.

Le vérificateur indépendant contrôle un résultat :

```sh
python3 tools/validate.py instances/abz5 abz5.json
```

## Méthode

Le solveur est un PSO pur, sans recherche locale.

- **Codage** : chaque particule est un vecteur de clés aléatoires (*random
  keys*), une valeur réelle par opération.
- **Décodage** : les clés servent de priorités à un décodeur Giffler-Thompson
  paramétré (fenêtre `delta`), qui construit un ordonnancement réalisable. Un
  décodeur semi-actif est aussi disponible.
- **Initialisation** : les positions sont tirées au hasard. Dans le premier
  essaim, trois particules partent des règles de priorité SPT, MWKR et MOPNR
  pour fournir des points de départ structurés.
- **Essaims parallèles** : un essaim par cœur. La topologie `mixed` alterne
  des essaims en anneau et des essaims CLPSO. Un essaim en anneau qui stagne
  est relancé, et le meilleur résultat global est migré régulièrement dans
  chaque essaim.
- **Inertie** : décroissante de 0,9 à 0,4 au fil des itérations.

## Options

| Option | Défaut | Rôle |
|---|---|---|
| `--time S` | 60 | budget en secondes (0 = sans limite) |
| `--threads N` | 0 | nombre d'essaims, 0 = un par cœur |
| `--topology` | `mixed` | `ring`, `global`, `cl` (CLPSO) ou `mixed` (un essaim sur deux en `cl`) |
| `--neighbors K` | 2 | voisins de chaque côté dans l'anneau |
| `--restart N` | 200 | itérations sans progrès avant relance d'un essaim `ring` (0 = jamais) |
| `--migrate N` | 50 | toutes les N itérations, le record global rejoint chaque essaim (0 = jamais) |
| `--particles N` | 30 | particules par essaim |
| `--vmax V` | 0.2 | vitesse maximale par dimension |
| `--decoder` | `gt` | `gt` (Giffler-Thompson paramétré) ou `semi` (semi-actif) |
| `--delta D` | 0.2 | fenêtre du décodeur `gt` (0 = sans retard, 1 = actif) |
| `--no-decreasing-inertia` | | inertie fixe 0,7298 au lieu de 0,9 → 0,4 |
| `--seed N` | 0 | graine aléatoire |
| `--out DOSSIER` | `.` | dossier de sortie du JSON |
| `--target M` | | arrêt dès qu'un makespan ≤ M est atteint |
| `--log FICHIER` | | enregistre la courbe de convergence |

Le budget est mesuré avec une horloge monotone.

## Organisation du dépôt

| Chemin | Contenu |
|---|---|
| `src/instance.*` | lecture et validation des fichiers d'instance |
| `src/schedule.*` | décodeurs et écriture du JSON |
| `src/pso.*` | essaims particulaires |
| `src/main.cpp` | ligne de commande |
| `tests/` | tests automatiques (CTest) |
| `tools/` | vérificateur (`validate.py`) et banc d'essai (`bench.py`) |
| `instances/` | instance d'exemple (abz5) |
| `rapport/` | rapport LaTeX/PDF et données de convergence |

## Outils complémentaires

Lancer une série d'instances, valider chaque JSON et produire un CSV :

```sh
python3 tools/bench.py build/jsp_pso instances/ --time 60 --out resultats
```

Vérification mémoire sous Linux (AddressSanitizer / UBSan) :

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DJSP_SANITIZE=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```
