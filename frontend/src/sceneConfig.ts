/**
 * Repère unique de la scène :
 * X = largeur de la route, Y = hauteur, Z = sens longitudinal.
 * Les véhicules vont de ENTRY_Z (positif) vers EXIT_Z (négatif).
 */
export const ROAD_WIDTH = 5.4;
export const LANE_WIDTH = ROAD_WIDTH / 2;
export const BRIDGE_LENGTH = 5.6;
export const BRIDGE_WIDTH = ROAD_WIDTH;
export const RIVER_WIDTH = BRIDGE_LENGTH;
export const BARRIER_Z = BRIDGE_LENGTH / 2 + 1.05;
export const ENTRY_Z = 14;
export const EXIT_Z = -12;
export const LANE_CENTERS = [-LANE_WIDTH / 2, LANE_WIDTH / 2] as const;

export const ROAD_TOP_Y = 0.32;
export const MARKING_Y = ROAD_TOP_Y + 0.07;
export const BRIDGE_HALF_LENGTH = BRIDGE_LENGTH / 2;
export const WORLD_WIDTH = 20;
export const QUEUE_GAP = 2.35;
export const WAITING_LINE_Z = BARRIER_Z + 1.15;

export const ENTRY_ROAD_LENGTH = ENTRY_Z - BRIDGE_HALF_LENGTH;
export const ENTRY_ROAD_CENTER_Z = (ENTRY_Z + BRIDGE_HALF_LENGTH) / 2;
export const EXIT_ROAD_LENGTH = Math.abs(EXIT_Z) - BRIDGE_HALF_LENGTH;
export const EXIT_ROAD_CENTER_Z = (EXIT_Z - BRIDGE_HALF_LENGTH) / 2;

export function validateSceneConfig(): string[] {
  const errors: string[] = [];
  const entryRoadStart = ENTRY_ROAD_CENTER_Z - ENTRY_ROAD_LENGTH / 2;
  const exitRoadEnd = EXIT_ROAD_CENTER_Z + EXIT_ROAD_LENGTH / 2;
  if (BRIDGE_WIDTH !== ROAD_WIDTH) errors.push('Le pont et la route doivent avoir la même largeur.');
  if (Math.abs(entryRoadStart - BRIDGE_HALF_LENGTH) > 0.0001) errors.push('La route d’entrée ne touche pas le pont.');
  if (Math.abs(exitRoadEnd + BRIDGE_HALF_LENGTH) > 0.0001) errors.push('La route de sortie ne touche pas le pont.');
  if (RIVER_WIDTH < BRIDGE_LENGTH) errors.push('Le pont doit couvrir toute la largeur de l’oued.');
  if (BARRIER_Z <= BRIDGE_HALF_LENGTH) errors.push('La barrière doit être avant le pont, côté Z positif.');
  if (ENTRY_Z <= BARRIER_Z) errors.push('La route d’entrée doit laisser de la place à la file.');
  if (EXIT_Z >= -BRIDGE_HALF_LENGTH) errors.push('La route de sortie doit prolonger le pont côté Z négatif.');
  if (LANE_CENTERS.some(x => Math.abs(x) + 0.85 > ROAD_WIDTH / 2)) errors.push('Un camion sortirait de sa voie.');
  return errors;
}
