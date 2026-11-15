#pragma once

#include <string>
#include <vector>
// Notre structure de données
struct Operation {        //les étapes pour la "recette"
    int machine = 0;        // quelle machine
    int duration = 0;    // Et combien de temps
};

struct Instance {        // Tout l'atelier 
    std::string name;   // Nom du fichier
    int jobs = 0;              // Nbr de job
    int machines = 0;          // Nbr de machines
    std::vector<std::vector<Operation>> operations;  //contruction de la liste d'opérations (recette d'un job)
};

// Lit une instance texte dans le format jobs, machines, puis recettes.
Instance read_instance(const std::string& path);
