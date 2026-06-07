import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl

def main():
    plt.style.use("dark_background")
    mpl.rcParams["axes.prop_cycle"] = mpl.cycler(color=["#00ffff", "#ff00ff", "#ffff00", "#00ff00"])
    mpl.rcParams["axes.grid"] = True
    mpl.rcParams["grid.color"] = "#333333"
    mpl.rcParams["axes.edgecolor"] = "#555555"
    
    df = pd.read_csv("data/sweep_results.csv")
    
    # Handle both old (Nash_B_A) and new (Eq_B_A) column names
    b_a_col = "Eq_B_A" if "Eq_B_A" in df.columns else "Nash_B_A"
    
    df_plot = df[df["Sigma_A"] >= df["Sigma_B"]].copy()
    df_plot["Variance_Gap"] = df_plot["Sigma_A"] - df_plot["Sigma_B"]
    
    # Plot 1: Competitive Cost vs Variance Gap
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(df_plot["Variance_Gap"], df_plot["Competitive_Cost"], marker='o', linewidth=2.5, color="#ff00ff")
    ax.set_title("Competitive Cost of Latency Variance", fontsize=16, fontweight='bold', color='white')
    ax.set_xlabel("Variance Gap ($\\sigma_A - \\sigma_B$)", fontsize=14)
    ax.set_ylabel("PnL Penalty (Solo - Agent A)", fontsize=14)
    ax.fill_between(df_plot["Variance_Gap"], df_plot["Competitive_Cost"], color="#ff00ff", alpha=0.1)
    plt.tight_layout()
    plt.savefig("data/competitive_cost_curve.png", dpi=300)
    plt.close()
    
    # Plot 2: Win Rate and Equilibrium Boundary vs Variance Gap
    fig, ax1 = plt.subplots(figsize=(10, 6))
    color1 = "#00ffff"
    ax1.set_xlabel("Variance Gap ($\\sigma_A - \\sigma_B$)", fontsize=14)
    ax1.set_ylabel("Win Rate (%)", color=color1, fontsize=14)
    ax1.plot(df_plot["Variance_Gap"], df_plot["Win_Rate_A"] * 100, marker='s', linewidth=2.5, color=color1, label="Agent A Win Rate")
    ax1.tick_params(axis='y', labelcolor=color1)
    
    ax2 = ax1.twinx()  
    color2 = "#ffff00"
    ax2.set_ylabel("Equilibrium Boundary ($b_A^*$)", color=color2, fontsize=14)  
    ax2.plot(df_plot["Variance_Gap"], df_plot[b_a_col], marker='^', linewidth=2.5, color=color2, linestyle='--', label="$b_A^*$ (Dominant Strategy)")
    ax2.tick_params(axis='y', labelcolor=color2)
    
    plt.title("Win Rate & Equilibrium Boundary Compression under Competitive Pressure", fontsize=16, fontweight='bold', color='white')
    fig.tight_layout()
    plt.savefig("data/nash_boundary_compression.png", dpi=300)
    plt.close()
    
    # Plot 3: Sharpe Ratio vs Variance Gap (if column exists)
    if "Sharpe_A" in df_plot.columns:
        fig, ax = plt.subplots(figsize=(10, 6))
        ax.plot(df_plot["Variance_Gap"], df_plot["Sharpe_A"], marker='D', linewidth=2.5, color="#00ff00")
        ax.set_title("Sharpe Ratio Degradation under Latency Variance", fontsize=16, fontweight='bold', color='white')
        ax.set_xlabel("Variance Gap ($\\sigma_A - \\sigma_B$)", fontsize=14)
        ax.set_ylabel("Sharpe Ratio (Agent A)", fontsize=14)
        ax.fill_between(df_plot["Variance_Gap"], df_plot["Sharpe_A"], color="#00ff00", alpha=0.1)
        plt.tight_layout()
        plt.savefig("data/sharpe_degradation.png", dpi=300)
        plt.close()
    
    print("Plots generated successfully in data/ directory.")

if __name__ == "__main__":
    main()
