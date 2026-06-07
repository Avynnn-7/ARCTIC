# ARCTIC: Agentic Racing under Competitive Timing in Continuous-time

ARCTIC is a C++ framework that simulates and resolves high-frequency algorithmic racing games using continuous-time stochastic models. The project merges Information-Based Trading (Laughlin et al., 2014) with non-cooperative game theory to identify competitive execution boundaries in the presence of microstructural network jitter.

## Core Deliverables
- **Dominant Strategy Equilibrium Solver**: Derives competitive action boundaries from the indifference condition under signal decay and latency race probability, with formal proof of the dominant strategy property.
- **Limit Order Book (LOB)**: Price-time priority matching engine with flat-array price levels, intrusive doubly-linked order lists, and a pre-allocated pool allocator. Zero heap allocation during add/cancel/match. Integrated into the Monte Carlo simulation — agents now execute via market orders against resting LOB liquidity.
- **Live OS Jitter Telemetry**: Lock-free (`std::atomic`) ring buffers (`SPSCBuffer`) that measure local RTT via UDP echo, using `epoll` (Linux) / `select` (Windows) for non-blocking timeout-based measurement. Timed via `RDTSCP` (not `clock_gettime`).
- **Custom Memory Allocators**: Bump-pointer arena (page-aligned, pre-faulted) for per-path simulation data. Intrusive free-list pool allocator for LOB order storage. Zero `malloc`/`free` in the hot path.
- **Continuous-Time Stochastic Process**: Exact-kernel Ornstein-Uhlenbeck mean-reverting signals (zero discretization bias).
- **Browser-Native Visualization**: C++ → WebAssembly with SharedArrayBuffer, WebGL rendering, and live KaTeX mathematics.
- **Statistical Instrumentation**: Per-run Sharpe ratios, stopping time distributions, lag-1 autocorrelation validation, and Monte Carlo timing benchmarks.

## Benchmark Results

