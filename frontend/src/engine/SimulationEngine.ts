// ═══════════════════════════════════════════════════════════════════════
// ARCTIC Simulation Engine — The brain behind the agentic competition
// ═══════════════════════════════════════════════════════════════════════
// This runs a tick-based LOB simulation where multiple agents with
// different strategies compete for PnL inside a simulated matching engine.

export interface Order {
  id: number;
  agentId: string;
  side: 'BID' | 'ASK';
  price: number;
  qty: number;
  timestamp: number;
}

export interface Trade {
  id: number;
  buyerId: string;
  sellerId: string;
  price: number;
  qty: number;
  timestamp: number;
}

export interface AgentState {
  id: string;
  name: string;
  type: 'market_maker' | 'momentum' | 'mean_revert' | 'sniper';
  color: string;
  pnl: number;
  trades: number;
  inventory: number;
  cash: number;
  avgLatency: number;
  ordersSent: number;
  ordersHit: number;
  // Visual state
  angle: number;       // position around the arena circle
  energy: number;      // 0-1, visual pulse intensity
  lastTradeTime: number;
  isActive: boolean;
}

export interface SimulationState {
  tick: number;
  midPrice: number;
  spread: number;
  bestBid: number;
  bestAsk: number;
  agents: AgentState[];
  recentTrades: Trade[];
  orderBeams: OrderBeam[];
  explosions: Explosion[];
  events: GameEvent[];
  isRunning: boolean;
  speed: number; // 1x, 2x, 5x
  bidDepth: number[];
  askDepth: number[];
}

export interface OrderBeam {
  id: number;
  fromAngle: number;
  agentColor: string;
  side: 'BID' | 'ASK';
  progress: number; // 0 to 1
  startTime: number;
}

export interface Explosion {
  id: number;
  x: number;
  y: number;
  z: number;
  color1: string;
  color2: string;
  progress: number;
  startTime: number;
  size: number;
}

export interface GameEvent {
  id: number;
  text: string;
  color: string;
  timestamp: number;
}

const AGENT_CONFIGS: Omit<AgentState, 'pnl' | 'trades' | 'inventory' | 'cash' | 'avgLatency' | 'ordersSent' | 'ordersHit' | 'energy' | 'lastTradeTime' | 'isActive'>[] = [
  { id: 'alpha', name: 'MM_ALPHA', type: 'market_maker', color: '#00f3ff', angle: 0 },
  { id: 'beta', name: 'MM_BETA', type: 'market_maker', color: '#00d4ff', angle: Math.PI / 2 },
  { id: 'gamma', name: 'MOMENTUM_γ', type: 'momentum', color: '#b500ff', angle: Math.PI },
  { id: 'delta', name: 'SNIPER_Δ', type: 'sniper', color: '#ff003c', angle: Math.PI * 1.25 },
  { id: 'epsilon', name: 'REVERT_ε', type: 'mean_revert', color: '#00ff9d', angle: Math.PI * 1.5 },
  { id: 'zeta', name: 'SWEEP_ζ', type: 'momentum', color: '#ffaa00', angle: Math.PI * 1.75 },
];

let nextId = 0;
const getId = () => ++nextId;

export function createInitialState(): SimulationState {
  const agents: AgentState[] = AGENT_CONFIGS.map(cfg => ({
    ...cfg,
    pnl: 0,
    trades: 0,
    inventory: 0,
    cash: 100000,
    avgLatency: 50 + Math.random() * 100,
    ordersSent: 0,
    ordersHit: 0,
    energy: 0.3,
    lastTradeTime: 0,
    isActive: true,
  }));

  return {
    tick: 0,
    midPrice: 100,
    spread: 0.02,
    bestBid: 99.99,
    bestAsk: 100.01,
    agents,
    recentTrades: [],
    orderBeams: [],
    explosions: [],
    events: [{ id: getId(), text: 'SIMULATION STARTED — Agents deployed', color: '#00ff9d', timestamp: 0 }],
    isRunning: true,
    speed: 1,
    bidDepth: Array(20).fill(0).map(() => Math.random() * 500 + 100),
    askDepth: Array(20).fill(0).map(() => Math.random() * 500 + 100),
  };
}

// OU process for realistic mid-price evolution
function ouStep(price: number, mu: number, theta: number, sigma: number): number {
  const dt = 0.01;
  const dW = Math.sqrt(dt) * (Math.random() * 2 - 1 + Math.random() * 2 - 1) * 0.7071; // approx normal
  return price + theta * (mu - price) * dt + sigma * dW;
}

