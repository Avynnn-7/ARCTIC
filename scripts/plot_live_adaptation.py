import pandas as pd
import matplotlib.pyplot as plt
import matplotlib as mpl

def main():
    plt.style.use("dark_background")
    mpl.rcParams["axes.prop_cycle"] = mpl.cycler(color=["#00ffff", "#ff00ff", "#ffff00", "#00ff00"])
    mpl.rcParams["axes.grid"] = True
    mpl.rcParams["grid.color"] = "#333333"
    mpl.rcParams["axes.edgecolor"] = "#555555"
    
    df = pd.read_csv("data/live_adaptation.csv")
    
    # Handle both old and new column names
    b_a_col = "Eq_b_A" if "Eq_b_A" in df.columns else "Nash_b_A"
    
    fig, ax1 = plt.subplots(figsize=(10, 6))
    
    color1 = "#00ffff"
    ax1.set_xlabel("Time (s)", fontsize=14)
    ax1.set_ylabel("Measured Jitter $\\sigma_A$ (Live)", color=color1, fontsize=14)
    ax1.plot(df["Time_s"], df["Sigma"], linewidth=2.5, color=color1, label="Live Jitter ($\\sigma_A$)")
    ax1.tick_params(axis='y', labelcolor=color1)
    
    ax2 = ax1.twinx()  
    color2 = "#ff00ff"
    ax2.set_ylabel("Equilibrium Boundary ($b_A^*$)", color=color2, fontsize=14)  
    ax2.plot(df["Time_s"], df[b_a_col], linewidth=2.5, color=color2, linestyle='--', label="Adaptive $b_A^*$ (Dominant Strategy)")
    ax2.tick_params(axis='y', labelcolor=color2)
    
    # Mark the CPU stress zone (approx 15s to 45s)
    ax1.axvspan(15, 45, color='white', alpha=0.1, label="CPU Stress Injected")
    
    # Custom legend
    lines_1, labels_1 = ax1.get_legend_handles_labels()
    lines_2, labels_2 = ax2.get_legend_handles_labels()
    ax1.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper left')

    plt.title("Live Equilibrium Boundary Adaptation Under CPU Load", fontsize=16, fontweight='bold', color='white')
    fig.tight_layout()
    plt.savefig("data/live_adaptation_load.png", dpi=300)
    plt.close()
    
    print("Plot generated: data/live_adaptation_load.png")

if __name__ == "__main__":
    main()
