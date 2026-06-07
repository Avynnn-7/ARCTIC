import React from 'react';
import type { SimulationState } from '../engine/SimulationEngine';
import './SimulatorHUD.css';

interface HUDProps {
  state: SimulationState;
  hoveredAgent: string | null;
  onTogglePause: () => void;
  onSpeedChange: (speed: number) => void;
}

export default function SimulatorHUD({ state, hoveredAgent, onTogglePause, onSpeedChange }: HUDProps) {
  const sorted = [...state.agents].sort((a, b) => b.pnl - a.pnl);
  const hovered = state.agents.find(a => a.id === hoveredAgent);

  return (
    <>
      {/* ── Top Bar ────────────────────────────────────────── */}
      <div className="hud-topbar">
        <div className="hud-topbar-left">
          <span className="hud-label">TICK</span>
          <span className="hud-value cyan">{state.tick.toLocaleString()}</span>
        </div>
        <div className="hud-topbar-center">
          <span className="hud-label">MID PRICE</span>
          <span className="hud-value green">${state.midPrice.toFixed(4)}</span>
          <span className="hud-spread">spread: {(state.spread * 100).toFixed(1)}¢</span>
        </div>
        <div className="hud-topbar-right">
          <button className="hud-btn" onClick={onTogglePause}>
            {state.isRunning ? '⏸ PAUSE' : '▶ PLAY'}
          </button>
          <button className={`hud-btn-speed ${state.speed === 1 ? 'active' : ''}`} onClick={() => onSpeedChange(1)}>1×</button>
          <button className={`hud-btn-speed ${state.speed === 2 ? 'active' : ''}`} onClick={() => onSpeedChange(2)}>2×</button>
          <button className={`hud-btn-speed ${state.speed === 5 ? 'active' : ''}`} onClick={() => onSpeedChange(5)}>5×</button>
        </div>
      </div>

      {/* ── Leaderboard ────────────────────────────────────── */}
      <div className="hud-leaderboard">
        <div className="hud-panel-title">
          <span className="hud-icon">1</span> LEADERBOARD
        </div>
        {sorted.map((agent, i) => {
          const barWidth = Math.min(100, Math.max(5, ((agent.pnl + 500) / 1000) * 100));
          return (
            <div key={agent.id} className={`lb-row ${hoveredAgent === agent.id ? 'highlighted' : ''}`}>
              <div className="lb-rank">#{i + 1}</div>
              <div className="lb-dot" style={{ background: agent.color, boxShadow: `0 0 8px ${agent.color}` }} />
              <div className="lb-name" style={{ color: agent.color }}>{agent.name}</div>
              <div className="lb-bar-container">
                <div className="lb-bar" style={{
                  width: `${barWidth}%`,
                  background: `linear-gradient(90deg, ${agent.color}33, ${agent.color})`,
                  boxShadow: `0 0 10px ${agent.color}66`,
                }} />
              </div>
              <div className={`lb-pnl ${agent.pnl >= 0 ? 'positive' : 'negative'}`}>
                {agent.pnl >= 0 ? '+' : ''}{agent.pnl.toFixed(0)}
              </div>
            </div>
          );
        })}
      </div>

      {/* ── Event Log ──────────────────────────────────────── */}
      <div className="hud-eventlog">
        <div className="hud-panel-title">
          <span className="hud-icon">/</span> LIVE FEED
        </div>
        <div className="eventlog-scroll">
          {[...state.events].reverse().slice(0, 8).map(ev => (
            <div key={ev.id} className="event-row" style={{ borderLeftColor: ev.color }}>
              <span className="event-text">{ev.text}</span>
            </div>
          ))}
        </div>
      </div>

      {/* ── Agent Detail Panel (on hover) ──────────────────── */}
      {hovered && (
        <div className="hud-agent-detail" style={{ borderColor: hovered.color }}>
          <div className="agent-detail-header" style={{ color: hovered.color }}>
            {hovered.name}
          </div>
          <div className="agent-detail-type">
            {hovered.type.replace('_', ' ').toUpperCase()}
            <div className="agent-detail-desc" style={{ marginTop: '4px', fontSize: '10px', color: '#999', textTransform: 'none', lineHeight: '1.4' }}>
              {hovered.id === 'alpha' && 'Primary liquidity provider. Solves the Hamilton-Jacobi-Bellman (HJB) equation to continuously compute optimal quotes, maximizing terminal wealth under a strict inventory penalty parameter (γ).'}
              {hovered.id === 'beta' && 'Adverse selection aware market maker. Overlays order flow imbalance (OFI) signals onto baseline quoting, dynamically widening spreads or withdrawing quotes when short-term directional predictability is high.'}
              {hovered.id === 'gamma' && 'Employs a trend-following heuristic to capture alpha from persistent price drifts. Executes aggressive orders in the direction of order flow imbalance and local volatility breakouts.'}
              {hovered.id === 'delta' && 'Utilizes a low-latency aggressive execution strategy. Monitors L3 order book data to detect resting liquidity imbalances, executing IOC orders to pick off stale quotes before the market reprices.'}
              {hovered.id === 'epsilon' && 'Exploits short-term statistical arbitrage. Assumes the mid-price follows an Ornstein-Uhlenbeck process, executing trades when the price diverges significantly from its long-run equilibrium (μ).'}
              {hovered.id === 'zeta' && 'Executes aggressive liquidity-consuming sweeps. Analyzes deep order book resting volume, submitting marketable orders to clear multiple price levels when expected short-term price impact exceeds transaction costs.'}
            </div>
          </div>
          <div className="agent-detail-grid">
            <div className="detail-item">
              <span className="detail-label">PnL</span>
              <span className={`detail-value ${hovered.pnl >= 0 ? 'positive' : 'negative'}`}>
                ${hovered.pnl.toFixed(2)}
              </span>
            </div>
            <div className="detail-item">
              <span className="detail-label">Trades</span>
              <span className="detail-value">{hovered.trades}</span>
            </div>
            <div className="detail-item">
              <span className="detail-label">Inventory</span>
              <span className="detail-value">{hovered.inventory}</span>
            </div>
            <div className="detail-item">
              <span className="detail-label">Avg Latency</span>
              <span className="detail-value">{hovered.avgLatency.toFixed(0)}ns</span>
            </div>
            <div className="detail-item">
              <span className="detail-label">Orders Sent</span>
              <span className="detail-value">{hovered.ordersSent}</span>
            </div>
            <div className="detail-item">
              <span className="detail-label">Fill Rate</span>
              <span className="detail-value">
                {hovered.ordersSent > 0 ? ((hovered.ordersHit / hovered.ordersSent) * 100).toFixed(1) : '0.0'}%
              </span>
            </div>
            <div className="detail-item">
              <span className="detail-label">Energy</span>
              <div className="energy-bar">
                <div className="energy-fill" style={{
                  width: `${hovered.energy * 100}%`,
                  background: hovered.color,
                  boxShadow: `0 0 10px ${hovered.color}`,
                }} />
              </div>
            </div>
          </div>
        </div>
      )}

      {/* ── Stats Bar (Bottom) ─────────────────────────────── */}
      <div className="hud-bottombar">
        <div className="bottom-stat">
          <span className="stat-label">Total Trades</span>
          <span className="stat-value">{state.recentTrades.length}</span>
        </div>
        <div className="bottom-stat">
          <span className="stat-label">Active Agents</span>
          <span className="stat-value">{state.agents.filter(a => a.isActive).length}</span>
        </div>
        <div className="bottom-stat">
          <span className="stat-label">Best Bid</span>
          <span className="stat-value green">{state.bestBid.toFixed(4)}</span>
        </div>
        <div className="bottom-stat">
          <span className="stat-label">Best Ask</span>
          <span className="stat-value red">{state.bestAsk.toFixed(4)}</span>
        </div>
      </div>
    </>
  );
}