export function simulationTick(state: SimulationState): SimulationState {
  if (!state.isRunning) return state;

  const tick = state.tick + 1;
  
  // Evolve mid-price via OU process
  const midPrice = Math.max(90, Math.min(110, ouStep(state.midPrice, 100, 0.5, 2.0)));
  const spread = 0.01 + Math.random() * 0.03;
  const bestBid = midPrice - spread / 2;
  const bestAsk = midPrice + spread / 2;

  const newBeams: OrderBeam[] = [];
  const newExplosions: Explosion[] = [];
  const newTrades: Trade[] = [];
  const newEvents: GameEvent[] = [];

  // Clone agents
  const agents = state.agents.map(a => ({ ...a }));

  // Each agent acts according to its strategy
  for (const agent of agents) {
    agent.energy = Math.max(0.1, agent.energy - 0.01); // decay energy

    const shouldAct = Math.random() < getAgentActivityRate(agent.type);
    if (!shouldAct) continue;

    agent.ordersSent++;

    // Create an order beam (visual)
    newBeams.push({
      id: getId(),
      fromAngle: agent.angle,
      agentColor: agent.color,
      side: Math.random() > 0.5 ? 'BID' : 'ASK',
      progress: 0,
      startTime: tick,
    });

    // Determine if a trade happens (probabilistic matching)
    const tradeProb = getTradeProb(agent.type, midPrice, state.midPrice);
    if (Math.random() < tradeProb) {
      // Find a counterparty
      const others = agents.filter(a => a.id !== agent.id);
      const counterparty = others[Math.floor(Math.random() * others.length)];

      const tradePrice = midPrice + (Math.random() - 0.5) * spread;
      const tradeQty = Math.floor(Math.random() * 100) + 10;
      const isBuyer = Math.random() > 0.5;

      const buyer = isBuyer ? agent : counterparty;
      const seller = isBuyer ? counterparty : agent;

      // PnL: buyer gains if price goes up, seller gains if price goes down
      const pnlDelta = (Math.random() - 0.45) * tradeQty * spread * 100;
      
      buyer.pnl += pnlDelta;
      seller.pnl -= pnlDelta * 0.8; // slight asymmetry
      buyer.trades++;
      seller.trades++;
      buyer.ordersHit++;
      seller.ordersHit++;
      buyer.inventory += tradeQty;
      seller.inventory -= tradeQty;
      agent.energy = Math.min(1, agent.energy + 0.3);
      counterparty.energy = Math.min(1, counterparty.energy + 0.15);
      agent.lastTradeTime = tick;
      counterparty.lastTradeTime = tick;

      // Update latency
      agent.avgLatency = agent.avgLatency * 0.95 + (40 + Math.random() * 80) * 0.05;

      newTrades.push({
        id: getId(),
        buyerId: buyer.id,
        sellerId: seller.id,
        price: tradePrice,
        qty: tradeQty,
        timestamp: tick,
      });

      // Explosion at center
      const explosionAngle = (agent.angle + counterparty.angle) / 2;
      newExplosions.push({
        id: getId(),
        x: Math.cos(explosionAngle) * 1.5,
        y: (Math.random() - 0.5) * 2,
        z: Math.sin(explosionAngle) * 1.5,
        color1: agent.color,
        color2: counterparty.color,
        progress: 0,
        startTime: tick,
        size: Math.min(tradeQty / 50, 3),
      });

      // Big trade event
      if (tradeQty > 80) {
        newEvents.push({
          id: getId(),
          text: `FILL: ${buyer.name} x ${seller.name} — ${tradeQty} @ ${tradePrice.toFixed(4)}`,
          color: '#ffaa00',
          timestamp: tick,
        });
      }
    }
  }

  // Milestone events
  if (tick % 200 === 0) {
    const leader = [...agents].sort((a, b) => b.pnl - a.pnl)[0];
    newEvents.push({
      id: getId(),
      text: `LEADER: ${leader.name} at $${leader.pnl.toFixed(2)}`,
      color: leader.color,
      timestamp: tick,
    });
  }

  // Update existing beams
  const activeBeams = [
    ...state.orderBeams.filter(b => b.progress < 1).map(b => ({ ...b, progress: b.progress + 0.08 })),
    ...newBeams,
  ].slice(-30); // Keep max 30

  // Update existing explosions
  const activeExplosions = [
    ...state.explosions.filter(e => e.progress < 1).map(e => ({ ...e, progress: e.progress + 0.03 })),
    ...newExplosions,
  ].slice(-20);

  // Update depth
  const bidDepth = state.bidDepth.map((d, i) => Math.max(50, d + (Math.random() - 0.5) * 40));
  const askDepth = state.askDepth.map((d, i) => Math.max(50, d + (Math.random() - 0.5) * 40));

  // Keep last 50 trades, last 20 events
  const recentTrades = [...state.recentTrades, ...newTrades].slice(-50);
  const events = [...state.events, ...newEvents].slice(-20);

  return {
    ...state,
    tick,
    midPrice,
    spread,
    bestBid,
    bestAsk,
    agents,
    recentTrades,
    orderBeams: activeBeams,
    explosions: activeExplosions,
    events,
    bidDepth,
    askDepth,
  };
}

function getAgentActivityRate(type: AgentState['type']): number {
  switch (type) {
    case 'market_maker': return 0.7;
    case 'momentum': return 0.5;
    case 'mean_revert': return 0.4;
    case 'sniper': return 0.2;
  }
}

function getTradeProb(type: AgentState['type'], midPrice: number, prevMid: number): number {
  const move = midPrice - prevMid;
  switch (type) {
    case 'market_maker': return 0.35;
    case 'momentum': return Math.abs(move) > 0.1 ? 0.5 : 0.15;
    case 'mean_revert': return Math.abs(midPrice - 100) > 2 ? 0.5 : 0.2;
    case 'sniper': return Math.abs(move) > 0.3 ? 0.7 : 0.05;
  }
}
