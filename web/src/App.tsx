import { useState, useEffect, useRef, useCallback } from 'react';
import 'katex/dist/katex.min.css';
import { BlockMath, InlineMath } from 'react-katex';
import { WebGLCanvas } from './WebGLCanvas';
// @ts-ignore
import initWasm from './arctic_wasm.js';
// @ts-ignore
import wasmUrl from './arctic_wasm.wasm?url';

// --- Intersection Observer hook for reveal-on-scroll ---
function useReveal() {
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) {
          el.classList.add('visible');
        }
      },
      { threshold: 0.15 }
    );
    observer.observe(el);
    return () => observer.disconnect();
  }, []);
  return ref;
}

function RevealDiv({ className = '', delay = 0, children }: { className?: string; delay?: number; children: React.ReactNode }) {
  const ref = useReveal();
  const delayClass = delay > 0 ? ` reveal-delay-${delay}` : '';
  return <div ref={ref} className={`reveal${delayClass} ${className}`}>{children}</div>;
}

// --- Small reusable components ---

function Metric({ label, value, color }: { label: string; value: string; color?: string }) {
  return (
    <div className="metric-card">
      <div className="text-[10px] uppercase tracking-wider text-text-tertiary font-mono mb-1">{label}</div>
      <div className={`font-mono text-sm ${color || 'text-text-primary'}`}>{value}</div>
    </div>
  );
}

function SectionNumber({ n }: { n: string }) {
  return <span className="text-text-tertiary font-mono text-xs tracking-widest">{n}</span>;
}

// =============================================================================
// APP
// =============================================================================

