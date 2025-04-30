import subprocess
import time
import matplotlib.pyplot as plt

# capture_output must be true for python script to see output
output = subprocess.run(["echo", "hello"], capture_output=True)
# output of the echo is printed (should be hello)
print(output)

x = []
y = []

# DOWNLOADING FILES

start = time.perf_counter()
# download 10 kB file
output = subprocess.run("curl -X GET http://localhost:8080/api/info/test_1kb.txt", capture_output=True)
end = time.perf_counter()
commandTime = end - start
# command time
x.append(commandTime)
y.append(1)
# output the time it took to download this file
output = subprocess.run(["echo", "Command Time: $commandTime"], capture_output=True)

start = time.perf_counter()
# download 10 kB file
output = subprocess.run("curl -X GET http://localhost:8080/api/info/test_10kb.txt", capture_output=True)
end = time.perf_counter()
commandTime = end - start
# command time
x.append(commandTime)
y.append(10)
# output the time it took to download this file
output = subprocess.run(["echo", "Command Time: $commandTime"], capture_output=True)

start = time.perf_counter()
# download 100 kB file
output = subprocess.run("curl -X GET http://localhost:8080/api/info/test_100kb.txt", capture_output=True)
end = time.perf_counter()
commandTime = end - start
# command time
x.append(commandTime)
y.append(100)
# output the time it took to download this file
output = subprocess.run(["echo", "Command Time: $commandTime"], capture_output=True)

start = time.perf_counter()
# download 100 kB file
output = subprocess.run("curl -X GET http://localhost:8080/api/info/test_1000kb.txt", capture_output=True)
end = time.perf_counter()
commandTime = end - start
# command time
x.append(commandTime)
y.append(1000)
# output the time it took to download this file
output = subprocess.run(["echo", "Command Time: $commandTime"], capture_output=True)

plt.plot(x, y)
plt.xlabel('time (seconds)')
plt.ylabel('file size (kilobytes)')
plt.title('Throughput')
plt.legend()
plt.show()
