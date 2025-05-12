import os
import time
import matplotlib.pyplot as plt

SIZES_MB = [10**exp for exp in [0, 1, 1.5, 2, 2.5, 3]]  # MB values: 1, 10, 31.6, 100, 316.2, 1000
SIZES_KB = [int(mb * 1024) for mb in SIZES_MB]
MOUNT_PATH = "mnt/fuse_test"

info_latencies = []
read_latencies = []
write_latencies = []
read_throughput = []
write_throughput = []

print("\n--- FUSE Latency & Throughput Tests ---")

for size_kb, size_mb in zip(SIZES_KB, SIZES_MB):
    filename = f"test_{size_kb}kb.txt"
    filepath = os.path.join(MOUNT_PATH, filename)
    data = b'x' * (size_kb * 1024)

    print(f"\nTesting file: {filename}")

    # --- Write Test ---
    start = time.perf_counter()
    with open(filepath, 'wb') as f:
        f.write(data)
    end = time.perf_counter()
    latency_ms = (end - start) * 1000
    write_latencies.append(latency_ms)
    tp = size_kb / (latency_ms / 1000) / 1024  # MB/s
    write_throughput.append(tp)
    print(f"[WRITE] {size_mb:>6.1f}MB → {latency_ms:.3f} ms → {tp:.2f} MB/s")

    # --- Info Test (stat) ---
    start = time.perf_counter()
    os.stat(filepath)
    end = time.perf_counter()
    latency_ms = (end - start) * 1000
    info_latencies.append(latency_ms)
    print(f"[INFO]  {size_mb:>6.1f}MB → {latency_ms:.3f} ms")

    # --- Read Test ---
    start = time.perf_counter()
    with open(filepath, 'rb') as f:
        _ = f.read()
    end = time.perf_counter()
    latency_ms = (end - start) * 1000
    read_latencies.append(latency_ms)
    tp = size_kb / (latency_ms / 1000) / 1024  # MB/s
    read_throughput.append(tp)
    print(f"[READ]  {size_mb:>6.1f}MB → {latency_ms:.3f} ms → {tp:.2f} MB/s")

# --- Plot Latency ---
plt.figure(figsize=(10, 5))
plt.plot(SIZES_MB, info_latencies, marker='o', label="INFO (stat)")
plt.plot(SIZES_MB, read_latencies, marker='o', label="READ (open+read)")
plt.plot(SIZES_MB, write_latencies, marker='o', label="WRITE (open+write)")
plt.xlabel("File Size (MB)")
plt.ylabel("Latency (ms)")
plt.title("FUSE Filesystem: Latency vs File Size")
plt.xscale('log')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("fuse_latency_plot.png")
plt.show()

# --- Plot Throughput ---
plt.figure(figsize=(10, 5))
plt.plot(SIZES_MB, read_throughput, marker='o', label="READ Throughput")
plt.plot(SIZES_MB, write_throughput, marker='o', label="WRITE Throughput")
plt.xlabel("File Size (MB)")
plt.ylabel("Throughput (MB/s)")
plt.title("FUSE Filesystem: Throughput vs File Size")
plt.xscale('log')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("fuse_throughput_plot.png")
plt.show()
