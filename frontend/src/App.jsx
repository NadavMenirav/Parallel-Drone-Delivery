import { useEffect, useMemo, useState } from 'react';
import { simAPI } from './api/simulationClient';
import ControlPanel from './components/ControlPanel';
import BakeryStats from './components/BakeryStats';
import CustomerTable from './components/CustomerTable';
import SimulationMap from './components/SimulationMap';
import './styles.css';

function normalizeStatus(status = '') {
  return String(status).replace('CUSTOMER_', '').toLowerCase();
}

function App() {
  const [allRounds, setAllRounds] = useState([]);
  const [currentFrame, setCurrentFrame] = useState(0);
  const [simState, setSimState] = useState({
    round: 0,
    bakeries: [],
    customers: [],
    drones: [],
  });

  const [isPlaying, setIsPlaying] = useState(false);
  const [error, setError] = useState(null);
  const [isLoading, setIsLoading] = useState(false);
  const [playSpeed, setPlaySpeed] = useState(900);
  const [selectedMock, setSelectedMock] = useState('mock1');

  useEffect(() => {
    fetchSimulationData();
  }, []);

  useEffect(() => {
    if (!isPlaying || allRounds.length === 0) return undefined;

    const interval = setInterval(() => {
      setCurrentFrame((frame) => {
        if (frame >= allRounds.length - 1) {
          setIsPlaying(false);
          return frame;
        }

        const nextFrame = frame + 1;
        setSimState(allRounds[nextFrame]);
        return nextFrame;
      });
    }, playSpeed);

    return () => clearInterval(interval);
  }, [isPlaying, allRounds, playSpeed]);

  const fetchSimulationData = async () => {
    setIsLoading(true);

    try {
      const response = await simAPI.getState();
      const data = response.data;

      if (Array.isArray(data) && data.length > 0) {
        setAllRounds(data);
        setSimState(data[0]);
        setCurrentFrame(0);
        setError(null);
      } else if (data && typeof data === 'object') {
        setAllRounds([data]);
        setSimState(data);
        setCurrentFrame(0);
        setError(null);
      } else {
        setError('Simulation data is empty. Generate a mock first.');
      }
    } catch (err) {
      console.error('Failed to fetch state', err);
      setError('Cannot connect to the API server. Make sure server.js is running.');
    } finally {
      setIsLoading(false);
    }
  };

  const runMock = async (mockName = selectedMock) => {
  setSelectedMock(mockName);
  setIsPlaying(false);
  setIsLoading(true);
  setError(`Generating ${mockName} from the C backend...`);

  try {
    const response = await simAPI.runMock(mockName);
    const data = response.data;
    console.log('MOCK:', mockName);
    console.log('FIRST ROUND:', data[0]);
    console.log('CUSTOMERS:', data[0]?.customers);
    console.log('DRONES:', data[0]?.drones);
    if (Array.isArray(data) && data.length > 0) {
      setCurrentFrame(0);
      setAllRounds([...data]);
      setSimState({ ...data[0] });
    } else if (data && typeof data === 'object') {
      setCurrentFrame(0);
      setAllRounds([{ ...data }]);
      setSimState({ ...data });
    } else {
      setError(`Generated ${mockName}, but response was empty.`);
      return;
    }

    setError(null);
  } catch (err) {
    console.error(err);
    setError(`Failed to generate ${mockName}. Check backend logs.`);
  } finally {
    setIsLoading(false);
  }
};


  const generateNewSimulation = async () => {
    await runMock(selectedMock);
  };

  const stepForward = () => {
    if (currentFrame < allRounds.length - 1) {
      const nextFrame = currentFrame + 1;
      setCurrentFrame(nextFrame);
      setSimState(allRounds[nextFrame]);
    } else {
      setIsPlaying(false);
    }
  };

  const stepBack = () => {
    if (currentFrame > 0) {
      const prevFrame = currentFrame - 1;
      setCurrentFrame(prevFrame);
      setSimState(allRounds[prevFrame]);
    }
  };

  const jumpToFrame = (frame) => {
    const safeFrame = Math.max(0, Math.min(Number(frame), allRounds.length - 1));
    setCurrentFrame(safeFrame);
    setSimState(allRounds[safeFrame]);
  };

  const stats = useMemo(() => {
    const bakeries = simState.bakeries || [];
    const customers = simState.customers || [];
    const drones = simState.drones || [];

    const inventory = bakeries.reduce((sum, b) => sum + Number(b.inventory || 0), 0);
    const capacity = bakeries.reduce((sum, b) => sum + Number(b.capacity || 0), 0);
    const openDemand = customers.reduce((sum, c) => sum + Number(c.demand || 0), 0);

    const served = customers.filter((c) => {
      const s = normalizeStatus(c.status);
      return s === 'served' || s === 'departed';
    }).length;

    const active = customers.filter((c) => normalizeStatus(c.status) === 'active').length;
    const departed = customers.filter((c) => normalizeStatus(c.status) === 'departed').length;

    const busy = drones.filter(
      (d) =>
        d.currentCustomer ||
        d.currentCustomerId ||
        d.targetCustomerId > 0 ||
        String(d.status || '').toLowerCase().includes('deliver')
    ).length;

    return {
      inventory,
      capacity,
      openDemand,
      served,
      active,
      departed,
      busy,
      totalCustomers: customers.length,
      totalDrones: drones.length,
    };
  }, [simState]);

  return (
    <div className="app-shell">
      <header className="hero-card">
        <div className="hero-copy">
          <div className="eyebrow">OpenMP Bread Delivery Simulation</div>
          <h1>Parallel Drone Delivery</h1>
          <p>
            Real simulation playback from your backend: bakeries produce bread, customers create demand,
            and drones execute Stage 3 / 3.5 routing decisions over time.
          </p>

          <div className="mock-buttons">
            <button
              className={selectedMock === 'mock1' ? 'active' : ''}
              onClick={() => runMock('mock1')}
              disabled={isLoading}
            >
              Mock 1: Balanced
            </button>

            <button
              className={selectedMock === 'mock2' ? 'active' : ''}
              onClick={() => runMock('mock2')}
              disabled={isLoading}
            >
              Mock 2: Split Order
            </button>

            <button
              className={selectedMock === 'mock3' ? 'active' : ''}
              onClick={() => runMock('mock3')}
              disabled={isLoading}
            >
              Mock 3: Multi Stop
            </button>
          </div>
        </div>

        <ControlPanel
          onRefresh={fetchSimulationData}
          onGenerate={generateNewSimulation}
          onNextRound={stepForward}
          onPrevRound={stepBack}
          isPlaying={isPlaying}
          togglePlay={() => setIsPlaying((value) => !value)}
          isLoading={isLoading}
          currentFrame={currentFrame}
          totalFrames={allRounds.length}
          onJump={jumpToFrame}
          playSpeed={playSpeed}
          setPlaySpeed={setPlaySpeed}
        />
      </header>

      {error && (
        <div className={error.includes('Generating') ? 'status-banner loading' : 'status-banner error'}>
          {error}
        </div>
      )}

      <section className="metric-grid">
        <div className="metric-card accent-orange">
          <span>Round</span>
          <strong>{simState.round ?? currentFrame + 1}</strong>
          <small>
            Frame {currentFrame + 1}/{Math.max(allRounds.length, 1)}
          </small>
        </div>

        <div className="metric-card accent-green">
          <span>Total Inventory</span>
          <strong>{stats.inventory}</strong>
          <small>
            {stats.capacity ? `${Math.round((stats.inventory / stats.capacity) * 100)}% storage used` : 'No capacity data'}
          </small>
        </div>

        <div className="metric-card accent-blue">
          <span>Open Demand</span>
          <strong>{stats.openDemand}</strong>
          <small>{stats.active} active customers</small>
        </div>

        <div className="metric-card accent-purple">
          <span>Drones Busy</span>
          <strong>
            {stats.busy}/{stats.totalDrones}
          </strong>
          <small>Currently assigned</small>
        </div>

        <div className="metric-card accent-slate">
          <span>Served / Completed</span>
          <strong>{stats.served}</strong>
          <small>{stats.departed} served + left</small>
        </div>
      </section>

      <main className="dashboard-grid">
        <section className="panel map-panel">
          <div className="panel-header">
            <div>
              <h2>Live Tracking Map</h2>
              <p>Scaled automatically from C coordinates. Departed customers are shown as successful completions.</p>
            </div>

            <div className="legend-row">
              <span>
                <i className="legend-dot bakery" />
                Bakery
              </span>
              <span>
                <i className="legend-dot customer" />
                Customer
              </span>
              <span>
                <i className="legend-dot drone" />
                Drone
              </span>
            </div>
          </div>

        <SimulationMap
          key={`${selectedMock}-${currentFrame}-${JSON.stringify(simState.customers)}-${JSON.stringify(simState.drones)}`}
          bakeries={simState.bakeries}
          customers={simState.customers}
          drones={simState.drones}
          selectedMock={selectedMock}
        />
        </section>

        <aside className="side-stack">
          <section className="panel compact-panel">
            <div className="panel-header slim">
              <h2>Bakery Logistics</h2>
            </div>
            <BakeryStats bakeries={simState.bakeries} />
          </section>

          <section className="panel compact-panel fill-panel">
            <div className="panel-header slim">
              <h2>Customer Demand Queue</h2>
            </div>
            <CustomerTable customers={simState.customers} />
          </section>
        </aside>
      </main>
    </div>
  );
}

export default App;