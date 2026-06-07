# ARCTIC Codebase: Detailed Architectural Breakdown

This document provides an extremely detailed, phase-by-phase explanation of the **ARCTIC (Agentic Racing under Competitive Timing in Continuous-time)** codebase. It is designed to give you a complete understanding of exactly what every component does, how it works under the hood, and why it was built this way. You can use this as a direct study guide for your recruiter presentations.

---

## 1. Tech Stack Overview

The project is built for ultra-low-latency performance and rigorous mathematical simulation.

*   **Core Language:** Modern C++ (C++17/C++20).
*   **Memory Management:** Custom zero-allocation architectures (Bump-pointer Arenas, Intrusive Doubly-Linked Pool Allocators). No `std::malloc` or `new` in the hot paths.
*   **Networking & OS:** Raw UDP sockets, `epoll` (Linux) / `select` (Windows) for event-driven I/O, Thread affinity pinning.
*   **Timing:** Hardware Time Stamp Counters (`RDTSCP` instruction) for sub-nanosecond precision. Lock-free Atomics (`std::atomic`) for cross-thread synchronization.
*   **Frontend / Visualization:** WebAssembly (WASM) via Emscripten, JavaScript, WebGL, SharedArrayBuffer for lock-free memory sharing between C++ and JS.
*   **Math & Stats:** Welford's online algorithm for stable variance computation, Exact Ornstein-Uhlenbeck (OU) stochastic kernels.

---

## 2. Phase-by-Phase Breakdown

We can divide the codebase into 6 distinct logical phases/components.

### Phase 1: The Stochastic Market Environment
**Files:** `ou_sampler.hpp`, `ou_sampler.cpp`

*   **What it does:** Simulates the financial market's "fair value" or "mispricing" signal over time.
*   **Why it does it:** High-frequency trading models need realistic price signals. Instead of a random walk (Brownian motion), this uses an Ornstein-Uhlenbeck (OU) process, which means the price tends to "mean-revert" (snap back to a baseline).
*   **How it does it:** Most simulators use the Euler-Maruyama method, which approximates the next step and introduces "discretization bias" (errors that accumulate over time). ARCTIC uses the **Exact Transition Density Kernel**. Because the mathematics of the OU process are fully known, the code directly samples from the exact Gaussian distribution for the next time step. This means `dt` (time step) can be any size and the math remains perfectly accurate.

### Phase 2: Game Theory & Mathematics
**Files:** `math_utils.hpp`, `math_utils.cpp`

*   **What it does:** Calculates the core game-theoretic boundaries. When should an agent trade? Who wins a latency race?
*   **Why it does it:** Two agents competing for the same trade will face a "latency race". If you shoot too early, the signal might decay before your order reaches the exchange. If you shoot too late, your competitor beats you. The system must find the exact mathematical point (the "Dominant Strategy Equilibrium Boundary") where the expected profit is perfectly balanced against the risk of losing the race or the signal decaying.
*   **How it does it:** 
    *   **Signal Decay:** Uses $e^{-\theta \cdot \mathbb{E}[\delta]}$ to figure out how much the price will revert to the mean while the order is traveling over the network.
    *   **Win Probability:** Calculates the exact probability of Agent A beating Agent B given their respective Log-Normal latency distributions.
    *   **Indifference Condition:** Solves for the boundary `b*` where expected profit equals zero. Crucially, the code uses an iterative best-response loop (`verify_equilibrium_convergence`) to prove mathematically that this is a **Dominant Strategy**—meaning an agent's optimal threshold doesn't depend on what the opponent does, only on the physical network latency.

### Phase 3: The Limit Order Book (LOB) Engine
**Files:** `order_book.hpp`

*   **What it does:** Simulates the exchange where buyers and sellers match orders based on Price-Time Priority.
*   **Why it does it:** To simulate real trading, you can't just assume an order magically gets filled at a theoretical price. Orders must cross the spread and consume liquidity. It must be brutally fast to not slow down the millions of Monte Carlo simulation paths.
*   **How it does it (Extreme Low-Level Detail):**
    *   **Flat Array Prices:** It maps prices to integer "ticks". To find the bid/ask queue for a price, it just looks up an array index (`bid_levels_[ticks]`). This is $O(1)$ time complexity. No `std::map` or binary tree traversal.
    *   **Zero-Allocation:** Standard C++ containers (`std::vector`, `std::list`) call the OS kernel (`malloc`) to get memory, which ruins CPU cache and takes hundreds of nanoseconds. ARCTIC uses a `PoolAllocator`. It grabs one massive chunk of memory upfront. 
    *   **Intrusive Doubly-Linked Lists:** Every order has a `next` and `prev` integer index pointing to other orders in the pool. Adding or cancelling an order is just changing two integer pointers. $O(1)$ time complexity, zero cache misses.

### Phase 4: Low-Level Systems, Telemetry, and Memory
**Files:** `live_latency.hpp/cpp`, `tsc_clock.hpp`, `arena_allocator.hpp`, `spsc_buffer.hpp`

*   **What it does:** Measures actual operating system network jitter and manages high-performance memory.
*   **Why it does it:** Standard timers (`std::chrono`) and memory allocators (`new`) are too slow and noisy for HFT logic.
*   **How it does it:**
    *   **`LiveLatency`:** Sends UDP packets to a loopback address. It uses `epoll` (Linux) to wait for the packet to return without burning CPU cycles. It forces the thread to run on Core 2 (`pthread_setaffinity_np`) to avoid OS interrupt noise on Core 0. It calculates the mean and variance of this latency using Welford's online algorithm.
    *   **`TscClock`:** Instead of asking the OS what time it is, it executes the assembly instruction `RDTSCP`. This reads the hardware cycle counter directly from the CPU silicon, giving ~23 nanosecond resolution.
    *   **`ArenaAllocator`:** For temporary data inside the simulation paths, it allocates a massive page-aligned block of memory and uses a "bump pointer". When the simulation needs memory, it just increments a pointer (`offset += size`). This guarantees the data stays in the ultra-fast L1 CPU cache. It is 71x faster than `std::vector`.

### Phase 5: The Simulation Orchestrator
**Files:** `main.cpp`, `single_agent.hpp/cpp`, `race_resolver.hpp`

*   **What it does:** Glues the math, the memory, and the agents together to run millions of Monte Carlo paths.
*   **Why it does it:** To statistically prove the game theory and measure the Sharpe ratio (risk-adjusted return) of the agents.
*   **How it does it:**
    *   It spins up an OU path using the Arena Allocator.
    *   `SingleAgent` iterates through the path. If the price crosses the theoretical boundary `b*`, the agent draws a random latency delay to simulate network transit.
    *   If both agents act, `RaceResolver` determines who arrived first.
    *   The winner's order is submitted to the `OrderBook` to calculate exact slippage and fill prices.
    *   At the end, it calculates Welford Sharpe ratios, Win Rates, and outputs the statistics.

### Phase 6: WebAssembly Visualization
**Files:** `wasm_core.cpp`, `web/` directory

*   **What it does:** Compiles the C++ math engine into a format that runs natively in a web browser for visual presentation.
*   **Why it does it:** Recruiters and stakeholders need to "see" the math.
*   **How it does it:** 
    *   Uses Emscripten to compile `wasm_core.cpp` into WebAssembly.
    *   It binds C++ functions so JavaScript can call them.
    *   It uses a `SharedArrayBuffer` with `std::atomic`. This allows a Web Worker thread to write network latency changes directly into the C++ engine's memory space lock-free, updating the equilibrium boundaries in real-time at 60 FPS while WebGL renders the OU signal path.
