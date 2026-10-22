import { useCallback, useEffect, useRef, useState } from 'react';
import type { SceneVehicle } from '../components/scene/BridgeScene';
import { WEBSOCKET_URL, type ScenarioName, type SimulationSpeed } from '../simulationConfig';

export type SimulationEvent = {
  type: string;
  sequence?: number;
  generation?: number;
  id?: number;
  vehicleType?: string;
  vehicleState?: string;
  scenario?: ScenarioName;
  simulationState?: SimulationStatus;
  speed?: SimulationSpeed;
  stepMode?: boolean;
  weight?: number;
  activeThreads?: number;
  waitingCars?: number;
  waitingTrucks?: number;
  crossingCars?: number;
  crossingTrucks?: number;
  monitorState?: string;
  step?: string;
  posixEvent?: string;
  explanation?: string;
  message?: string;
  vehicles?: Array<{ id: number; vehicleType: string; vehicleState: string }>;
  checks?: number;
  cpuMs?: number;
  observedWeight?: number;
  trucksServed?: number;
  cycle?: string;
  task?: string;
  action?: string;
  thread?: string;
  threadId?: string;
  threadState?: string;
  holds?: string;
  owns?: string;
  waitsFor?: string;
  priority?: string;
  busyState?: string;
  conditionState?: string;
  conditionWakeups?: number;
  waitingVehicleId?: number;
  waitingMs?: number;
  risk?: boolean;
  variant?: string;
  inheritanceApplied?: boolean;
  collapsed?: boolean;
};

export type SimulationStatus = 'IDLE' | 'RUNNING' | 'PAUSED' | 'STOPPING' | 'FINISHED' | 'ERROR';

export type SimulationViewState = {
  connected: boolean;
  generation: number;
  scenario: ScenarioName;
  simulationState: SimulationStatus;
  speed: SimulationSpeed;
  stepMode: boolean;
  weight: number;
  activeThreads: number;
  waitingCars: number;
  waitingTrucks: number;
  monitorState: string;
  vehicles: SceneVehicle[];
  events: SimulationEvent[];
  currentStep: string;
  posixEvent: string;
  explanation: string;
  collapsed: boolean;
  error: string;
  transitioning: boolean;
};

const initialState: SimulationViewState = {
  connected: false,
  generation: 0,
  scenario: 'NORMAL',
  simulationState: 'IDLE',
  speed: 1,
  stepMode: false,
  weight: 0,
  activeThreads: 0,
  waitingCars: 0,
  waitingTrucks: 0,
  monitorState: 'UNLOCKED',
  vehicles: [],
  events: [],
  currentStep: 'Prêt',
  posixEvent: 'Aucun',
  explanation: 'Sélectionnez un scénario ou créez un véhicule en mode NORMAL.',
  collapsed: false,
  error: '',
  transitioning: false,
};

function upsertVehicle(vehicles: SceneVehicle[], event: SimulationEvent): SceneVehicle[] {
  if (!event.id) return vehicles;
  const existing = vehicles.find(vehicle => vehicle.id === event.id);
  const state = event.vehicleState || existing?.state || 'APPEARING';
  if (existing) return vehicles.map(vehicle => vehicle.id === event.id ? { ...vehicle, state, vehicleType: event.vehicleType || vehicle.vehicleType } : vehicle);
  return [...vehicles, { id: event.id, vehicleType: event.vehicleType || 'CAR', state }];
}

