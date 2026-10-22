# Architecture

Le bouton React envoie une commande JSON `CREATE_VEHICLE`. Le serveur crée une structure `Vehicle`, lance `pthread_create`, puis le thread appelle `bridge_acquire`. Le moniteur protège uniquement les données partagées avec `bridge_mutex`; il ne garde jamais le mutex pendant la traversée.

`bridge_acquire` applique les règles suivantes : un camion entre seulement sur un pont vide; une voiture ne dépasse pas 15 tonnes et cède la priorité à un camion déjà en attente. Les boucles `while` autour de `pthread_cond_wait` protègent contre les réveils intempestifs. Chaque changement produit un événement placé dans une file thread-safe.

Le thread réseau vide cette file hors du mutex du pont et envoie les événements JSON au navigateur. Le renderer ne décide jamais de l'admission : il anime uniquement les états reçus. Au démarrage, `SNAPSHOT` resynchronise l'interface.

Les six scénarios sont lancés par le backend : normal utilise le moniteur; race condition réserve deux voitures après un check contrôlé sans rendre le C indéfini; starvation est bornée; deadlock utilise deux mutex POSIX avec un arrêt contrôlé; busy waiting compare un compteur de sondages; priority inversion expose une timeline contrôlée et indique si `PTHREAD_PRIO_INHERIT` est disponible.

## Organisation du code

`src/simulation.c` contient maintenant uniquement le cycle de vie de la simulation : état courant, génération, pause, vitesse, instantané JSON et lancement/arrêt contrôlé des threads. Les six démonstrations sont dans `scenarios/`, une par fichier. Les fonctions communes aux scénarios (barrière de synchronisation, étapes pédagogiques et attente des véhicules) sont regroupées dans `scenario_common.c`.

Le moniteur du pont reste isolé dans `src/bridge.c`. Les véhicules sont créés et joints dans `src/vehicle.c`, tandis que `src/websocket_server.c` ne fait que traduire les commandes réseau et diffuser les événements. Cette séparation permet de présenter chaque responsabilité indépendamment pendant la validation.

Chaque événement porte une génération de simulation. Lors d'un reset ou d'un changement de scénario, les événements d'une ancienne génération sont donc ignorés par le frontend; cela évite d'afficher un état ancien après un redémarrage.
