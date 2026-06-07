"""
Monte Carlo Convergence Analysis for ARCTIC

Demonstrates that PnL standard error decreases as 1/sqrt(N), validating
that the Monte Carlo estimator converges properly and that the chosen
num_paths (50,000) provides sufficient precision.

This script implements the OU + boundary simulation directly in Python
rather than calling the C++ binary, enabling controlled variation of num_paths.
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl
from scipy.stats import norm

def normal_cdf(x):
    return norm.cdf(x)

def compute_p_win(sig_a, sig_b):
    var_a = sig_a ** 2
    var_b = sig_b ** 2
    combined = np.sqrt(var_a + var_b)
    if combined < 1e-10:
        return 0.5
    return normal_cdf((var_b - var_a) / (2.0 * combined))

def compute_equilibrium_boundary(sig_self, sig_comp, theta, mu_delta, mu, cost_c):
    expected_latency = np.exp(mu_delta + sig_self**2 / 2.0)
    decay = np.exp(-theta * expected_latency)
    if decay < 1e-10:
        return 1.0
    p_win = compute_p_win(sig_self, sig_comp)
    effective = p_win * decay
    if effective < 1e-10:
        return 1.0
    return mu + (cost_c - mu) / effective

def run_simulation(num_paths, sigma_A, sigma_B, seed=42):
    """Run the ARCTIC simulation with exact OU kernel in Python."""
    theta = 2.0
    mu = 0.0
    sigma_V = 1.0
    dt = 0.01
    steps = 1000
    cost_c = 0.5
    mu_delta = -4.0
    
    rng = np.random.RandomState(seed)
    
    # Precompute OU transition kernel
    ou_decay = np.exp(-theta * dt)
    ou_std = sigma_V * np.sqrt((1.0 - np.exp(-2.0 * theta * dt)) / (2.0 * theta))
    
    b_a = compute_equilibrium_boundary(sigma_A, sigma_B, theta, mu_delta, mu, cost_c)
    
    pnl_per_path = np.zeros(num_paths)
    
    for p in range(num_paths):
        # Generate OU path (exact kernel)
        v = np.zeros(steps)
        v[0] = mu
        z = rng.randn(steps - 1)
        for i in range(1, steps):
            v[i] = mu + (v[i-1] - mu) * ou_decay + ou_std * z[i-1]
        
        # Agent A decision: observe with latency, act if above boundary
        latency_a = rng.lognormal(mu_delta, sigma_A)
        lag_steps = int(latency_a / dt)
        
        for i in range(1, steps):
            obs_idx = max(0, i - lag_steps)
            if v[obs_idx] >= b_a:
                exec_step = min(i + int(latency_a / dt), steps - 1)
                pnl_per_path[p] = v[exec_step] - cost_c
                break
    
    return pnl_per_path

def main():
    plt.style.use("dark_background")
    mpl.rcParams["axes.grid"] = True
    mpl.rcParams["grid.color"] = "#333333"
    mpl.rcParams["axes.edgecolor"] = "#555555"
    
    sigma_A = 0.5
    sigma_B = 0.1
    
    path_counts = [500, 1000, 2500, 5000, 10000, 25000, 50000]
    means = []
    std_errors = []
    
    print("Running convergence analysis...")
    for n in path_counts:
        print(f"  num_paths = {n}...", end="", flush=True)
        pnl = run_simulation(n, sigma_A, sigma_B)
        m = np.mean(pnl)
        se = np.std(pnl, ddof=1) / np.sqrt(n)
        means.append(m)
        std_errors.append(se)
        print(f" mean={m:.6f}, SE={se:.6f}")
    
    path_counts = np.array(path_counts)
    std_errors = np.array(std_errors)
    means = np.array(means)
    
    # Reference 1/sqrt(N) line scaled to match the first data point
    ref_scale = std_errors[0] * np.sqrt(path_counts[0])
    ref_line = ref_scale / np.sqrt(path_counts)
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # Left: Standard Error vs N (log-log)
    ax1.loglog(path_counts, std_errors, 'o-', color="#ff00ff", linewidth=2.5, markersize=8, label="Empirical SE")
    ax1.loglog(path_counts, ref_line, '--', color="#555555", linewidth=1.5, label="$1/\\sqrt{N}$ reference")
    ax1.set_xlabel("Number of Monte Carlo Paths ($N$)", fontsize=14)
    ax1.set_ylabel("PnL Standard Error", fontsize=14)
    ax1.set_title("Monte Carlo Convergence", fontsize=16, fontweight='bold', color='white')
    ax1.legend(fontsize=12)
    
    # Right: Mean PnL vs N (shows stabilization)
    ax2.semilogx(path_counts, means, 's-', color="#00ffff", linewidth=2.5, markersize=8)
    ax2.fill_between(path_counts, means - 2*std_errors, means + 2*std_errors, color="#00ffff", alpha=0.15, label="$\\pm 2$ SE")
    ax2.set_xlabel("Number of Monte Carlo Paths ($N$)", fontsize=14)
    ax2.set_ylabel("Mean PnL (Agent A)", fontsize=14)
    ax2.set_title("PnL Estimate Stabilization", fontsize=16, fontweight='bold', color='white')
    ax2.legend(fontsize=12)
    
    fig.suptitle(f"Convergence Analysis ($\\sigma_A={sigma_A}$, $\\sigma_B={sigma_B}$)", 
                 fontsize=18, fontweight='bold', color='white', y=1.02)
    plt.tight_layout()
    plt.savefig("data/convergence_analysis.png", dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"\nConvergence plot saved to data/convergence_analysis.png")
    print(f"At N=50000: SE={std_errors[-1]:.6f} (mean={means[-1]:.6f})")
    print(f"SE ratio (N=500 vs N=50000): {std_errors[0]/std_errors[-1]:.1f}x")
    print(f"Expected ratio (sqrt(50000/500)): {np.sqrt(50000/500):.1f}x")

if __name__ == "__main__":
    main()
