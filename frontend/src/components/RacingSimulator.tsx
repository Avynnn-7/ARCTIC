import React, { useRef, useEffect, useCallback } from 'react';
import type { SimulationState } from '../engine/SimulationEngine';

// ═══════════════════════════════════════════════════════════════════════
// Pure HTML5 Canvas 2D Racing Simulator
// No Three.js, no WebGL — guaranteed to render on any browser.
// ═══════════════════════════════════════════════════════════════════════

const COLORS: Record<string, string> = {
  alpha: '#00f3ff',
  beta: '#00d4ff',
  gamma: '#b500ff',
  delta: '#ff003c',
  epsilon: '#00ff9d',
  zeta: '#ffaa00',
};

const AGENT_SHAPES: Record<string, string> = {
  market_maker: 'diamond',
  momentum: 'triangle',
  mean_revert: 'circle',
  sniper: 'square',
};

interface CanvasSimProps {
  state: SimulationState;
  onHoverAgent: (id: string | null) => void;
}

export default function CanvasSimulator({ state, onHoverAgent }: CanvasSimProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const mouseRef = useRef({ x: 0, y: 0 });
  const frameRef = useRef<number>(0);
  const particlesRef = useRef<Particle[]>([]);

  interface Particle {
    x: number; y: number;
    vx: number; vy: number;
    life: number; maxLife: number;
    color: string; size: number;
  }

  const spawnExplosion = useCallback((x: number, y: number, color1: string, color2: string, count: number) => {
    for (let i = 0; i < count; i++) {
      const angle = (Math.PI * 2 * i) / count + Math.random() * 0.3;
      const speed = 1.5 + Math.random() * 3;
      particlesRef.current.push({
        x, y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        life: 1,
        maxLife: 30 + Math.random() * 20,
        color: Math.random() > 0.5 ? color1 : color2,
        size: 2 + Math.random() * 3,
      });
    }
  }, []);

  // Track explosions we've already spawned
  const lastExplosionCount = useRef(0);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const resize = () => {
      const rect = canvas.parentElement?.getBoundingClientRect();
      if (rect) {
        canvas.width = rect.width * window.devicePixelRatio;
        canvas.height = rect.height * window.devicePixelRatio;
        canvas.style.width = rect.width + 'px';
        canvas.style.height = rect.height + 'px';
        ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
      }
    };
    resize();
    window.addEventListener('resize', resize);

    const handleMouseMove = (e: MouseEvent) => {
      const rect = canvas.getBoundingClientRect();
      mouseRef.current = { x: e.clientX - rect.left, y: e.clientY - rect.top };
    };
    canvas.addEventListener('mousemove', handleMouseMove);

    return () => {
      window.removeEventListener('resize', resize);
      canvas.removeEventListener('mousemove', handleMouseMove);
    };
  }, []);

  // Main render loop
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Spawn new explosion particles
    if (state.explosions.length > lastExplosionCount.current) {
      const newOnes = state.explosions.slice(lastExplosionCount.current);
      const w = canvas.width / window.devicePixelRatio;
      const h = canvas.height / window.devicePixelRatio;
      const cx = w / 2;
      const cy = h / 2;
      for (const exp of newOnes) {
        const ex = cx + exp.x * 25;
        const ey = cy + exp.z * 25;
        spawnExplosion(ex, ey, exp.color1, exp.color2, 15 + Math.floor(exp.size * 8));
      }
    }
    lastExplosionCount.current = state.explosions.length;

    const draw = () => {
      const w = canvas.width / window.devicePixelRatio;
      const h = canvas.height / window.devicePixelRatio;
      ctx.setTransform(window.devicePixelRatio, 0, 0, window.devicePixelRatio, 0, 0);

      // Background
      ctx.fillStyle = '#06060a';
      ctx.fillRect(0, 0, w, h);

      // Grid
      ctx.strokeStyle = 'rgba(255,255,255,0.03)';
      ctx.lineWidth = 1;
      for (let x = 0; x < w; x += 30) {
        ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
      }
      for (let y = 0; y < h; y += 30) {
        ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
      }

      const cx = w / 2;
      const cy = h / 2;
      const radius = Math.min(w, h) * 0.32;
      const t = performance.now() / 1000;

      // Arena circle
      ctx.strokeStyle = 'rgba(0, 243, 255, 0.12)';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(cx, cy, radius, 0, Math.PI * 2);
      ctx.stroke();

      // Inner ring
      ctx.strokeStyle = 'rgba(0, 255, 157, 0.15)';
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.arc(cx, cy, radius * 0.35, 0, Math.PI * 2);
      ctx.stroke();

      // Central engine glow
      const enginePulse = 1 + Math.sin(t * 6) * 0.15;
      const engineRadius = 28 * enginePulse;
      const grd = ctx.createRadialGradient(cx, cy, 0, cx, cy, engineRadius * 3);
      grd.addColorStop(0, 'rgba(0, 255, 157, 0.3)');
      grd.addColorStop(0.5, 'rgba(0, 255, 157, 0.05)');
      grd.addColorStop(1, 'rgba(0, 255, 157, 0)');
      ctx.fillStyle = grd;
      ctx.beginPath();
      ctx.arc(cx, cy, engineRadius * 3, 0, Math.PI * 2);
      ctx.fill();

      // Central engine shape (rotating hexagon)
      ctx.save();
      ctx.translate(cx, cy);
      ctx.rotate(t * 0.5);
      ctx.strokeStyle = '#00ff9d';
      ctx.lineWidth = 2;
      ctx.shadowColor = '#00ff9d';
      ctx.shadowBlur = 20;
      ctx.beginPath();
      for (let i = 0; i < 6; i++) {
        const a = (Math.PI * 2 * i) / 6;
        const px = Math.cos(a) * engineRadius;
        const py = Math.sin(a) * engineRadius;
        if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
      }
      ctx.closePath();
      ctx.stroke();

      // Inner rotating triangle
      ctx.rotate(-t * 1.2);
      ctx.strokeStyle = 'rgba(0, 255, 157, 0.5)';
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      for (let i = 0; i < 3; i++) {
        const a = (Math.PI * 2 * i) / 3;
        const px = Math.cos(a) * engineRadius * 0.5;
        const py = Math.sin(a) * engineRadius * 0.5;
        if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
      }
      ctx.closePath();
      ctx.stroke();
      ctx.restore();
      ctx.shadowBlur = 0;

      // Engine label
      ctx.fillStyle = '#00ff9d';
      ctx.font = '10px Orbitron, monospace';
      ctx.textAlign = 'center';
      ctx.fillText('MATCHING ENGINE', cx, cy + engineRadius + 18);

      // Draw order beams (animated lines from agents to center)
      for (const beam of state.orderBeams) {
        const ax = cx + Math.cos(beam.fromAngle) * radius;
        const ay = cy + Math.sin(beam.fromAngle) * radius;

        const progress = beam.progress;
        const headX = ax + (cx - ax) * progress;
        const headY = ay + (cy - ay) * progress;
        const tailProgress = Math.max(0, progress - 0.3);
        const tailX = ax + (cx - ax) * tailProgress;
        const tailY = ay + (cy - ay) * tailProgress;

        const opacity = (1 - progress);
        ctx.strokeStyle = beam.agentColor;
        ctx.globalAlpha = opacity * 0.8;
        ctx.lineWidth = 2;
        ctx.shadowColor = beam.agentColor;
        ctx.shadowBlur = 8;
        ctx.beginPath();
        ctx.moveTo(tailX, tailY);
        ctx.lineTo(headX, headY);
        ctx.stroke();

        // Beam head dot
        ctx.fillStyle = beam.agentColor;
        ctx.beginPath();
        ctx.arc(headX, headY, 3, 0, Math.PI * 2);
        ctx.fill();

        ctx.globalAlpha = 1;
        ctx.shadowBlur = 0;
      }

      // Draw agents
      let hoveredId: string | null = null;
      const sorted = [...state.agents].sort((a, b) => b.pnl - a.pnl);

      for (const agent of state.agents) {
        const ax = cx + Math.cos(agent.angle) * radius;
        const ay = cy + Math.sin(agent.angle) * radius;
        const color = COLORS[agent.id] || agent.color;
        const energyScale = 1 + agent.energy * 0.4;
        const baseSize = 16 * energyScale;

        // Connection line to center (faint)
        ctx.strokeStyle = color;
        ctx.globalAlpha = 0.06 + agent.energy * 0.08;
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 6]);
        ctx.beginPath();
        ctx.moveTo(ax, ay);
        ctx.lineTo(cx, cy);
        ctx.stroke();
        ctx.setLineDash([]);
        ctx.globalAlpha = 1;

        // Agent glow
        const glowRad = baseSize * 2.5;
        const agentGrd = ctx.createRadialGradient(ax, ay, 0, ax, ay, glowRad);
        agentGrd.addColorStop(0, color + '30');
        agentGrd.addColorStop(1, color + '00');
        ctx.fillStyle = agentGrd;
        ctx.beginPath();
        ctx.arc(ax, ay, glowRad, 0, Math.PI * 2);
        ctx.fill();

        // Draw shape
        ctx.fillStyle = color;
        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        ctx.shadowColor = color;
        ctx.shadowBlur = 12;

        const bounce = Math.sin(t * 8 + agent.angle) * agent.energy * 4;
        const dy = ay + bounce;

        const shape = AGENT_SHAPES[agent.type] || 'circle';
        ctx.beginPath();
        if (shape === 'diamond') {
          ctx.moveTo(ax, dy - baseSize);
          ctx.lineTo(ax + baseSize * 0.7, dy);
          ctx.lineTo(ax, dy + baseSize);
          ctx.lineTo(ax - baseSize * 0.7, dy);
          ctx.closePath();
        } else if (shape === 'triangle') {
          ctx.moveTo(ax, dy - baseSize);
          ctx.lineTo(ax + baseSize * 0.85, dy + baseSize * 0.7);
          ctx.lineTo(ax - baseSize * 0.85, dy + baseSize * 0.7);
          ctx.closePath();
        } else if (shape === 'square') {
          const s = baseSize * 0.7;
          ctx.save();
          ctx.translate(ax, dy);
          ctx.rotate(t * 2);
          ctx.rect(-s, -s, s * 2, s * 2);
          ctx.restore();
        } else {
          ctx.arc(ax, dy, baseSize * 0.7, 0, Math.PI * 2);
        }
        ctx.fill();
        ctx.shadowBlur = 0;

        // Agent name
        ctx.fillStyle = color;
        ctx.font = 'bold 11px Orbitron, monospace';
        ctx.textAlign = 'center';
        ctx.fillText(agent.name, ax, dy - baseSize - 12);

        // PnL
        const pnlColor = agent.pnl >= 0 ? '#00ff9d' : '#ff003c';
        ctx.fillStyle = pnlColor;
        ctx.font = 'bold 13px Orbitron, monospace';
        ctx.fillText((agent.pnl >= 0 ? '+' : '') + agent.pnl.toFixed(0), ax, dy - baseSize - 26);

        // Trade count
        ctx.fillStyle = '#555';
        ctx.font = '9px Inter, sans-serif';
        ctx.fillText(agent.trades + ' fills', ax, dy + baseSize + 18);

        // Rank badge
        const rank = sorted.findIndex(a => a.id === agent.id) + 1;
        if (rank === 1) {
          ctx.fillStyle = '#ffaa00';
          ctx.font = 'bold 10px Orbitron, monospace';
          ctx.fillText('#1', ax, dy + baseSize + 32);
        }

        // Hover detection
        const dx = mouseRef.current.x - ax;
        const dyMouse = mouseRef.current.y - ay;
        if (Math.sqrt(dx * dx + dyMouse * dyMouse) < baseSize * 2) {
          hoveredId = agent.id;
        }
      }

      // Update and draw particles
      const particles = particlesRef.current;
      for (let i = particles.length - 1; i >= 0; i--) {
        const p = particles[i];
        p.x += p.vx;
        p.y += p.vy;
        p.vx *= 0.96;
        p.vy *= 0.96;
        p.life += 1;

        const alpha = 1 - p.life / p.maxLife;
        if (alpha <= 0) {
          particles.splice(i, 1);
          continue;
        }

        ctx.fillStyle = p.color;
        ctx.globalAlpha = alpha;
        ctx.shadowColor = p.color;
        ctx.shadowBlur = 6;
        ctx.beginPath();
        ctx.arc(p.x, p.y, p.size * alpha, 0, Math.PI * 2);
        ctx.fill();
      }
      ctx.globalAlpha = 1;
      ctx.shadowBlur = 0;

      // Price ticker at bottom center
      ctx.fillStyle = '#00ff9d';
      ctx.font = 'bold 14px Orbitron, monospace';
      ctx.textAlign = 'center';
      ctx.fillText('MID: $' + state.midPrice.toFixed(4), cx, h - 20);

      ctx.fillStyle = '#555';
      ctx.font = '10px Inter, sans-serif';
      ctx.fillText(
        'BID ' + state.bestBid.toFixed(4) + '  |  ASK ' + state.bestAsk.toFixed(4) + '  |  SPREAD ' + (state.spread * 100).toFixed(1) + 'c',
        cx, h - 6
      );

      onHoverAgent(hoveredId);
      frameRef.current = requestAnimationFrame(draw);
    };

    frameRef.current = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(frameRef.current);
  }, [state, spawnExplosion, onHoverAgent]);

  return (
    <canvas
      ref={canvasRef}
      style={{ width: '100%', height: '100%', display: 'block', cursor: 'crosshair' }}
    />
  );
}
