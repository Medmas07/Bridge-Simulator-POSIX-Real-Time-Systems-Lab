# STR Bridge Simulator

Simulateur pédagogique du pont en POSIX : le backend C11 est la source de vérité et le frontend React/Three.js visualise les threads, la charge et les scénarios de concurrence.

## Démarrage rapide

### Backend

```sh
cmake -S backend -B backend/build
cmake --build backend/build
./backend/build/bridge_server
```

Le serveur WebSocket écoute sur `ws://localhost:8080`.

### Frontend

```sh
cd frontend
npm install
npm run dev
```

Le frontend fonctionne aussi en mode démo si le backend n'est pas lancé.

## Tests

```sh
cmake --build backend/build --target bridge_tests
ctest --test-dir backend/build --output-on-failure
```

Le mode normal utilise `pthread_mutex_t`, `pthread_cond_t` et deux files de condition distinctes. Les scénarios 2 à 6 sont des démonstrations bornées et explicitement présentées comme pédagogiques.

Voir [ARCHITECTURE.md](ARCHITECTURE.md) pour le détail du protocole et du moniteur.
