#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "helper.h"

unsigned char *disk;

int main(int argc, char **argv) {
    // check if right number of args
    if ((argc != 3 && argc != 4) || (argc == 4 && strcmp(argv[2],"-a") != 0)) {
        // usage error
        fprintf(stderr, "Usage: ext2_ls <img file name> [-a] <directory>\n");
        // exit
        exit(1);
    }
    // flag for -a param
    int flag = 0;
    // directory location
    int dir_loc = 2;
    // if there is a -a flag
    if (argc == 4) {
        // flag is true
        flag = 1;
        // directory location is in third spot
        dir_loc = 3;
    }
    // get the length of the dir path
    int dir_length = strlen(argv[dir_loc]);
    // create array of char to hold dir
    char dir[dir_length+1];
    // make room in mem
    memcpy(dir,argv[dir_loc],dir_length);
    // ending 0
    dir[dir_length] = 0;
    // make sure we have an absolute path
    if (dir[0] != '/') {
        // error msg
        fprintf(stderr,"Error: Please provide an absolute path.\n");
        // exit
        exit(EXIT_FAILURE);
    }
    // open the image for read/write
    int img = open(argv[1], O_RDWR);
    // load into the disk
    disk = mmap(NULL,NUM_BLOCKS * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, img, 0);
    // if it fails
    if (disk == MAP_FAILED) {
        // error msg
        perror("mmap failed.\n");
        // exit
        exit(EXIT_FAILURE);
    }
    // group descriptor
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
    // inodes
    struct ext2_inode* inodes = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    // get the directory's inode
    int dir_inode = get_dirs_inode(dir,inodes);
    // if failed
    if (dir_inode == FAILED) {
        // print to stderr
        fprintf(stderr,"No such file or directory\n");
        // no entry error
        return ENOENT;
    }
    // dir's inode is the correct type (directory)
    if (dir_inode != NOT_DIRECTORY) {
        // print ls to stdout
        print_ls(dir_inode,inodes,flag);
    // if the inputted path was not a directory
    } else if (dir_inode == NOT_DIRECTORY){
        // file name
        char* file = strdup("");
        // init parent dir
        char* parent_dir = strdup("");
        // split the dir into parent and the file at the end of the path
        split_dir(dir,parent_dir,file);
        // print the file name
        printf("%s\n",file);
    }
}
