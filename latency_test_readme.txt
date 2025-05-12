Quick Run Instructions (Latency Test)

This is a quick-start guide to test and compare HTTP vs FUSE filesystem latency.

Terminal 1: Build and Start HTTP Server
make test
python3 backend/local_server.py --port 8080 --directory backend/test_data

Terminal 2: Run HTTP Latency Test
python3 http_latency_test.py

Terminal 3: Mount the FUSE Filesystem
mkdir -p mnt/fuse_test
mkdir -p mnt/cache
./remote_cache mnt/cache http://localhost:8080/api/data mnt/fuse_test -f -o allow_other

Terminal 4: Run FUSE Latency Test
python3 fuse_latency_test.py

Notes:

Output includes latency/throughput results and saved plots.

Ensure 'user_allow_other' is added to /etc/fuse.conf to allow mounting
