COMPILER = gcc
FILESYSTEM_FILES = fuse.cc

build: $(FILESYSTEM_FILES)
	$(COMPILER) $(FILESYSTEM_FILES) -o fusexec `pkg-config fuse3 --cflags --libs`
	echo 'To Mount: ./fusexec -f [mount point]'

clean:
	rm ssfs
