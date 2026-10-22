import { createRoot } from 'react-dom/client';
import { BridgeScene } from './components/scene/BridgeScene';
import { useSimulation } from './hooks/useSimulation';
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
  const { state, createVehicle, startScenario, replay, reset, pause, resume, setSpeed } = useSimulation();
  const waitingVehicles = state.vehicles.filter(vehicle => vehicle.state === 'WAITING');
  const crossingVehicles = state.vehicles.filter(vehicle => vehicle.state === 'CROSSING');
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

createRoot(document.getElementById('root')!).render(<App />);
