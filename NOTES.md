# Notes pour le rapport

## Étape 1 — Lecture du fichier d'instance

Le module `src/instance.cpp` lit le fichier texte et range l'atelier en mémoire :
le nom, le nombre de jobs et de machines, puis pour chaque job la liste ordonnée
de ses opérations (machine, durée). Il ignore les lignes vides, les commentaires
`#` et les fins de ligne Windows ; si la première ligne contient déjà les deux
dimensions, le nom est pris dans le nom du fichier. Un fichier mal formé (paire
manquante ou en trop, machine inexistante ou visitée deux fois par un job, job
manquant) produit un message d'erreur clair ; des lignes en trop après le dernier
job ne déclenchent qu'un avertissement, pour ne pas perdre une instance le jour du
concours. Les tests automatiques (CTest) vérifient l'exemple du cours (3×3,
makespan 11) et huit fichiers volontairement défectueux.
