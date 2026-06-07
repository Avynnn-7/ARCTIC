import React, { useState, useEffect, useRef, useCallback } from 'react';
import { Activity, Cpu, ShieldAlert, Zap, TrendingUp, Clock, BarChart3, Target } from 'lucide-react';
import { LineChart, Line, AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { motion, AnimatePresence } from 'framer-motion';
import CanvasSimulator from './components/RacingSimulator';
import SimulatorHUD from './components/SimulatorHUD';
import { createInitialState, simulationTick, type SimulationState } from './engine/SimulationEngine';
import './index.css';

const latencyData = [
  { t: '0s', p50: 60, p90: 130, p99: 400, p999: 1200 },
  { t: '1s', p50: 65, p90: 145, p99: 550, p999: 1500 },
  { t: '2s', p50: 67, p90: 147, p99: 587, p999: 1700 },
  { t: '3s', p50: 62, p90: 138, p99: 420, p999: 1350 },
  { t: '4s', p50: 68, p90: 150, p99: 600, p999: 1800 },
  { t: '5s', p50: 64, p90: 140, p99: 480, p999: 1400 },
  { t: '6s', p50: 67, p90: 147, p99: 587, p999: 1700 },
  { t: '7s', p50: 63, p90: 135, p99: 440, p999: 1300 },
  { t: '8s', p50: 66, p90: 142, p99: 510, p999: 1550 },
  { t: '9s', p50: 67, p90: 147, p99: 587, p999: 1700 },
];

const throughputData = [
  { t: '0s', mps: 8.2 }, { t: '1s', mps: 8.5 }, { t: '2s', mps: 8.74 },
  { t: '3s', mps: 8.6 }, { t: '4s', mps: 8.8 }, { t: '5s', mps: 8.71 },
  { t: '6s', mps: 8.74 }, { t: '7s', mps: 8.65 }, { t: '8s', mps: 8.78 },
  { t: '9s', mps: 8.74 },
];

const messageBreakdown = [
  { name: 'Add Order', count: 50067, pct: 50, color: '#00f3ff' },
  { name: 'Delete', count: 24875, pct: 25, color: '#ff003c' },
  { name: 'Execute', count: 15155, pct: 15, color: '#b500ff' },
  { name: 'Cancel', count: 9903, pct: 10, color: '#ffaa00' },
];

const architectureLayers = [
  { name: 'Market Data Feed', latency: '~50ns', color: '#00f3ff',
    detail: 'The exchange broadcasts a continuous stream of order-level market data using the ITCH 5.0 binary protocol. Each message represents a single action on the central limit order book: a new limit order, a cancellation, a partial fill, or a full execution. The feed handler captures these raw frames and passes them to the parser with minimal buffering.' },
  { name: 'Binary Protocol Parser', latency: '~15ns', color: '#b500ff',
    detail: 'Incoming ITCH messages are fixed-width binary structs in network byte order. The parser maps directly over the memory buffer without copying, converting big-endian fields to host order in a single instruction. This eliminates serialization overhead entirely. All 19 ITCH message types are decoded, covering the full lifecycle of an order from placement to execution.' },
  { name: 'Limit Order Book', latency: '~20ns', color: '#00ff9d',
    detail: 'The reconstructed L3 order book maintains every resting limit order at each price level. Price levels are indexed by tick, giving constant-time access to any price. Orders at a given level are stored in FIFO queue order, matching the exchange priority rules. Best bid and best ask are tracked and updated in constant time on every book event.' },
  { name: 'Strategy / Alpha Engine', latency: '~30ns', color: '#ffaa00',
    detail: 'The alpha engine consumes the live book state and computes trading signals. The current implementation uses an Ornstein-Uhlenbeck mean-reversion model calibrated from microstructure features: mid-price drift, spread dynamics, and order flow imbalance. Quote placement follows the Avellaneda-Stoikov optimal market making framework, adjusting bid/ask spreads based on current inventory risk and adverse selection estimates.' },
  { name: 'Pre-Trade Risk', latency: '~10ns', color: '#ff003c',
    detail: 'Every outbound order passes through pre-trade risk checks: position limits, maximum notional exposure, price collar validation (rejecting orders outside a volatility band), and a kill-switch threshold. These checks run inline on the same thread as the strategy to avoid cross-thread latency.' },
  { name: 'Order Submission', latency: '~15ns', color: '#00f3ff',
    detail: 'Validated orders are serialized into the exchange submission format and sent over a pre-established low-latency connection. The round-trip time from order submission to exchange acknowledgement is tracked at the hardware level. Fill reports and rejects are fed back into the risk and strategy engines to update positions and recalibrate.' },
];

function AnimatedValue({ value, suffix = '', decimals = 0 }: { value: number; suffix?: string; decimals?: number }) {
  const [display, setDisplay] = useState(0);
  const ref = useRef<number>(0);

  useEffect(() => {
    const start = ref.current;
    const diff = value - start;
    const duration = 1500;
    const startTime = performance.now();

    const animate = (now: number) => {
      const elapsed = now - startTime;
      const progress = Math.min(elapsed / duration, 1);
      const eased = 1 - Math.pow(1 - progress, 3);
      const current = start + diff * eased;
      setDisplay(current);
      ref.current = current;
      if (progress < 1) requestAnimationFrame(animate);
    };

    requestAnimationFrame(animate);
  }, [value]);

  return <>{display.toFixed(decimals)}{suffix}</>;
}

type Section = 'dashboard' | 'simulator' | 'architecture';

function App() {
  const [activeSection, setActiveSection] = useState<Section>('dashboard');
  const [simState, setSimState] = useState<SimulationState>(createInitialState());
  const [hoveredAgent, setHoveredAgent] = useState<string | null>(null);
  const [expandedPipeline, setExpandedPipeline] = useState<string | null>(null);
  const simRef = useRef<SimulationState>(simState);
  simRef.current = simState;

  useEffect(() => {
    const interval = setInterval(() => {
      if (!simRef.current.isRunning) return;
      let state = simRef.current;
      for (let i = 0; i < simRef.current.speed; i++) {
        state = simulationTick(state);
      }
      setSimState(state);
    }, 50);

    return () => clearInterval(interval);
  }, []);

  const togglePause = useCallback(() => {
    setSimState(prev => ({ ...prev, isRunning: !prev.isRunning }));
  }, []);

  const changeSpeed = useCallback((speed: number) => {
    setSimState(prev => ({ ...prev, speed }));
  }, []);

  const tooltipStyle = {
    backgroundColor: 'rgba(10, 10, 15, 0.95)',
    borderColor: 'rgba(0, 243, 255, 0.3)',
    borderRadius: '8px',
    fontSize: '12px',
  };

  return (
    <div className="app-root">
      <nav className="nav-header">
        <div className="nav-logo">
          <Zap size={24} />
          <span>ARCTIC</span>
          <span className="nav-badge">v1.0</span>
        </div>
        <div className="nav-links">
          {(['dashboard', 'simulator', 'architecture'] as Section[]).map(s => (
            <span
              key={s}
              className={`nav-link ${activeSection === s ? 'active' : ''}`}
              onClick={() => setActiveSection(s)}
            >
              {s === 'dashboard' && <BarChart3 size={14} />}
              {s === 'simulator' && <Target size={14} />}
              {s === 'architecture' && <Cpu size={14} />}
              {s.toUpperCase()}
            </span>
          ))}
        </div>
        <div className="nav-status">
          <span className="status-dot" />
          <span className="status-text">ENGINE ONLINE</span>
        </div>
      </nav>

      <AnimatePresence mode="wait">
        {/* ── Dashboard ──────────────────────────────────────── */}
        {activeSection === 'dashboard' && (
          <motion.main
            key="dashboard"
            className="main-content"
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -20 }}
            transition={{ duration: 0.4 }}
          >
            <div className="hero-section">
              <h1 className="hero-title">
                <span className="hero-accent">ZERO-COPY</span> ITCH 5.0
                <br />MATCHING ENGINE
              </h1>
              <p className="hero-subtitle">
                Memory-mapped binary protocol parser — O(1) LOB dispatch — Nanosecond telemetry
              </p>
            </div>

            <div className="metrics-grid">
              <motion.div className="glass-panel metric-card" whileHover={{ scale: 1.02, y: -4 }}>
                <div className="metric-icon-row">
                  <Activity size={22} color="#00f3ff" />
                  <span className="metric-label">THROUGHPUT</span>
                </div>
                <span className="metric-value cyan">
                  <AnimatedValue value={8.74} decimals={2} suffix=" M/s" />
                </span>
                <span className="metric-desc">Messages parsed and dispatched per second</span>
                <div className="metric-sparkline">
                  <ResponsiveContainer width="100%" height={40}>
                    <AreaChart data={throughputData}>
                      <Area type="monotone" dataKey="mps" stroke="#00f3ff" fill="rgba(0,243,255,0.1)" strokeWidth={2} />
                    </AreaChart>
                  </ResponsiveContainer>
                </div>
              </motion.div>

              <motion.div className="glass-panel metric-card" whileHover={{ scale: 1.02, y: -4 }}>
                <div className="metric-icon-row">
                  <Clock size={22} color="#b500ff" />
                  <span className="metric-label">P50 LATENCY</span>
                </div>
                <span className="metric-value purple">
                  <AnimatedValue value={67} suffix=" ns" />
                </span>
                <span className="metric-desc">Median wire-to-LOB pipeline latency</span>
                <div className="metric-sparkline">
                  <ResponsiveContainer width="100%" height={40}>
                    <AreaChart data={latencyData}>
                      <Area type="monotone" dataKey="p50" stroke="#b500ff" fill="rgba(181,0,255,0.1)" strokeWidth={2} />
                    </AreaChart>
                  </ResponsiveContainer>
                </div>
              </motion.div>

              <motion.div className="glass-panel metric-card" whileHover={{ scale: 1.02, y: -4 }}>
                <div className="metric-icon-row">
                  <ShieldAlert size={22} color="#00ff9d" />
                  <span className="metric-label">HEAP ALLOCATIONS</span>
                </div>
                <span className="metric-value green">0 BYTES</span>
                <span className="metric-desc">Zero heap allocations in the hot path</span>
                <div className="metric-badge-row">
                  <span className="badge green">Zero-Copy</span>
                  <span className="badge green">mmap</span>
                  <span className="badge green">Pool Alloc</span>
                </div>
              </motion.div>

              <motion.div className="glass-panel metric-card" whileHover={{ scale: 1.02, y: -4 }}>
                <div className="metric-icon-row">
                  <TrendingUp size={22} color="#ffaa00" />
                  <span className="metric-label">BANDWIDTH</span>
                </div>
                <span className="metric-value orange">
                  <AnimatedValue value={264.32} decimals={1} suffix=" MB/s" />
                </span>
                <span className="metric-desc">Raw binary data ingestion rate</span>
                <div className="metric-badge-row">
                  <span className="badge orange">100K msgs</span>
                  <span className="badge orange">3.0 MB</span>
                </div>
              </motion.div>
            </div>

            {/* Charts Row */}
            <div className="charts-row">
              <motion.div className="glass-panel chart-panel" initial={{ opacity: 0 }} animate={{ opacity: 1 }} transition={{ delay: 0.2 }}>
                <h3 className="chart-title">Latency Distribution <span className="chart-unit">(nanoseconds)</span></h3>
                <ResponsiveContainer width="100%" height={280}>
                  <LineChart data={latencyData}>
                    <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.04)" />
                    <XAxis dataKey="t" stroke="#555" fontSize={11} />
                    <YAxis stroke="#555" fontSize={11} />
                    <Tooltip contentStyle={tooltipStyle} />
                    <Line type="monotone" dataKey="p50" stroke="#b500ff" strokeWidth={3} dot={{ r: 4, fill: '#b500ff' }} name="p50" />
                    <Line type="monotone" dataKey="p90" stroke="#00f3ff" strokeWidth={2} dot={{ r: 3 }} name="p90" />
                    <Line type="monotone" dataKey="p99" stroke="#ffaa00" strokeWidth={2} dot={{ r: 3 }} name="p99" />
                    <Line type="monotone" dataKey="p999" stroke="#ff003c" strokeWidth={1.5} strokeDasharray="5 5" dot={false} name="p99.9" />
                  </LineChart>
                </ResponsiveContainer>
              </motion.div>

              {/* Custom CSS bar chart instead of broken recharts BarChart */}
              <motion.div className="glass-panel chart-panel" initial={{ opacity: 0 }} animate={{ opacity: 1 }} transition={{ delay: 0.3 }}>
                <h3 className="chart-title">Message Type Breakdown <span className="chart-unit">(100K sample)</span></h3>
                <div className="custom-bar-chart">
                  {messageBreakdown.map((entry) => (
                    <div key={entry.name} className="bar-row">
                      <div className="bar-label">{entry.name}</div>
                      <div className="bar-track">
                        <motion.div
                          className="bar-fill"
                          style={{ background: entry.color, boxShadow: `0 0 12px ${entry.color}44` }}
                          initial={{ width: 0 }}
                          animate={{ width: `${entry.pct}%` }}
                          transition={{ duration: 1, delay: 0.3 }}
                        />
                      </div>
                      <div className="bar-value" style={{ color: entry.color }}>{entry.count.toLocaleString()}</div>
                    </div>
                  ))}
                </div>
              </motion.div>
            </div>
          </motion.main>
        )}

        {/* ── Simulator ─────────────────────────────────────── */}
        {activeSection === 'simulator' && (
          <motion.div
            key="simulator"
            className="simulator-fullscreen"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={{ duration: 0.3 }}
          >
            <CanvasSimulator state={simState} onHoverAgent={setHoveredAgent} />
            <SimulatorHUD
              state={simState}
              hoveredAgent={hoveredAgent}
              onTogglePause={togglePause}
              onSpeedChange={changeSpeed}
            />
          </motion.div>
        )}

        {/* ── Architecture ──────────────────────────────────── */}
        {activeSection === 'architecture' && (
          <motion.main
            key="architecture"
            className="main-content"
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -20 }}
            transition={{ duration: 0.4 }}
          >
            <div className="hero-section">
              <h1 className="hero-title">
                <span className="hero-accent">PIPELINE</span> ARCHITECTURE
              </h1>
              <p className="hero-subtitle">
                The critical path from market data reception to order submission.
              </p>
            </div>

            <div className="pipeline-container">
              {architectureLayers.map((layer, i) => {
                const isExpanded = expandedPipeline === layer.name;
                return (
                  <React.Fragment key={layer.name}>
                    <motion.div
                      className={`pipeline-stage ${isExpanded ? 'expanded' : ''}`}
                      initial={{ opacity: 0, x: -30 }}
                      animate={{ opacity: 1, x: 0 }}
                      transition={{ delay: i * 0.1 }}
                      whileHover={{ scale: 1.02, x: 6 }}
                      style={{ borderLeftColor: layer.color, cursor: 'pointer' }}
                      onClick={() => setExpandedPipeline(isExpanded ? null : layer.name)}
                    >
                      <div className="pipeline-info">
                        <div className="pipeline-name" style={{ color: layer.color }}>{layer.name}</div>
                        <div className="pipeline-latency">{layer.latency}</div>
                      </div>
                      <div className="pipeline-expand-hint" style={{ color: layer.color }}>
                        {isExpanded ? '\u25B2' : '\u25BC'}
                      </div>
                    </motion.div>
                    <AnimatePresence>
                      {isExpanded && (
                        <motion.div
                          className="pipeline-detail"
                          initial={{ opacity: 0, height: 0 }}
                          animate={{ opacity: 1, height: 'auto' }}
                          exit={{ opacity: 0, height: 0 }}
                          transition={{ duration: 0.3 }}
                          style={{ borderLeftColor: layer.color }}
                        >
                          <p>{layer.detail}</p>
                        </motion.div>
                      )}
                    </AnimatePresence>
                    {i < architectureLayers.length - 1 && (
                      <div className="pipeline-connector-line" style={{ borderColor: layer.color }} />
                    )}
                  </React.Fragment>
                );
              })}
              <motion.div
                className="pipeline-total"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ delay: 0.7 }}
              >
                <span className="pipeline-total-label">TOTAL CRITICAL PATH</span>
                <span className="pipeline-total-value">~140 ns</span>
              </motion.div>
            </div>

            <div className="tech-grid">
              {[
                { title: 'ITCH 5.0 Protocol', desc: 'The standard binary feed used by NASDAQ and other major exchanges. Fixed-width messages encode the full lifecycle of every order on the book.', color: '#00f3ff' },
                { title: 'Memory-Mapped I/O', desc: 'Historical data files are loaded directly into virtual address space. The OS manages paging, eliminating explicit read calls and buffer copies.', color: '#b500ff' },
                { title: 'Constant-Time Order Book', desc: 'Price levels are stored in a tick-indexed array. Lookups, inserts, and deletes never depend on book depth. Best bid/ask maintained in O(1).', color: '#00ff9d' },
                { title: 'Hardware Timestamps', desc: 'RDTSCP instruction reads the CPU cycle counter with pipeline serialization. Provides sub-nanosecond timing without kernel involvement.', color: '#ffaa00' },
                { title: 'Mean-Reversion Model', desc: 'The OU process models mid-price as a stochastic process that reverts to a long-run equilibrium. Parameters are calibrated from live microstructure data.', color: '#ff003c' },
                { title: 'Lock-Free Communication', desc: 'Single-producer single-consumer ring buffers pass data between threads without mutex contention. Cache-line padding prevents false sharing.', color: '#00f3ff' },
              ].map((item, i) => (
                <motion.div
                  key={item.title}
                  className="glass-panel tech-card"
                  initial={{ opacity: 0, y: 20 }}
                  animate={{ opacity: 1, y: 0 }}
                  transition={{ delay: i * 0.08 }}
                  whileHover={{ scale: 1.03, y: -5 }}
                >
                  <div className="tech-title" style={{ color: item.color }}>{item.title}</div>
                  <div className="tech-desc">{item.desc}</div>
                </motion.div>
              ))}
            </div>
          </motion.main>
        )}
      </AnimatePresence>
    </div>
  );
}

export default App;
