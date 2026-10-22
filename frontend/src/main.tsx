import { createRoot } from 'react-dom/client';
import { BridgeScene } from './components/scene/BridgeScene';
import { useSimulation, type SimulationEvent } from './hooks/useSimulation';
import {
  SCENARIO_LABELS,
  SCENARIO_PROBLEMS,
  SCENARIOS,
  SIMULATION_SPEEDS,
  type ScenarioName,
  type SimulationSpeed,
} from './simulationConfig';
import './style.css';
import './enhancements.css';

function App() {
  const { state, createVehicle, startScenario, replay, reset, pause, resume, setSpeed, setStepMode, step } = useSimulation();
  const waitingVehicles = state.vehicles.filter(vehicle => vehicle.state === 'WAITING');
  const crossingVehicles = state.vehicles.filter(vehicle => vehicle.state === 'CROSSING');
  const deadlockResources = state.events.filter(event => event.type === 'RESOURCE_STATE').slice(0, 2);
  const raceReads = state.events.filter(event => event.type === 'RACE_READ').slice(0, 2);
  const raceDecisions = state.events.filter(event => event.type === 'RACE_DECISION').slice(0, 2);
  const busyResult = state.events.find(event => event.type === 'BUSY_WAIT_RESULT');
  const starvationResult = state.events.find(event => event.type === 'STARVATION_METRIC');
  const priorityEvents = state.events.filter(event => event.type === 'PRIORITY_EVENT').slice(0, 8);
  const priorityVariants = state.events.filter(event => event.type === 'PRIORITY_VARIANT').slice(0, 2);
  const threadEvents = state.events.filter(event => event.threadId || event.task || event.type === 'RESOURCE_STATE').slice(0, 8);
  const controlsDisabled = !state.connected || state.transitioning || state.simulationState === 'STOPPING';

  return (
    <main>
      <header>
        <div>
          <span className="eyebrow">STR / POSIX LAB</span>
          <h1>Bridge <i>Simulator</i></h1>
          <p>Simulateur 3D piloté par de véritables threads POSIX.</p>
        </div>
        <div className={`status ${state.connected ? '' : 'offline'}`}><span className="dot" />{state.connected ? 'BACKEND CONNECTÉ' : 'BACKEND DÉCONNECTÉ'}</div>
      </header>

      <div className="toolbar">
        <button disabled={controlsDisabled || state.scenario !== 'NORMAL'} onClick={() => createVehicle('CAR')}>+ Voiture</button>
        <button disabled={controlsDisabled || state.scenario !== 'NORMAL'} className="orange" onClick={() => createVehicle('TRUCK')}>+ Camion</button>
        {state.simulationState === 'PAUSED'
          ? <button disabled={controlsDisabled} onClick={resume}>Reprendre</button>
          : <button disabled={controlsDisabled || state.simulationState !== 'RUNNING'} onClick={pause}>Pause</button>}
        <button disabled={controlsDisabled} onClick={replay}>Rejouer</button>
        <button disabled={!state.connected} className="quiet" onClick={reset}>Réinitialiser</button>
        <button disabled={!state.connected} className={state.stepMode ? 'selected' : ''} onClick={() => setStepMode(!state.stepMode)}>Pas-à-pas</button>
        <button disabled={controlsDisabled || !state.stepMode} onClick={step}>Étape suivante</button>
        <label>Vitesse
          <select value={state.speed} disabled={controlsDisabled} onChange={event => setSpeed(Number(event.target.value) as SimulationSpeed)}>
            {SIMULATION_SPEEDS.map(speed => <option key={speed} value={speed}>{speed}x</option>)}
          </select>
        </label>
      </div>

      {state.error && <div className="error-banner">{state.error}</div>}

      <div className="layout">
        <section className={`scene ${state.collapsed ? 'danger' : ''}`}>
          <BridgeScene vehicles={state.vehicles} weight={state.weight} speed={state.simulationState === 'PAUSED' ? 0 : state.speed} collapsed={state.collapsed} />
          <div className="weight">
            <small>POIDS ACTUEL · BACKEND</small>
            <strong>{state.weight} <em>/ 15 T</em></strong>
            <div className="bar"><span style={{ width: `${Math.min(state.weight / 15 * 100, 100)}%`, background: state.weight > 15 ? '#eb5757' : state.weight === 15 ? '#f2994a' : '#55d98a' }} /></div>
          </div>
          <div className="direction">ENTRÉE <span>→</span> PONT <span>→</span> SORTIE</div>
        </section>

        <aside>
          <div className="card">
            <div className="cardtitle">SIMULATION <span className={`state-pill state-${state.simulationState.toLowerCase()}`}>{state.simulationState}</span></div>
            <div className="metrics">
              <div><b>{state.activeThreads}</b><small>THREADS RÉELS</small></div>
              <div><b>{state.waitingCars + state.waitingTrucks}</b><small>EN ATTENTE</small></div>
              <div><b>G{state.generation}</b><small>GÉNÉRATION</small></div>
            </div>
          </div>

          <div className="card lesson">
            <div className="cardtitle">SCÉNARIO ACTIF</div>
            <h2>{SCENARIO_LABELS[state.scenario]}</h2>
            <p>{SCENARIO_PROBLEMS[state.scenario]}</p>
            <dl><dt>ÉTAPE</dt><dd>{state.currentStep}</dd><dt>ÉVÉNEMENT POSIX</dt><dd>{state.posixEvent}</dd><dt>EXPLICATION</dt><dd>{state.explanation}</dd></dl>
          </div>

          <div className="card monitor-card">
            <div className="cardtitle">DIAGNOSTIC DU MONITEUR</div>
            <div className="monitor-row"><span>MUTEX</span><b>{state.monitorState}</b></div>
            <div className="monitor-row"><span>CAPACITÉ</span><b>{state.weight} / 15 T</b></div>
            <small>Le mutex protège les données ; la barrière représente l’autorisation physique.</small>
          </div>

          <div className="card inspector-card">
            <div className="cardtitle">THREAD INSPECTOR <span className="subtle">ÉTATS INSTRUMENTÉS</span></div>
            <div className="inspector-list">
              {threadEvents.length === 0 && <p className="empty-state">Les threads apparaîtront pendant le scénario.</p>}
              {threadEvents.map((event, index) => (
                <div className="thread-row" key={`${event.type}-${event.threadId || event.task}-${index}`}>
                  <div><b>{event.threadId || event.task || event.thread || 'THREAD'}</b><small>{event.priority || event.type}</small></div>
                  <span className={`thread-state ${String(event.threadState || event.action || '').toLowerCase()}`}>{event.threadState || event.action || 'EVENT'}</span>
                  <small>{event.holds || event.owns ? `détient ${event.holds || event.owns}` : event.waitsFor ? `attend ${event.waitsFor}` : '—'}</small>
                </div>
              ))}
            </div>
          </div>

          <ScenarioObservatory
            scenario={state.scenario}
            deadlockResources={deadlockResources}
            raceReads={raceReads}
            raceDecisions={raceDecisions}
            busyResult={busyResult}
            starvationResult={starvationResult}
            priorityEvents={priorityEvents}
            priorityVariants={priorityVariants}
          />

          <div className="card">
            <div className="cardtitle">SCÉNARIOS DE CONCURRENCE</div>
            <div className="scenarios">
              {SCENARIOS.map((scenario, index) => (
                <button disabled={controlsDisabled} className={state.scenario === scenario ? 'selected' : ''} onClick={() => startScenario(scenario)} key={scenario}>
                  <span>0{index + 1}</span>{SCENARIO_LABELS[scenario]}
                </button>
              ))}
            </div>
          </div>

          <div className="card queues">
            <div className="cardtitle">FILES RÉELLES DU BACKEND</div>
            <p>ATTENTE <span>{waitingVehicles.map(vehicle => `${vehicle.vehicleType === 'TRUCK' ? 'C' : 'V'}${vehicle.id}`).join(' → ') || '—'}</span></p>
            <p>TRAVERSÉE <span className="accent">{crossingVehicles.map(vehicle => `${vehicle.vehicleType === 'TRUCK' ? 'C' : 'V'}${vehicle.id}`).join(' + ') || '—'}</span></p>
          </div>

          <div className="card log">
            <div className="cardtitle">TIMELINE DES ÉVÉNEMENTS</div>
            {state.events.slice(0, 12).map((event, index) => (
              <p key={`${event.generation}-${event.sequence}-${index}`}><time>#{event.sequence || '—'}</time> <b>{event.type}</b> {event.message || event.action || event.step}</p>
            ))}
          </div>
        </aside>
      </div>
    </main>
  );
}

