import pandas as pd
import matplotlib.pyplot as plt

# Read the results from results.txt
# Assuming the file is comma-separated with no header
df = pd.read_csv('results.txt', header=None)

# Define column names based on the C program's output
columns = ['algo', 'size', 'min_cycles', 'max_cycles', 'avg_cycles', 'med_cycles',
           'min_comps', 'max_comps', 'avg_comps', 'med_comps',
           'min_swaps', 'max_swaps', 'avg_swaps', 'med_swaps']
df.columns = columns

# Define the algorithms and metrics to plot
algos = ['bubble', 'quick', 'merge', 'heap']
metrics = ['cycles', 'comps', 'swaps']
stats = ['min', 'max', 'avg', 'med']

# Create separate plots for each metric
for metric in metrics:
    fig, axs = plt.subplots(2, 2, figsize=(12, 10))
    axs = axs.flatten()
    for i, stat in enumerate(stats):
        ax = axs[i]
        for algo in algos:
            # Filter data for the current algorithm
            data = df[df['algo'] == algo]
            ax.plot(data['size'], data[f'{stat}_{metric}'], label=algo, marker='o')
        # Set titles and labels
        if metric != 'cycles':
            ax.set_title(f'{stat.capitalize()} Normalized {metric.capitalize()}')
            ax.set_ylabel(f'Normalized {metric.capitalize()}')
        else:
            ax.set_title(f'{stat.capitalize()} Clock Cycles')
            ax.set_ylabel('Clock Cycles')
        ax.set_xlabel('Array Size')
        ax.legend()
        ax.grid(True)
    plt.tight_layout()
    plt.savefig(f'{metric}_comparison.png')
    plt.close()

print("Plots saved as 'cycles_comparison.png', 'comps_comparison.png', and 'swaps_comparison.png'")