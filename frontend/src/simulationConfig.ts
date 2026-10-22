export const APPEARANCE_SECONDS = 0.5;
export const APPROACH_SECONDS = 3;
export const CROSSING_SECONDS = 7;
export const EXIT_SECONDS = 2;
export const WEBSOCKET_URL = 'ws://localhost:8081';

export const SIMULATION_SPEEDS = [0.5, 1, 2] as const;
export type SimulationSpeed = typeof SIMULATION_SPEEDS[number];

export const SCENARIOS = [
  'NORMAL',
  'RACE_CONDITION',
  'STARVATION',
  'DEADLOCK',
  'BUSY_WAITING',
  'PRIORITY_INVERSION',
] as const;

export type ScenarioName = typeof SCENARIOS[number];

export const SCENARIO_LABELS: Record<ScenarioName, string> = {
  NORMAL: 'NORMAL',
  RACE_CONDITION: 'RACE CONDITION',
  STARVATION: 'FAMINE / STARVATION',
  DEADLOCK: 'DEADLOCK',
  BUSY_WAITING: 'BUSY WAITING',
  PRIORITY_INVERSION: 'PRIORITY INVERSION',
};

export const SCENARIO_PROBLEMS: Record<ScenarioName, string> = {
  NORMAL: 'Capacité de 15 t et priorité correcte aux camions.',
  RACE_CONDITION: 'Vérification et réservation non atomiques.',
  STARVATION: 'Une priorité stricte peut affamer une voiture.',
  DEADLOCK: 'Cycle d’attente entre deux mutex POSIX.',
  BUSY_WAITING: 'Comparaison entre sondage actif et condition.',
  PRIORITY_INVERSION: 'Une tâche moyenne retarde indirectement une tâche haute.',
};