export default function App() {
  const [wasmEngine, setWasmEngine] = useState<any>(null);
  const [wasmMemory, setWasmMemory] = useState<any>(null);
  const [liveSigma, setLiveSigma] = useState(0.1);
  const [isStressed, setIsStressed] = useState(false);
  const [metrics, setMetrics] = useState({
    currentV: 0,
    boundaryA: 1,
    boundaryB: 1,
    pWin: 0.5,
    signalDecay: 1,
    signalMean: 0,
    signalVar: 0,
    theoreticalVar: 0.25,
  });
  const latencyBufferRef = useRef<Float32Array | null>(null);

  // Boot WASM
  useEffect(() => {
    (async () => {
      try {
        const module = await initWasm({
          locateFile: (path: string) => path.endsWith('.wasm') ? wasmUrl : path,
        });
        const engine = new module.ArcticWasmEngine(1000, 0.01);
        const ptr = module._malloc(4);
        engine.bind_latency_buffer(ptr);
        latencyBufferRef.current = new Float32Array(module.wasmMemory.buffer, ptr, 1);
        latencyBufferRef.current[0] = 0.1;
        setWasmMemory(module.wasmMemory);
        setWasmEngine(engine);
      } catch (err) {
        console.error('WASM init failed:', err);
      }
    })();
  }, []);

  // Latency sigma simulation
  useEffect(() => {
    const interval = setInterval(() => {
      if (!latencyBufferRef.current) return;
      let s = liveSigma;
      if (isStressed) {
        s = Math.min(1.5, s + 0.1 + Math.random() * 0.2);
      } else {
        s = Math.max(0.05, s - 0.05 - Math.random() * 0.02);
      }
      latencyBufferRef.current[0] = s;
      setLiveSigma(s);
    }, 100);
    return () => clearInterval(interval);
  }, [isStressed, liveSigma]);

  // Metrics readback
  useEffect(() => {
    if (!wasmEngine) return;
    const interval = setInterval(() => {
      try {
        setMetrics({
          currentV: wasmEngine.get_current_v(),
          boundaryA: wasmEngine.get_boundary_a_val(),
          boundaryB: wasmEngine.get_boundary_b_val(),
          pWin: wasmEngine.get_p_win(),
          signalDecay: wasmEngine.get_signal_decay(),
          signalMean: wasmEngine.get_signal_mean(),
          signalVar: wasmEngine.get_signal_variance(),
          theoreticalVar: wasmEngine.get_theoretical_variance(),
        });
      } catch {}
    }, 200);
    return () => clearInterval(interval);
  }, [wasmEngine]);

  const stressDown = useCallback(() => setIsStressed(true), []);
  const stressUp = useCallback(() => setIsStressed(false), []);

  return (
    <div className="snap-container">

      {/* ================================================================= */}
      {/* SECTION 1 -- HERO                                                  */}
      {/* ================================================================= */}
      <section className="snap-section flex-col px-6">
        <div className="max-w-3xl text-center">
          <RevealDiv>
            <div className="tag mb-8">C++ / WASM / WebGL</div>
          </RevealDiv>

          <RevealDiv delay={1}>
            <h1 className="text-5xl md:text-7xl font-semibold tracking-tight text-text-primary leading-[1.1] mb-6">
              ARCTIC
            </h1>
          </RevealDiv>

          <RevealDiv delay={2}>
            <p className="text-lg md:text-xl text-text-secondary font-light leading-relaxed max-w-2xl mx-auto mb-4">
              Flat-array limit order book, Ornstein-Uhlenbeck signal process, and dominant strategy equilibrium solver -- compiled to WebAssembly and rendered with WebGL at 60fps.
            </p>
          </RevealDiv>

          <RevealDiv delay={3}>
            <p className="text-sm text-text-tertiary font-mono max-w-xl mx-auto">
              Pool-allocated matching engine with O(1) add/cancel. Lock-free SPSC ring buffers. RDTSCP hardware timestamps. Zero heap allocations in the hot path.
            </p>
          </RevealDiv>
        </div>

        <RevealDiv delay={4} className="absolute bottom-10 text-text-tertiary text-xs font-mono tracking-widest animate-pulse">
          SCROLL
        </RevealDiv>
      </section>

      {/* ================================================================= */}
      {/* SECTION 2 -- WHAT WE BUILT                                         */}
      {/* ================================================================= */}
      <section className="snap-section flex-col px-6 py-20">
        <div className="max-w-5xl w-full">
          <RevealDiv className="mb-12">
            <SectionNumber n="01" />
            <h2 className="text-3xl md:text-4xl font-semibold text-text-primary mt-2">What this project contains</h2>
          </RevealDiv>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-5">
            <RevealDiv delay={1}>
              <div className="card p-6 h-full">
                <div className="text-xs font-mono text-accent uppercase tracking-wider mb-3">Matching Engine</div>
                <p className="text-sm text-text-secondary leading-relaxed mb-4">
                  Flat-array order book indexed by integer tick. Orders stored in a pool with intrusive doubly-linked lists per price level. No <code>std::map</code>, no tree traversal -- O(1) insert and cancel.
                </p>
                <div className="text-xs font-mono text-text-tertiary">
                  65,536 order pool / 10,000 price levels
                </div>
              </div>
            </RevealDiv>

            <RevealDiv delay={2}>
              <div className="card p-6 h-full">
                <div className="text-xs font-mono text-boundary-cyan uppercase tracking-wider mb-3">OU Process Simulator</div>
                <p className="text-sm text-text-secondary leading-relaxed mb-4">
                  Continuous-time Ornstein-Uhlenbeck signal with the exact transition kernel -- not Euler-Maruyama. Precomputed <code>exp(-theta*dt)</code> and conditional standard deviation. Welford online variance tracking.
                </p>
                <div className="text-xs font-mono text-text-tertiary">
                  Zero discretization bias
                </div>
              </div>
            </RevealDiv>

            <RevealDiv delay={3}>
              <div className="card p-6 h-full">
                <div className="text-xs font-mono text-boundary-magenta uppercase tracking-wider mb-3">Equilibrium Solver</div>
                <p className="text-sm text-text-secondary leading-relaxed mb-4">
                  Computes the Nash indifference boundary for two competing agents under LogNormal latency. The boundary is a dominant strategy: it does not depend on the opponent's threshold, only on their variance.
                </p>
                <div className="text-xs font-mono text-text-tertiary">
                  Verified via fixed-point convergence check
                </div>
              </div>
            </RevealDiv>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-5 mt-5">
            <RevealDiv delay={1}>
              <div className="card p-6 h-full">
                <div className="text-xs font-mono text-signal-amber uppercase tracking-wider mb-3">WASM Bridge</div>
                <p className="text-sm text-text-secondary leading-relaxed">
                  C++ compiled to WebAssembly via Emscripten. JS reads OU signal and boundary arrays directly from the WASM linear memory heap via raw pointers -- no serialization, no copying. Live latency variance written to a <code>SharedArrayBuffer</code> and read via <code>std::atomic</code> with <code>memory_order_acquire</code>.
                </p>
              </div>
            </RevealDiv>

            <RevealDiv delay={2}>
              <div className="card p-6 h-full">
                <div className="text-xs font-mono text-signal-green uppercase tracking-wider mb-3">Infrastructure</div>
                <p className="text-sm text-text-secondary leading-relaxed">
                  Lock-free SPSC ring buffer with power-of-two masking. Cache-line aligned atomics (<code>alignas(64)</code>) to prevent false sharing. RDTSCP hardware timestamps with median-filtered calibration. KS-test validation confirming loopback jitter deviates from LogNormal (expected -- heavy tails from OS scheduling).
                </p>
              </div>
            </RevealDiv>
          </div>
        </div>
      </section>

      {/* ================================================================= */}
      {/* SECTION 3 -- MATH                                                  */}
      {/* ================================================================= */}
      <section className="snap-section flex-col px-6 py-20">
        <div className="max-w-4xl w-full">
          <RevealDiv className="mb-12">
            <SectionNumber n="02" />
            <h2 className="text-3xl md:text-4xl font-semibold text-text-primary mt-2">Implemented math</h2>
            <p className="text-sm text-text-secondary mt-3 max-w-xl">These are the formulas used in the C++ engine. Each one maps to a function in the codebase.</p>
          </RevealDiv>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            <RevealDiv delay={1}>
              <div className="card p-6">
                <div className="text-xs font-mono text-text-tertiary uppercase tracking-wider mb-4">Exact OU Transition Kernel</div>
                <div className="bg-bg rounded-lg p-4 overflow-x-auto">
                  <BlockMath math={String.raw`V_{t+\Delta t} \sim \mathcal{N}\!\left(\mu + (V_t - \mu)e^{-\theta \Delta t},\; \frac{\sigma_V^2}{2\theta}(1 - e^{-2\theta \Delta t})\right)`} />
                </div>
                <p className="text-xs text-text-tertiary mt-3">
                  Used in <code>wasm_core.cpp</code> and <code>ou_sampler.hpp</code>. Draws from the analytically exact conditional Gaussian instead of discretizing the SDE.
                </p>
              </div>
            </RevealDiv>

            <RevealDiv delay={2}>
              <div className="card p-6">
                <div className="text-xs font-mono text-text-tertiary uppercase tracking-wider mb-4">Latency Race Probability</div>
                <div className="bg-bg rounded-lg p-4 overflow-x-auto">
                  <BlockMath math={String.raw`P(\delta_A < \delta_B) = \Phi\!\left(\frac{\sigma_B^2 - \sigma_A^2}{2\sqrt{\sigma_A^2 + \sigma_B^2}}\right)`} />
                </div>
                <p className="text-xs text-text-tertiary mt-3">
                  Implemented in <code>math_utils.cpp</code> as <code>compute_p_win()</code>. Follows from the ratio of two LogNormals sharing the same location parameter.
                </p>
              </div>
            </RevealDiv>

            <RevealDiv delay={3}>
              <div className="card p-6">
                <div className="text-xs font-mono text-text-tertiary uppercase tracking-wider mb-4">Signal Decay Under Latency</div>
                <div className="bg-bg rounded-lg p-4 overflow-x-auto">
                  <BlockMath math={String.raw`\mathbb{E}[V_{t+\delta} \mid V_t = b] = \mu + (b - \mu) \cdot e^{-\theta \cdot \mathbb{E}[\delta]}`} />
                </div>
                <p className="text-xs text-text-tertiary mt-3">
                  With <InlineMath math={String.raw`\delta \sim \text{LogNormal}(\mu_\delta, \sigma^2)`} />, expected latency is <InlineMath math={String.raw`e^{\mu_\delta + \sigma^2/2}`} />. The signal mean-reverts during the transit delay.
                </p>
              </div>
            </RevealDiv>

            <RevealDiv delay={4}>
              <div className="card p-6">
                <div className="text-xs font-mono text-text-tertiary uppercase tracking-wider mb-4">Nash Indifference Boundary</div>
                <div className="bg-bg rounded-lg p-4 overflow-x-auto">
                  <BlockMath math={String.raw`b^* = \mu + \frac{c - \mu}{P(\text{win}) \cdot e^{-\theta \cdot \mathbb{E}[\delta]}}`} />
                </div>
                <p className="text-xs text-text-tertiary mt-3">
                  Implemented in <code>compute_equilibrium_boundary()</code>. This is a dominant strategy -- b* depends only on the agent's own latency variance, not the opponent's threshold.
                </p>
              </div>
            </RevealDiv>
          </div>
        </div>
      </section>

      {/* ================================================================= */}
      {/* SECTION 4 -- LIVE SIMULATION                                       */}
      {/* ================================================================= */}
      <section className="snap-section" style={{ minHeight: '100vh', scrollSnapAlign: 'start' }}>
        <div className="w-full h-screen flex gap-0">
          
          {/* Left telemetry panel */}
          <div className="w-80 flex-shrink-0 bg-surface border-r border-border-subtle flex flex-col p-6 overflow-y-auto">
            <div className="text-xs font-mono text-text-tertiary uppercase tracking-widest mb-6">Live Telemetry</div>

            <div className="mb-5">
              <div className="text-[10px] uppercase tracking-wider text-text-tertiary font-mono mb-1">Latency Sigma (Local)</div>
              <div className="flex items-baseline gap-2">
                <span className={`text-3xl font-mono tabular-nums ${liveSigma > 0.2 ? 'text-signal-red' : 'text-text-primary'}`}>
                  {liveSigma.toFixed(4)}
                </span>
              </div>
            </div>

            <div className="mb-6">
              <div className="text-[10px] uppercase tracking-wider text-text-tertiary font-mono mb-1">Competitor Sigma (Fixed)</div>
              <span className="text-xl font-mono text-text-secondary">0.2000</span>
            </div>

            <div className="divider mb-5" />

            <div className="grid grid-cols-2 gap-3 mb-5">
              <Metric label="b*_A (ours)" value={metrics.boundaryA.toFixed(4)} />
              <Metric label="b*_B (theirs)" value={metrics.boundaryB.toFixed(4)} />
              <Metric label="P(win)" value={`${(metrics.pWin * 100).toFixed(1)}%`} color={metrics.pWin < 0.5 ? 'text-signal-red' : 'text-signal-green'} />
              <Metric label="Decay" value={metrics.signalDecay.toFixed(4)} />
              <Metric label="V(t)" value={metrics.currentV.toFixed(4)} />
              <Metric label="Var emp/theo" value={`${metrics.signalVar.toFixed(2)}/${metrics.theoreticalVar.toFixed(2)}`} />
            </div>

            <div className="divider mb-5" />

            <div className="mb-4">
              <div className="flex gap-4 text-[10px] font-mono">
                <span className="flex items-center gap-1.5"><span className="w-2.5 h-0.5 rounded bg-boundary-cyan inline-block" /> b*_A</span>
                <span className="flex items-center gap-1.5"><span className="w-2.5 h-0.5 rounded bg-boundary-magenta inline-block" /> b*_B</span>
                <span className="flex items-center gap-1.5"><span className="w-2.5 h-0.5 rounded bg-text-primary inline-block" /> V(t)</span>
              </div>
            </div>

            <button
              onMouseDown={stressDown}
              onMouseUp={stressUp}
              onMouseLeave={stressUp}
              className={`mt-auto py-3 rounded-lg font-mono text-xs uppercase tracking-wider transition-all duration-150 border ${
                isStressed
                  ? 'bg-signal-red/20 border-signal-red text-signal-red'
                  : 'bg-transparent border-border text-text-tertiary hover:border-text-tertiary hover:text-text-secondary'
              }`}
            >
              {isStressed ? 'Inducing Latency...' : 'Hold to Stress Network'}
            </button>
          </div>

          {/* WebGL canvas */}
          <div className="flex-1 relative">
            {wasmEngine ? (
              <WebGLCanvas wasmEngine={wasmEngine} wasmMemory={wasmMemory} numPoints={1000} />
            ) : (
              <div className="w-full h-full flex items-center justify-center bg-bg">
                <div className="text-text-tertiary font-mono text-sm animate-pulse">
                  Loading WASM engine...
                </div>
              </div>
            )}
          </div>
        </div>
      </section>

      {/* ================================================================= */}
      {/* SECTION 5 -- ARCHITECTURE DETAILS                                  */}
      {/* ================================================================= */}
      <section className="snap-section flex-col px-6 py-20">
        <div className="max-w-5xl w-full">
          <RevealDiv className="mb-12">
            <SectionNumber n="03" />
            <h2 className="text-3xl md:text-4xl font-semibold text-text-primary mt-2">Implementation details</h2>
          </RevealDiv>

          <div className="space-y-4">
            <RevealDiv delay={1}>
              <div className="card p-6 flex flex-col md:flex-row gap-6">
                <div className="md:w-40 flex-shrink-0">
                  <div className="font-mono text-xs text-accent uppercase tracking-wider">order_book.hpp</div>
                  <div className="text-xs text-text-tertiary mt-1">315 lines</div>
                </div>
                <div className="flex-1">
                  <p className="text-sm text-text-secondary leading-relaxed">
                    Orders are 32-byte aligned structs stored in a contiguous pool of 65,536 slots. Each price level maintains head/tail indices into the pool forming an intrusive doubly-linked list. The book tracks <code>best_bid_</code> and <code>best_ask_</code> as integer tick indices, so finding the top of book is an array lookup, not a tree search. Matching walks the resting queue at a given level and produces a fill array capped at 1,024 entries per call.
                  </p>
                </div>
              </div>
            </RevealDiv>

            <RevealDiv delay={2}>
              <div className="card p-6 flex flex-col md:flex-row gap-6">
                <div className="md:w-40 flex-shrink-0">
                  <div className="font-mono text-xs text-boundary-cyan uppercase tracking-wider">spsc_buffer.hpp</div>
                  <div className="text-xs text-text-tertiary mt-1">71 lines</div>
                </div>
                <div className="flex-1">
                  <p className="text-sm text-text-secondary leading-relaxed">
                    Single-producer single-consumer ring buffer. Capacity must be a power of two so the modulo operation reduces to a bitwise AND mask. Head and tail are <code>std::atomic</code> with acquire/release semantics -- no mutex, no lock, no syscall. Producer stores with <code>memory_order_release</code>, consumer loads with <code>memory_order_acquire</code>.
                  </p>
                </div>
              </div>
            </RevealDiv>

            <RevealDiv delay={3}>
              <div className="card p-6 flex flex-col md:flex-row gap-6">
                <div className="md:w-40 flex-shrink-0">
                  <div className="font-mono text-xs text-signal-amber uppercase tracking-wider">tsc_clock.hpp</div>
                  <div className="text-xs text-text-tertiary mt-1">120 lines</div>
                </div>
                <div className="flex-1">
                  <p className="text-sm text-text-secondary leading-relaxed">
                    Reads the CPU cycle counter via RDTSCP, which serializes instruction execution before the read. Calibration runs 5 rounds of 50ms sleeps, measures TSC ticks per wall-clock nanosecond, sorts, and takes the median to reject outliers from context switches. Also exposes <code>rdtsc_fenced()</code> as an alternative using LFENCE for load serialization.
                  </p>
                </div>
              </div>
            </RevealDiv>

            <RevealDiv delay={4}>
              <div className="card p-6 flex flex-col md:flex-row gap-6">
                <div className="md:w-40 flex-shrink-0">
                  <div className="font-mono text-xs text-boundary-magenta uppercase tracking-wider">wasm_core.cpp</div>
                  <div className="text-xs text-text-tertiary mt-1">204 lines</div>
                </div>
                <div className="flex-1">
                  <p className="text-sm text-text-secondary leading-relaxed">
                    The simulation engine compiled to WASM via Emscripten. Exposes <code>ArcticWasmEngine</code> with <code>emscripten::bind</code>. JavaScript receives raw <code>uintptr_t</code> pointers to the internal <code>float</code> arrays and constructs <code>Float32Array</code> views directly over WASM linear memory. The WebGL render loop reads these views each frame with zero intermediate allocation.
                  </p>
                </div>
              </div>
            </RevealDiv>
          </div>
        </div>
      </section>

      {/* ================================================================= */}
      {/* FOOTER                                                             */}
      {/* ================================================================= */}
      <div className="py-12 text-center border-t border-border-subtle">
        <p className="text-xs font-mono text-text-tertiary">ARCTIC // C++ / Emscripten / WebGL</p>
      </div>

    </div>
  );
}