function applyEvent(current: SimulationViewState, event: SimulationEvent): SimulationViewState {
  const generation = event.generation ?? current.generation;
  if (generation < current.generation) return current;
  let next = generation > current.generation
    ? { ...initialState, connected: current.connected, generation, speed: current.speed }
    : { ...current };

  if (event.type !== 'SNAPSHOT') next.events = [event, ...next.events].slice(0, 80);

  if (event.type === 'SNAPSHOT') {
    next = {
      ...next,
      generation,
      scenario: event.scenario || next.scenario,
      simulationState: event.simulationState || next.simulationState,
      speed: event.speed || next.speed,
      stepMode: event.stepMode ?? next.stepMode,
      weight: event.weight ?? 0,
      activeThreads: event.activeThreads ?? 0,
      waitingCars: event.waitingCars ?? 0,
      waitingTrucks: event.waitingTrucks ?? 0,
      vehicles: (event.vehicles || []).map(vehicle => ({ id: vehicle.id, vehicleType: vehicle.vehicleType, state: vehicle.vehicleState })),
      collapsed: event.collapsed ?? (event.scenario === 'RACE_CONDITION' && (event.weight || 0) > 15),
      transitioning: false,
    };
  }
  if (event.type === 'SCENARIO_STARTED') {
    next.scenario = event.scenario || next.scenario;
    next.simulationState = 'RUNNING';
    next.transitioning = false;
    next.weight = 0;
    next.vehicles = [];
    next.collapsed = false;
    next.currentStep = 'Démarrage';
  }
  if (event.type === 'SIMULATION_RESET') {
    next.weight = 0;
    next.vehicles = [];
    next.activeThreads = 0;
    next.waitingCars = 0;
    next.waitingTrucks = 0;
    next.collapsed = false;
    next.error = '';
    next.simulationState = 'IDLE';
    next.transitioning = false;
  }
  if (event.type.startsWith('VEHICLE_') && event.type !== 'VEHICLE_FINISHED') next.vehicles = upsertVehicle(next.vehicles, event);
  if (event.type === 'VEHICLE_FINISHED' && event.id) next.vehicles = next.vehicles.filter(vehicle => vehicle.id !== event.id);
  if (event.type === 'BRIDGE_STATE') {
    next.weight = event.weight ?? next.weight;
    next.waitingCars = event.waitingCars ?? next.waitingCars;
    next.waitingTrucks = event.waitingTrucks ?? next.waitingTrucks;
  }
  if (event.type === 'THREAD_COUNT_CHANGED') next.activeThreads = event.activeThreads ?? next.activeThreads;
  if (event.type === 'SIMULATION_STATE_CHANGED') next.simulationState = event.simulationState || next.simulationState;
  if (event.type === 'SPEED_CHANGED') next.speed = event.speed || next.speed;
  if (event.type === 'STEP_MODE_CHANGED') next.stepMode = event.stepMode ?? next.stepMode;
  if (event.type === 'MONITOR_EVENT') next.monitorState = event.monitorState || next.monitorState;
  if (event.type === 'SCENARIO_STEP') {
    next.currentStep = event.step || next.currentStep;
    next.posixEvent = event.posixEvent || next.posixEvent;
    next.explanation = event.explanation || next.explanation;
  }
  if (event.type === 'BRIDGE_COLLAPSED') {
    next.collapsed = true;
    next.weight = event.weight ?? 20;
    next.explanation = event.message || 'Surcharge contrôlée.';
  }
  if (event.type === 'DEADLOCK_DETECTED' || event.type === 'BUSY_WAIT_RESULT' || event.type === 'PRIORITY_VARIANT' || event.type === 'STARVATION_METRIC') {
    next.explanation = event.message || next.explanation;
  }
  if (event.type === 'ERROR') {
    next.error = event.message || 'Erreur backend';
    next.simulationState = 'ERROR';
  }
  if (next.scenario === 'NORMAL' && (next.weight < 0 || next.weight > 15)) {
    next.error = `Invariant violé en mode NORMAL : charge ${next.weight} t.`;
    next.simulationState = 'ERROR';
  }
  return next;
}

export function useSimulation() {
  const [state, setState] = useState(initialState);
  const socketRef = useRef<WebSocket | null>(null);
  const reconnectTimer = useRef<number | undefined>(undefined);

  useEffect(() => {
    let mounted = true;
    const connect = () => {
      if (!mounted) return;
      const socket = new WebSocket(WEBSOCKET_URL);
      socketRef.current = socket;
      socket.onopen = () => setState(current => ({ ...current, connected: true, error: '' }));
      socket.onmessage = message => {
        try {
          const event = JSON.parse(message.data) as SimulationEvent;
          setState(current => applyEvent(current, event));
        } catch {
          setState(current => ({ ...current, error: 'Événement WebSocket invalide.' }));
        }
      };
      socket.onerror = () => socket.close();
      socket.onclose = () => {
        if (!mounted) return;
        setState(current => ({ ...current, connected: false }));
        reconnectTimer.current = window.setTimeout(connect, 1500);
      };
    };
    connect();
    return () => {
      mounted = false;
      if (reconnectTimer.current) window.clearTimeout(reconnectTimer.current);
      socketRef.current?.close();
    };
  }, []);

  const send = useCallback((payload: object) => {
    if (socketRef.current?.readyState !== WebSocket.OPEN) {
      setState(current => ({ ...current, error: 'Backend déconnecté : commande non envoyée.' }));
      return false;
    }
    socketRef.current.send(JSON.stringify(payload));
    return true;
  }, []);

  const startScenario = useCallback((scenario: ScenarioName) => {
    setState(current => ({ ...current, transitioning: true }));
    if (!send({ command: 'START_SCENARIO', scenario })) setState(current => ({ ...current, transitioning: false }));
  }, [send]);

  return {
    state,
    createVehicle: (vehicleType: 'CAR' | 'TRUCK') => send({ command: 'CREATE_VEHICLE', vehicleType }),
    startScenario,
    replay: () => startScenario(state.scenario),
    reset: () => send({ command: 'RESET' }),
    pause: () => send({ command: 'PAUSE' }),
    resume: () => send({ command: 'RESUME' }),
    setSpeed: (speed: SimulationSpeed) => send({ command: 'SET_SPEED', speed }),
    setStepMode: (enabled: boolean) => send({ command: 'SET_STEP_MODE', enabled }),
    step: () => send({ command: 'STEP' }),
  };
}
