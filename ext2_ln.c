#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "helper.h"

unsigned char *disk;

int main(int argc, char **argv) {
    // not right amount of arguments
    if ((argc != 4 && argc != 5) || (argc == 5 && strcmp(argv[2],"-s") != 0)) {
        // error msg
        fprintf(stderr,"Usage: ext2_ln <img file name> [-s] <abs file/link path> <abs file/link path>\n");
        // exit
        exit(1);
    }
    // set the file type
    int link_type = EXT2_FT_REG_FILE;
    // if the -s flag is on
    if (argc == 5 && strcmp(argv[2],"-s") == 0) {
        // set the link type to symlink
        link_type = EXT2_FT_SYMLINK;
    }
    // get the path length
    int path1_len;
    int path2_len;
    // if no flag
    if (link_type == EXT2_FT_REG_FILE) {
        // path length is the size of second arg
        path1_len = strlen(argv[2]);
        path2_len = strlen(argv[3]);
    } else {
        path1_len = strlen(argv[3]);
        path2_len = strlen(argv[4]);
    }
    // empty string to copy abs path to
    char* path1 = (char*) malloc((path1_len+1)*sizeof(char));
    // empty string to copy abs path to
    char* path2 = (char*) malloc((path2_len+1)*sizeof(char));
    if (link_type == EXT2_FT_REG_FILE) {
        // copy the abs path to the string
        memcpy(path1,argv[2],path1_len);
        // copy the abs path to the string
        memcpy(path2,argv[3],path2_len);
    } else {
        // copy the abs path to the string
        memcpy(path1,argv[3],path1_len);
        // copy the abs path to the string
        memcpy(path2,argv[4],path2_len);
    }
    // trailing 0
    path1[path1_len] = 0;
    // trailing 0
    path2[path2_len] = 0;
    // if neither path is absolute
    if (path1[0] != '/' || path2[0] != '/') {
        // error msg
        fprintf(stderr,"Error: Please input both paths as absolute paths.\n");
        // exit
        exit(EXIT_FAILURE);
    }
    // if path1 ends in slash
    if (path1[path1_len-1] == '/' && strcmp(path1,"/") != 0) {
        // we're not going to assume its a dir, we'll give it a chance
        // strip off the slash
        path1[path1_len] = 0;
    }
    // if path2 ends in slash
    if (path2[path2_len-1] == '/' && strcmp(path2,"/") != 0) {
        // we're not going to assume its a dir, we'll give it a chance
        // strip off the slash
        path2[path2_len] = 0;
    }
     // open the image file
    int img = open(argv[1], O_RDWR);
    // read img file into memory
    disk = mmap(NULL,NUM_BLOCKS * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, img, 0);
    // if failed
    if (disk == MAP_FAILED) {
        // error msg
        perror("mmap failed");
        // exit
        exit(EXIT_FAILURE);
    }
    // group descriptor
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
    // inodes
    void* inodes = (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    // get dir inode of path1
    int path1_inode = get_dirs_inode(path1,inodes);
    // if it didnt fail
    if (path1_inode != NOT_DIRECTORY && path1_inode != FAILED) {
        // error msg
        fprintf(stderr,"First path is a directory.\n");
        // return dir error
        return EISDIR;
    // if we didnt find the file at all
    } else if (path1_inode == FAILED) {
        // error msg
        fprintf(stderr,"First path's file does not exist.\n");
        // return not entry error
        return ENOENT;
    }
    // get dir inode of second path
    int path2_inode = get_dirs_inode(path2,inodes);

    // if it didnt fail
    if (path2_inode != NOT_DIRECTORY && path2_inode != FAILED) {
        // error msg
        fprintf(stderr,"Second path is a directory.\n");
        // isdir error
        return EISDIR;
    // if we found it
    } else if (path2_inode == NOT_DIRECTORY) {
        // error msg
        fprintf(stderr,"Link to second path's file already exists");
        // return exists error
        return EEXIST;
    }
    // string to hold parent path (path1)
    char* parent1 = strdup("");
    // string to hold filename (path1)
    char* file_name1 = strdup("");
    // split the path
    split_dir(path1,parent1,file_name1);
    // string to hold parent path (path1)
    char* parent2 = strdup("");
    // string to hold filename (path1)
    char* file_name2 = strdup("");
    // split the path
    split_dir(path2,parent2,file_name2);
    // get path1 parent inode
    int path1_parent_inode = get_dirs_inode(parent1,inodes);
    // check for errors
    if (path1_parent_inode == FAILED || path1_parent_inode == NOT_DIRECTORY) {
        // error msg
        fprintf(stderr,"Error finding first path's parent directory.\n");
        // exit
        return EXIT_FAILURE;
    }
    // get path2 parent inode
    int path2_parent_inode = get_dirs_inode(parent2,inodes);
    // check for errors
    if (path2_parent_inode == FAILED || path2_parent_inode == NOT_DIRECTORY) {
        // error msg
        fprintf(stderr,"Error finding second path's parent directory.\n");
        // exit
        return EXIT_FAILURE;
    }
    // get the first file's inode
    int path1_file_inode = get_file_symlink_inode(file_name1,path1_parent_inode,inodes);
    // we already checked for errors before
    // find a free block
    int free_block = get_free_inode_or_block(gd,BLOCKS);
    // if we could not find a free block
    if (free_block == NOT_FOUND) {
        // error msg
        fprintf(stderr,"Error: No free blocks available.\n");
        // exit
        return ENOSPC;
    }
    // new directory entry
    struct ext2_dir_entry_2* link_to_add = (void*)(disk + EXT2_BLOCK_SIZE * (free_block + 1));
    // set all the info in the directory entry
    set_dir(link_to_add,file_name2,NUM_DIRECT_POINTERS,link_type,path1_file_inode);
    // get the parent inode of the destination path
    struct ext2_inode* path2_parent_struct = (struct ext2_inode*)(inodes + sizeof(struct ext2_inode) * path2_parent_inode);
    // add the directory entry
    add_dir_entry(file_name2,gd,path2_parent_inode,path1_file_inode-1,inodes,path2_parent_struct,link_type);
    // get the inode of the first path's file
    struct ext2_inode* path1_file_struct = (struct ext2_inode*)(inodes + sizeof(struct ext2_inode) * path1_file_inode);
    // update the link counter
    path1_file_struct->i_links_count++;
}