type ObservatoryProps = {
  scenario: ScenarioName;
  deadlockResources: SimulationEvent[];
  raceReads: SimulationEvent[];
  raceDecisions: SimulationEvent[];
  busyResult?: SimulationEvent;
  starvationResult?: SimulationEvent;
  priorityEvents: SimulationEvent[];
  priorityVariants: SimulationEvent[];
};

function ScenarioObservatory({ scenario, deadlockResources, raceReads, raceDecisions, busyResult, starvationResult, priorityEvents, priorityVariants }: ObservatoryProps) {
  if (scenario === 'DEADLOCK') return (
    <div className="card observatory deadlock-observatory">
      <div className="cardtitle">RESOURCE GRAPH <span className="danger-label">CYCLE DÉTECTÉ</span></div>
      <div className="resource-graph">
        {deadlockResources.length === 0 && <p className="empty-state">En attente des deux mutex.</p>}
        {deadlockResources.map((event, index) => <div className="resource-node" key={`${event.threadId}-${index}`}><b>{event.threadId || event.thread}</b><span> détient {event.holds || event.owns}</span><strong>→ attend {event.waitsFor}</strong></div>)}
      </div>
      <p className="pedagogical-note">T1 détient M1 et attend M2 ; T2 détient M2 et attend M1. L’arrêt contrôlé permet à RESET de récupérer les ressources.</p>
    </div>
  );
  if (scenario === 'BUSY_WAITING') return (
    <div className="card observatory busy-observatory">
      <div className="cardtitle">WAITING COMPARISON</div>
      <div className="comparison-grid"><div><b>BUSY WAITING</b><strong>{busyResult?.checks?.toLocaleString() || '—'}</strong><small>vérifications · {busyResult?.cpuMs ?? '—'} ms CPU</small><span className="cpu-pulse">CPU · RUNNING</span></div><div><b>PTHREAD_COND_WAIT</b><strong>{busyResult?.conditionWakeups ?? '—'}</strong><small>réveil(s)</small><span>WAITING · bloqué</span></div></div>
      <p className="pedagogical-note">L’attente active vérifie continuellement la condition ; la variable POSIX endort le thread jusqu’au signal.</p>
    </div>
  );
  if (scenario === 'STARVATION') return (
    <div className="card observatory starvation-observatory">
      <div className="cardtitle">STARVATION MONITOR <span className="danger-label">RISQUE DE FAMINE</span></div>
      <div className="starvation-metrics"><div><b>V{starvationResult?.waitingVehicleId || 2}</b><small>véhicule retardé</small></div><div><b>{starvationResult?.trucksServed ?? 0}</b><small>camions servis</small></div><div><b>{starvationResult?.waitingMs ?? 0} ms</b><small>attente logique</small></div></div>
      <p className="pedagogical-note">La démonstration est bornée : elle montre le risque créé par un flux continu prioritaire, sans prétendre prouver une attente infinie.</p>
    </div>
  );
  if (scenario === 'RACE_CONDITION') return (
    <div className="card observatory race-observatory">
      <div className="cardtitle">CHECK → ACT <span className="danger-label">INTERLEAVING</span></div>
      <div className="race-grid"><div><b>LECTURES</b>{raceReads.map(event => <span key={event.id}>T{event.id} lit {event.observedWeight} t</span>)}</div><div><b>DÉCISIONS</b>{raceDecisions.map(event => <span key={event.id}>T{event.id} autorise 10 + 5</span>)}</div></div>
      <p className="pedagogical-note">La barrière rouge et l’effondrement sont déclenchés par les événements backend après la charge contrôlée de 20 t.</p>
    </div>
  );
  if (scenario === 'PRIORITY_INVERSION') return (
    <div className="card observatory priority-observatory">
      <div className="cardtitle">PRIORITY TIMELINE <span className="subtle">DÉMONSTRATION CONTRÔLÉE</span></div>
      <div className="variant-list">{priorityVariants.map((event, index) => <span key={`${event.variant}-${index}`}>{event.variant} · héritage {event.inheritanceApplied ? 'appliqué' : 'non disponible'}</span>)}</div>
      <div className="priority-timeline">{priorityEvents.map((event, index) => <div key={`${event.threadId}-${index}`}><time>#{priorityEvents.length - index}</time><b>{event.threadId || event.task}</b><span className={`thread-state ${String(event.threadState || '').toLowerCase()}`}>{event.threadState || event.action}</span><small>{event.priority || ''}</small></div>)}</div>
      <p className="pedagogical-note">Les priorités demandées sont affichées séparément de l’ordonnancement Linux réellement appliqué.</p>
    </div>
  );
  return <div className="card observatory normal-observatory"><div className="cardtitle">MONITEUR EN ACTION</div><p className="pedagogical-note">Les admissions et les attentes visibles proviennent des mutex et variables de condition POSIX du backend. Le pont reste limité à 15 t.</p></div>;
}

createRoot(document.getElementById('root')!).render(<App />);
