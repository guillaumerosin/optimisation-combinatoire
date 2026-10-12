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
