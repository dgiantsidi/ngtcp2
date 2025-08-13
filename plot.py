
import re
import matplotlib.pyplot as plt

# Path to your file
file_path = "results_quic.txt"

# Array to store latency values
latencies = []

# Read the file and extract latency values
with open(file_path, "r") as file:
    for line in file:
        last_token = line.split(",")[-1].strip()

        # Remove 'ms' and ')' and convert to float
        value = float(last_token.replace("ms", "").replace(")", "").strip())

        latencies.append(value)

# Print the array of latencies
#print(latencies)


# Create a bar plot
plt.figure(figsize=(10, 6))
plt.bar(range(len(latencies)), latencies, color='skyblue')

# Add labels and title
plt.xlabel('Sample Index')
plt.ylabel('Latency (ms)')
plt.title('Latency Bar Plot')
plt.xticks(range(len(latencies)))  # Show index numbers on x-axis
plt.grid(axis='y', linestyle='--', alpha=0.7)
global_y_max = max(latencies) * 1.1

plt.ylim(0, global_y_max)

plt.savefig("cmts_latency_plot_w_ccf.png", dpi=300)



# Compute percentiles
p99 = np.percentile(latencies, 99)
p95 = np.percentile(latencies, 95)
p75 = np.percentile(latencies, 75)
p50 = np.percentile(latencies, 50)

# Prepare data for plotting
percentiles = ['p99', 'p95', 'p75', 'p50']
values = [p99, p95, p75, p50]

y_max = max(values) * 1.1

# Bar plot
plt.figure(figsize=(8, 6))
plt.bar(percentiles, values, color=['#ff9999','#66b3ff','#99ff99','#ffcc99'])
plt.title('Latency Percentiles')
plt.ylabel('Latency (ms)')
plt.grid(axis='y', linestyle='--', alpha=0.7)

plt.ylim(0, y_max)

# Annotate values on bars
for i, v in enumerate(values):
    plt.text(i, v + 0.01, f"{v:.3f}", ha='center', fontsize=10)

plt.savefig("cmts_latency_plot_w_ccf_distribution.png", dpi=300)