Measured via [Google Benchmark](https://github.com/google/benchmark) v1.8.3 with `RDTSCP` hardware timestamps.  
CPU: 12th Gen Intel (12 cores @ 2.61 GHz) | L1d: 48 KiB × 6 | L2: 1280 KiB × 6 | L3: 12288 KiB

### SPSC Ring Buffer Latency Distribution

| Metric | Single-Thread Push/Pop | Cross-Thread |
|--------|----------------------|---------------|
| **p50** | **46 ns** | 72 μs |
| **p90** | **61 ns** | 73 μs |
| **p99** | **67 ns** | 1.52 ms |
| **p99.9** | **84 ns** | 1.53 ms |
| min | 35 ns | — |
| max | 133 ns (OS jitter) | — |

Cross-thread p50 is dominated by cache-line ownership transfer (MESI protocol `Invalid → Shared` transitions) between producer and consumer cores. The 72 μs includes L3 round-trip + store buffer drain.

### Limit Order Book (LOB) Latency

| Operation | p50 | p90 | p99 | p99.9 |
|-----------|-----|-----|-----|-------|
| **Add Order** | **94 ns** | **140 ns** | **162 ns** | **227 ns** |
| **Match Market** | **32 ns** | **62 ns** | **158 ns** | **922 ns** |

O(1) add via intrusive list append. O(k) match where k = number of fills. All operations use the pre-allocated `PoolAllocator` — zero heap allocation.

### Arena Allocator vs `std::vector` (Cache-Miss Analysis)

| Allocator | Time per 1000-double alloc | Speedup |
|-----------|--------------------------|----------|
| `ArenaAllocator` | **1.65 ns** | **71×** |
| `std::vector<double>` | 117 ns | 1× (baseline) |

**Why 71× faster — the cache-miss story:**

1. **Arena** (`ArenaAllocator::allocate`): A single pointer bump (`offset_ += size`). The backing store is page-aligned (`std::align_val_t{4096}`) and pre-faulted (`memset` at construction). Every allocation hits L1d because the bump pointer is monotonically advancing through contiguous, pre-warmed cache lines. **Zero TLB misses, zero page faults, zero syscalls.**

2. **`std::vector`**: Each construction calls `operator new` → `malloc` → kernel `brk`/`mmap` (amortized). The allocator metadata (free-list headers, size classes) lives on different cache lines than the data, causing **L1d capacity evictions**. Deallocation dirties additional cache lines for coalescing. On repeated alloc/free cycles, the DRAM access pattern becomes non-sequential, degrading hardware prefetcher effectiveness.

3. **Measurement**: On this CPU (48 KiB L1d per core), the arena's 8 KB working set (1000 doubles) fits entirely in L1d with room for the bump pointer metadata on the same cache line. `std::vector`'s allocator metadata + free-list pointers push the working set past L1d capacity, forcing L2 round-trips (~4 ns each on this microarchitecture).

### RDTSCP Overhead

| Operation | Latency |
|-----------|----------|
| `RDTSCP` read | **23.5 ns** |
| Calibrated frequency | 2.6112 GHz |

Calibration: 5 rounds × 50 ms, median-filtered to reject context switch outliers.

## Architecture

### 1. The Stochastic Signal (Exact OU Kernel)
Asset mispricing $V_t$ is modeled as a mean-reverting Ornstein-Uhlenbeck process:
$$ dV_t = \theta (\mu - V_t) dt + \sigma_V dW_t $$

We use the **exact analytical transition density** (not Euler-Maruyama):
$$ V_{t+\Delta t} \sim \mathcal{N}\left(\mu + (V_t - \mu)e^{-\theta \Delta t},\; \frac{\sigma_V^2}{2\theta}(1 - e^{-2\theta \Delta t})\right) $$

This eliminates discretization bias entirely. The stationary distribution is $V_\infty \sim \mathcal{N}(\mu, \sigma_V^2 / 2\theta)$. The simulator validates the exact kernel by checking that the empirical lag-1 autocorrelation matches the theoretical value $\rho_1 = e^{-\theta \Delta t}$.

### 2. The Execution Game
If two agents observe $V_t > c$ (where $c$ is execution cost), both may trigger an action. The winner is determined by their latency distribution. We model latency $\delta \sim \text{Log-Normal}(\mu_\delta, \sigma_\delta^2)$.

### 3. Signal Decay Under Latency
When an agent acts, the signal mean-reverts during the latency delay:
$$ \mathbb{E}[V_{t+\delta} \mid V_t = b] = \mu + (b - \mu) \cdot e^{-\theta \cdot \mathbb{E}[\delta]} $$
where $\mathbb{E}[\delta] = e^{\mu_\delta + \sigma^2/2}$ for the Log-Normal.

### 4. Latency Race Win Probability
When both agents share the same $\mu_\delta$, the probability of agent A winning the latency race is:
$$ P(\delta_A < \delta_B) = \Phi\left(\frac{\sigma_B^2 - \sigma_A^2}{2\sqrt{\sigma_A^2 + \sigma_B^2}}\right) $$
This follows from the fact that $\log(\delta_A / \delta_B) \sim \mathcal{N}(0, \sigma_A^2 + \sigma_B^2)$.

### 5. Equilibrium Boundary Derivation
The optimal action boundary $b_A^*$ satisfies the **indifference condition**: expected competitive payoff equals zero at the boundary.
$$ P(\text{win}) \cdot \left[\mu + (b_A^* - \mu) \cdot e^{-\theta \cdot \mathbb{E}[\delta_A]} - c\right] = 0 $$

Solving:
$$ b_A^* = \mu + \frac{c - \mu}{P(\text{win}) \cdot e^{-\theta \cdot \mathbb{E}[\delta_A]}} $$

### 6. Dominant Strategy Equilibrium (Proof)

**Theorem.** The strategy profile $(b_A^*, b_B^*)$ constitutes a **dominant strategy equilibrium**.

**Proof.** Observe that $b_A^*$ is a function of $(\sigma_A, \sigma_B, \theta, \mu_\delta, \mu, c)$ — all exogenous parameters of the game. Crucially, $b_A^*$ does **not** depend on $b_B$ (the opponent's chosen boundary). Therefore:

1. $b_A^*$ is agent A's best response **regardless** of what boundary agent B selects. By definition, this makes $b_A^*$ a **dominant strategy** for agent A.
2. By the symmetric argument, $b_B^*$ is a dominant strategy for agent B.
3. A strategy profile where every player plays a dominant strategy is a **dominant strategy equilibrium**, which is the strongest solution concept in non-cooperative game theory — it implies Nash equilibrium, and is also robust to trembling-hand perturbations.

**Numerical verification.** The function `verify_equilibrium_convergence()` runs iterative best-response from arbitrary initial guesses $(b_A = 10, b_B = 10)$ and confirms convergence in **exactly one iteration**, which is the computational signature of the dominant strategy property. If the model had strategic coupling (e.g., market impact making $P(\text{competition})$ depend on $b_B$), convergence would require multiple iterations, indicating a non-trivial fixed-point problem.

**Model limitation.** This dominance arises because agents compete purely on execution speed — the probability of competition is not conditioned on the opponent's boundary. In a richer model where agents can infer the competitor's strategy (e.g., through market impact or information leakage), the best-response *would* depend on $b_B$, requiring Banach contraction or Kakutani fixed-point arguments to establish equilibrium existence.

The `LiveLatency` module actively fits empirical ping jitter to $\sigma_A$ using Welford's Online Algorithm in an independent thread, pushing updates to the execution thread lock-free via cache-line-aligned atomics with `release`/`acquire` memory ordering.

## Disclaimers

**Loopback Latency Proxy.** The UDP loopback measurement (`127.0.0.1`) captures OS scheduler jitter — context switches, timer interrupts, cache pressure — producing $\sigma \approx 0.3\text{--}1.0$. Real co-location execution jitter has $\sigma \approx 0.05\text{--}0.2$. The live adaptation demo uses local jitter as a **pedagogical proxy** to demonstrate how equilibrium boundaries respond dynamically to variance shifts. It does not claim to measure production-grade network latency.

**Web Frontend.** The WebAssembly dashboard uses a synthetic latency random walk for visualization purposes. It does not receive live socket measurements from the host OS.

**Model Scope.** This is a continuous-time stochastic game simulation, not a production trading system. It does not implement FIX/ITCH protocol parsing, kernel-bypass networking, or exchange connectivity.

## Build and Run
This project uses CMake. The build system defaults to **Release mode** — timing Debug builds produces meaningless results because the optimizer eliminates dead stores, inlines functions, and vectorizes hot loops.

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Components
- `arctic`: Core executable. Runs the continuous-time agent race with live latency adaptation and LOB execution. Outputs equilibrium verification, Sharpe ratios, stopping time distributions, OU autocorrelation validation, LOB fill metrics, and per-run timing benchmarks.
- `arctic_sweep`: Runs parameter sweeps across variance gap configurations. Outputs timing, Sharpe ratios, and mean stopping times per sweep step.
- `test_ks`: Validates live jitter against a log-normal distribution via the Kolmogorov-Smirnov test. Cross-platform (Windows/Linux).
- `live_adaptation_logger`: Monitors OS jitter and dynamically shifts $b_A^*$ over a 60-second window, logging to CSV.
- `bench_spsc`: Google Benchmark microbenchmarks for SPSC buffer, LOB, arena allocator, and RDTSCP overhead.

### Benchmark Build
```bash
cmake -DARCTIC_BUILD_BENCHMARKS=ON ..
cmake --build . --config Release --target bench_spsc
./Release/bench_spsc --benchmark_format=console
```

### Web Visualization
```bash
cd web
npm install
npm run dev
```
Opens a browser-native dashboard with:
- Real-time WebGL rendering of OU signal and equilibrium boundaries from WASM heap pointers
- Live telemetry: $V(t)$, $b_A^*$, $b_B^*$, $P(\text{win})$, signal decay
- KaTeX-rendered mathematical derivations
- Interactive latency variance controls via SharedArrayBuffer

## Mathematical Validity (KS-Test)
The KS test validates the empirical log-normal fit of local UDP RTTs.
$$ D_n = \sup_x |F_n(x) - F(x)| $$
If $D_n > 1.36 / \sqrt{n}$ (for $\alpha = 0.05$), the null hypothesis of pure log-normal distribution is rejected. **Loopback testing frequently rejects this** due to OS scheduler anomalies producing heavy tails that the symmetric log-normal cannot capture. This is expected — a mixture model or heavy-tailed distribution (e.g., log-$t$) would be more appropriate for production use.

## Performance Profiling

Measuring performance separates discussion from engineering. ARCTIC provides scripts to build the engine with debug symbols and optimizations enabled (`RelWithDebInfo`), which is required for meaningful profiling.

To profile on Windows:
```powershell
.\scripts\run_profiler.ps1
```

### Analyzing the Results
1. **Visual Studio Profiler**: Open the generated `arctic.sln` in `build_profile`, go to **Debug > Performance Profiler (Alt+F2)**, and select **CPU Usage**. This will generate a Flame Graph showing exactly where cycles are spent.
2. **Intel VTune**: If installed, run `vtune -collect hotspots` against the executable.
3. **What to look for**:
   - Cache misses in the `OrderBook` flat arrays versus the `PoolAllocator` linked list traversal.
   - SPSC Buffer cross-core latency (MESI cache-line transfers should be ~50-100ns when properly pinned).

## References
Laughlin, A., Aguirre, A., & Grundfest, J. (2014). *Information Transmission between Financial Markets in Chicago and New York.* Financial Review, 49(2), 283-312.
