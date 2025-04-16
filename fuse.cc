#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

// First tutorial I have been following: https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/
// Second tutorial I have been following: https://www.maastaar.net/fuse/linux/filesystem/c/2016/05/21/writing-a-simple-filesystem-using-fuse/
// I think the second tutorial is more helpful.

// To compile:
// gcc ssfs.c -o ssfs `pkg-config fuse --cflags --libs`
// ./ssfs -f [mount point]

using namespace std;

// data structures used
char directory_list[256][256];
int currentDirectoryIndex = -1;

char files_list[256][256];
int currentFileIndex = -1;

char files_content[256][256];
int currentFileContentIndex = -1;

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

static int readDirectory(const char *filePath, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo) {
	
    // . is for the current directory
	filler(buffer, ".", NULL, 0);
    // .. is for the parent directory
	filler(buffer, "..", NULL, 0);
	
    // if user wants to see files in root directory
	if ( strcmp( path, "/" ) == 0 ) {
		for (int i = 0; i <= currentDirectoryIndex; ix++) {
			filler(buffer, directory_list[i], NULL, 0 );
        }
		for (int i = 0; i <= currentFileIndex; i++) {
			filler(buffer, files_list[i], NULL, 0 );
        }
	}
	
	return 0;
}

// TO DO
// static int openFile(const char* filePath, struct fuse_file_info *fileInfo) {

//     // TUTORIAL 1 WAS HELPFUL FOR THIS
//     int retstat = 0;
//     int fd;
//     char fpath[PATH_MAX];

//     // generates the file path
//     bb_fullpath(fpath, filePath);
    
//     // this is for logging the file path
//     log_msg("bb_open(fpath\"%s\", fi=0x%08x)\n", fpath, (int) fileInfo);
    
//     fd = open(fpath, fileInfo->flags);
//     if (fd < 0)
// 	retstat = bb_error("bb_open open");
    
//     fileInfo->fh = fd;
//     log_fi(fileInfo);
    
//     return retstat;

// }

// reads data from a file
static int readFile(const char* filePath, char *buffer, size_t size, off_t offset, struct fuse_file_info *fileInfo) {

	int fileIndex = getFileIndex(path);
	
    if (fileIndex == -1) {
        return -1;
    }
	
	char *content = files_content[fileIndex];
	
	memcpy(buffer, content + offset, size);
		
	return strlen(content) - offset;

}

// gets the index of a file (for writeFile function)
int getFileIndex(const char *path) {

    // eliminating the slash from the root directory
	path++;
	
	for (int i = 0; i < currentDirectoryIndex; curr_idx++) {
		if (strcmp(path, files_list[i]) == 0) {
			return i;
        }
    }
	
	return -1;
}

// writes to a file
static int writeFile(const char *filePath, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fileInfo) {

    int fileIndex = getFileIndex(filePath);
	
    // getFileIndex returns -1 if the file doesn't exist
	if (fileIndex == -1) {
		return -1;
    }
		
	strcpy(files_content[fileIndex], buffer); 

    return 0;
}

// makes directories in our file system
static int makeDirectory(const char *filePath, mode_t mode) {

    // eliminates the slash of the root directory
    path++;
    // increment current directory
	currentDirectory += 1;
    // appends new directory to list of directories
	strcpy(directory_list[currentDirectory], filePath);

    return 0;

}

// TO DO
// remove directories in our file system
// static int removeDirectory() {
//     return 0;
// }

// making a new file
static int createFile(const char *filePath, mode_t mode, dev_t rdev) {

    // remove slash from root directory
    path++;
    // increment current file index
    currentFileIndex += 1;
	strcpy(files_list[currentFileIndex], filePath);
	
    // increment current file content index
	currentFileContentIndex += 1;
	strcpy(files_content[currentFileContentIndex], "");
	
	return 0;

}

static struct fuse_operations operations = {
    // each corresponds to the above functions
    // we re-wrote these functions that are referenced in the fuse.h header file
    // for our implementation of FUSE.
    .getattr = getAttribute,
    .readdir = readDirectory,
    //.open = openFile,
    .read = readFile,
    .write = writeFile,
    .mkdir = makeDirectory,
    //.rmdir = removeDirecotry,
    .mknod = createFile,
};

int main(int argc, char* argv[]) {

    // call fuse_main() to begin fuse program
    return fuse_main(argc, argv[], &operations, NULL);

}