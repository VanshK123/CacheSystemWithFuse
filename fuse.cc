#include <fuse.h>

// First tutorial I have been following: https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/
// Second tutorial I have been following: https://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
// I think the second tutorial is more helpful.

using namespace std;

static int getAttribute(const char* filePath, struct stat st*, struct fuse_file_info *fileInfo) {
    // stats struct contains data about the file (size, ownership, etc.)
    // so here, we are getting a bunch of those stats
    // get user id
    st->st_uid = getuid();
    // get group id
    st->st_gid = getgid();
    // last access time
    st->st_atime = time(NULL);
    // last modification time
    st->st_mtime = time(NULL);

    if (strcmp(filePath, "/") == 0) {
        // S_IFDIR means the file is a directory
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    } else {
        // S_IFREG means the file is a regular file
        st->mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_size = 1024;
    }

    return 0;
}
// TO DO
static int readDirectory() {
    return 0;
}
// TO DO
static int openFile(const char* filePath, struct fuse_file_info *fileInfo) {
    int fileDirectory = ;
    if (fileDirectory < 0) {
        // error
        return -1;
    }
    fileInfo->fh = fileDirectory;
    return 0;
}
// reads data from a file
static int readFile(const char* filePath, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    // pread is part of the linux library which reads data from a file descriptor
    int pread = pread(fileInfo->fh, buf, size, offset);
    if (pread < 0) {
        // error
        return -1;
    }

    return pread;
}
// TO DO
static int writeFile() {
    return 0;
}
// TO DO
// makes directories in our file system
static int makeDirectory() {
    return 0;
}
// TO DO
// remove directories in our file system
static int removeDirectory() {
    return 0;
}

static struct fuse_operations operations = {
    // each corresponds to the above functions
    // we re-wrote these functions that are referenced in the fuse.h header file
    // for our implementation of FUSE.
    .getattr = getAttribute,
    .readdir = readDirectory,
    .open = openFile,
    .read = readFile,
    .write = writeFile,
    .mkdir = makeDirectory,
    .rmdir = removeDirecotry,
};

int main(int argc, char* argv[]) {

    // call fuse_main() to begin fuse program
    return fuse_main(argc, argv[], &operations, NULL);

}