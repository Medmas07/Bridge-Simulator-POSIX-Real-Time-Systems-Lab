import { useEffect, useMemo, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { BridgeScene, type SceneVehicle } from './components/scene/BridgeScene';
import './style.css';

type SimulationEvent = {
  type: string;
  id?: number;
  vehicleType?: string;
  weight?: number;
  message?: string;
};

function App() {
  const [events, setEvents] = useState<SimulationEvent[]>([]);
  const [weight, setWeight] = useState(0);
  const [vehicles, setVehicles] = useState<SceneVehicle[]>([]);
  const [scenario, setScenario] = useState('NORMAL');
  const [demoId, setDemoId] = useState(1);
  const [connected, setConnected] = useState(false);
  const ws = useMemo(() => {
    try { return new WebSocket('ws://localhost:8080'); }
    catch { return null; }
  }, []);

  function updateVehicle(id: number, state: string) {
    setVehicles(current => current.map(vehicle => vehicle.id === id ? { ...vehicle, state } : vehicle));
  }

  function applyEvent(event: SimulationEvent) {
    setEvents(current => [event, ...current].slice(0, 24));
    if (event.type === 'BRIDGE_WEIGHT_CHANGED') setWeight(event.weight || 0);
    if (event.type === 'VEHICLE_CREATED') {
      setVehicles(current => [...current, { id: event.id || 0, vehicleType: event.vehicleType || 'CAR', state: 'ARRIVING' }]);
    }
    if (event.id && event.type === 'VEHICLE_WAITING') updateVehicle(event.id, 'WAITING');
    if (event.id && event.type === 'VEHICLE_ENTERED') updateVehicle(event.id, 'CROSSING');
    if (event.id && event.type === 'VEHICLE_EXITED') {
      updateVehicle(event.id, 'EXITING');
      window.setTimeout(() => setVehicles(current => current.filter(vehicle => vehicle.id !== event.id)), 2600);
    }
  }

  useEffect(() => {
    if (!ws) return;
    ws.onopen = () => setConnected(true);
    ws.onclose = () => setConnected(false);
    ws.onmessage = message => {
      try { applyEvent(JSON.parse(message.data)); }
      catch { /* Le frontend ignore les messages qui ne sont pas du JSON valide. */ }
    };
    return () => ws.close();
  }, [ws]);

  function send(command: string) {
    if (ws?.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ command }));
      return;
    }
    const vehicleType = command.includes('TRUCK') ? 'TRUCK' : 'CAR';
    const id = demoId;
    const vehicleWeight = vehicleType === 'TRUCK' ? 15 : 5;
    setDemoId(id + 1);
    applyEvent({ type: 'VEHICLE_CREATED', id, vehicleType, message: 'Thread créé en mode démo' });
    window.setTimeout(() => applyEvent({ type: 'VEHICLE_WAITING', id, vehicleType, message: 'En attente devant la barrière' }), 700);
    window.setTimeout(() => {
      applyEvent({ type: 'VEHICLE_ENTERED', id, vehicleType, message: 'Véhicule admis' });
      setWeight(current => Math.min(15, current + vehicleWeight));
    }, 1500);
    window.setTimeout(() => {
      applyEvent({ type: 'VEHICLE_EXITED', id, vehicleType, message: 'Véhicule sorti' });
      setWeight(current => Math.max(0, current - vehicleWeight));
    }, 5200);
  }

  function reset() {
    setVehicles([]);
    setWeight(0);
    setEvents([]);
    if (ws?.readyState === WebSocket.OPEN) ws.send(JSON.stringify({ command: 'RESET' }));
  }

  function selectScenario(name: string) {
    setScenario(name);
    applyEvent({ type: 'SCENARIO_STARTED', message: `Scénario : ${name}` });
    if (name === 'RACE CONDITION') setWeight(20);
  }

  return (
    <main>
      <header>
        <div><span className="eyebrow">STR / POSIX LAB</span><h1>Bridge <i>Simulator</i></h1><p>Simulateur 3D de concurrence sur un pont à capacité limitée.</p></div>
        <div className="status"><span className="dot" />{connected ? 'BACKEND CONNECTÉ' : 'MODE DÉMO'}</div>
      </header>
      <div className="toolbar">
        <button onClick={() => send('CREATE_VEHICLE CAR')}>+ Voiture</button>
        <button className="orange" onClick={() => send('CREATE_VEHICLE TRUCK')}>+ Camion</button>
        <button className="quiet" onClick={reset}>Réinitialiser</button>
        <label>Vitesse <select defaultValue="1"><option value="1">1x</option><option value="0.5">0.5x</option><option value="2">2x</option></select></label>
      </div>
      <div className="layout">
        <section className="scene">
          <BridgeScene vehicles={vehicles} weight={weight} />
          <div className="weight"><small>POIDS ACTUEL</small><strong>{weight} <em>/ 15 T</em></strong><div className="bar"><span style={{ width: `${Math.min(weight / 15 * 100, 100)}%`, background: weight > 15 ? '#eb5757' : weight === 15 ? '#f2994a' : '#55d98a' }} /></div></div>
          <div className="direction">ENTRÉE <span>→</span> PONT <span>→</span> SORTIE</div>
        </section>
        <aside>
          <div className="card"><div className="cardtitle">MONITEUR <span className="live">● ACTIF</span></div><div className="metrics"><div><b>{vehicles.length}</b><small>THREADS</small></div><div><b>{vehicles.filter(vehicle => vehicle.state === 'WAITING').length}</b><small>EN ATTENTE</small></div><div><b>OK</b><small>ÉTAT</small></div></div></div>
          <div className="card"><div className="cardtitle">SCÉNARIOS DE CONCURRENCE</div><div className="scenarios">{['NORMAL', 'RACE CONDITION', 'FAMINE / STARVATION', 'DEADLOCK', 'BUSY WAITING', 'PRIORITY INVERSION'].map((name, index) => <button className={scenario === name ? 'selected' : ''} onClick={() => selectScenario(name)} key={name}><span>0{index + 1}</span>{name}</button>)}</div></div>
          <div className="card queues"><div className="cardtitle">FILES DU MONITEUR</div><p>ARRIVÉE <span>{vehicles.map(vehicle => `${vehicle.vehicleType === 'TRUCK' ? 'C' : 'V'}${vehicle.id}`).join(' → ') || '—'}</span></p><p>PASSAGE <span className="accent">{vehicles.filter(vehicle => vehicle.state === 'CROSSING').map(vehicle => `${vehicle.vehicleType === 'TRUCK' ? 'C' : 'V'}${vehicle.id}`).join(' → ') || '—'}</span></p></div>
          <div className="card log"><div className="cardtitle">JOURNAL DES ÉVÉNEMENTS</div>{events.slice(0, 6).map((event, index) => <p key={`${event.type}-${index}`}><time>{new Date().toLocaleTimeString('fr-FR')}</time> <b>{event.type}</b> {event.message}</p>)}</div>
        </aside>
      </div>
    </main>
  );
}

createRoot(document.getElementById('root')!).render(<App />);
