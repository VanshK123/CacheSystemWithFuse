import subprocess
import time
import matplotlib.pyplot as plt

x = []
y = []

# Define file sizes and corresponding test files
test_files = [
    (1, "test_1kb.txt"),
    (10, "test_10kb.txt"),
    (100, "test_100kb.txt"),
    (1000, "test_1000kb.txt")
]

for size, filename in test_files:
    url = f"http://localhost:8080/api/info/{filename}"
    start = time.perf_counter()
    result = subprocess.run(["curl", "-X", "GET", url], capture_output=True)
    end = time.perf_counter()
    duration = end - start
    x.append(duration)
    y.append(size)
    print(f"Downloaded {filename} in {duration:.4f} seconds")

# Plotting
plt.plot(x, y, marker='o', label="Download speed")
plt.xlabel('Time (seconds)')
plt.ylabel('File size (kilobytes)')
plt.title('Download Time vs File Size')
plt.legend()
plt.grid(True)
plt.show()
