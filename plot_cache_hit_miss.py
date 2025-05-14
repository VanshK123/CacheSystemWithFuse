import ctypes
import matplotlib.pyplot as plt

# Load the shared library (adjust if it's in a different path)
lib = ctypes.CDLL('./libcache.so')

# Declare return types
lib.cache_get_hits.restype = ctypes.c_size_t
lib.cache_get_misses.restype = ctypes.c_size_t

# Get hit/miss stats
hits = lib.cache_get_hits()
misses = lib.cache_get_misses()
total = hits + misses

# Calculate percentages
hit_rate = (hits / total * 100) if total > 0 else 0
miss_rate = (misses / total * 100) if total > 0 else 0

# Plot
labels = ['Cache Hits', 'Cache Misses']
sizes = [hit_rate, miss_rate]

fig, ax = plt.subplots()
ax.pie(sizes, labels=labels, autopct='%1.1f%%', startangle=90)
ax.axis('equal')
plt.title("Cache Hit vs Miss Ratio")
plt.tight_layout()
plt.savefig("cache_hit_miss_plot.png")
plt.show()

