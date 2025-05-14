import time
import requests
import matplotlib.pyplot as plt
import numpy as np

BASE_URL = "http://localhost:8080/api"
SIZES_MB = [10**exp for exp in [0, 1, 1.5, 2, 2.5, 3]]  # MB values: 1, 10, 31.6, 100, 316.2, 1000
SIZES_KB = [int(mb * 1024) for mb in SIZES_MB]          # Convert to KB for actual file size

# Storage
info_latencies = []
read_latencies = []
write_latencies = []
read_throughput = []
write_throughput = []

def test_filename(size_kb):
    return f"test_{size_kb}kb.txt"

print("\n--- Latency & Throughput Tests ---")

for size_kb, size_mb in zip(SIZES_KB, SIZES_MB):
    name = test_filename(size_kb)
    url_info = f"{BASE_URL}/info/{name}"
    url_data = f"{BASE_URL}/data/{name}"

    print(f"\nTesting file: {name}")

    # --- INFO ---
    start = time.perf_counter()
    r = requests.get(url_info)
    end = time.perf_counter()
    latency_ms = (end - start) * 1000
    info_latencies.append(latency_ms)
    print(f"[INFO]   {size_mb:>6.1f}MB → {latency_ms:.3f} ms")

    # --- READ ---
    start = time.perf_counter()
    r = requests.get(url_data)
    end = time.perf_counter()
    latency_ms = (end - start) * 1000
    read_latencies.append(latency_ms)
    tp_kb = size_kb / (latency_ms / 1000) if latency_ms > 0 else 0
    tp_mb = tp_kb / 1024
    read_throughput.append(tp_mb)
    print(f"[READ]   {size_mb:>6.1f}MB → {latency_ms:.3f} ms → {tp_mb:.2f} MB/s")

    # --- WRITE ---
    data = b'x' * (size_kb * 1024)
    start = time.perf_counter()
    r = requests.put(url_data, data=data)
    end = time.perf_counter()
    latency_ms = (end - start) * 1000
    write_latencies.append(latency_ms)
    tp_kb = size_kb / (latency_ms / 1000) if latency_ms > 0 else 0
    tp_mb = tp_kb / 1024
    write_throughput.append(tp_mb)
    print(f"[WRITE]  {size_mb:>6.1f}MB → {latency_ms:.3f} ms → {tp_mb:.2f} MB/s")

# --- Plot: Latency ---
plt.figure(figsize=(10, 5))
plt.plot(SIZES_MB, info_latencies, marker='o', label="INFO (Metadata)")
plt.plot(SIZES_MB, read_latencies, marker='o', label="READ (GET)")
plt.plot(SIZES_MB, write_latencies, marker='o', label="WRITE (PUT)")
plt.title("HTTP Backend: Latency vs File Size (MB)")
plt.xlabel("File Size (MB)")
plt.ylabel("Latency (ms)")
plt.xscale('log')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("http_latency_plot.png")
plt.show()

# --- Plot: Throughput ---
plt.figure(figsize=(10, 5))
plt.plot(SIZES_MB, read_throughput, marker='o', label="READ Throughput")
plt.plot(SIZES_MB, write_throughput, marker='o', label="WRITE Throughput")
plt.title("HTTP Backend: Throughput vs File Size (MB)")
plt.xlabel("File Size (MB)")
plt.ylabel("Throughput (MB/s)")
plt.xscale('log')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("http_throughput_plot.png")
plt.show()
