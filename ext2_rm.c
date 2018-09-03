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
    // check for the right number of args
    if (argc != 3) {
        // error msg
        fprintf(stderr,"Usage: ext2_rm <img file name> <abs file path>\n");
        // exit
        exit(1);
    }
    // get the path length
    int path_len = strlen(argv[2]);
    // empty string to copy abs path to
    char path[path_len];
    // copy the abs path to the string
    memcpy(path,argv[2],path_len);
    // trailing 0
    path[path_len] = 0;
    // if not an abs path
    if (path[0] != '/') {
        // error
        fprintf(stderr,"Error: Please provide an absolute path.\n");
        // exit
        exit(EXIT_FAILURE);
    }
    // if the dir ends in / and its not just a slash
    if (path[path_len - 1] == '/' && strcmp(path,"/") != 0) {
        // strip off the slash
        path[path_len - 1] = 0;
    }
    // open the image file
    int img = open(argv[1],O_RDWR);
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
    // see if a directory exists at the given path
    int dir_path = get_dirs_inode(path,inodes);
    // if it didnt fail or say that it found something of a different type
    if (dir_path != NOT_DIRECTORY && dir_path != FAILED) {
        // error msg
        fprintf(stderr,"File to remove is a directory.\n");
        // return directory error
        return EISDIR;
    } else if (dir_path == FAILED) {
        // error msg
        fprintf(stderr,"File does not exist.\n");
        // return no entry error
        return ENOENT;
    }
    int file_inode = 0;
    // string to hold parent path
    char* parent = strdup("");
    // string to hold filename, not really going to use this
    char* file_name = strdup("");
    // split the path
    split_dir(path,parent,file_name);
    // get the inode of the parent
    int parent_inode = get_dirs_inode(parent,inodes);
    // make sure it exists
    if (parent_inode == FAILED) {
        // error msg
        fprintf(stderr,"Parent directory does not exist.\n");
        // no directory
        return ENOENT;
    }
    // loop counter
    int i;
    // instatiate size
    int size = 0;
    // instantiate block num
    int block_num = 0;
    // previous rec len
    int prev_rec_len = 0;
    // get the struct version of the inode
    struct ext2_inode* inodes_struct = (struct ext2_inode *) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    // current directory were looking at instantiation
    struct ext2_dir_entry_2* curr_dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * inodes_struct[parent_inode].i_block[0]);;
    // go through all the inode pointers
    for (i = 0; i < NUM_INODE_POINTERS && curr_dir->inode != file_inode; i++) {
        // get the block number
        block_num = inodes_struct[parent_inode].i_block[i];
        // if block number is 0
        if (block_num == 0) {
            // break out of loop
            break;
        }
        // get size of inode
        int size_total = inodes_struct[parent_inode].i_size;
        // get the directory from the block number
        curr_dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
        // while we haven't gone through the entire inode and we havent found the child to be
        // removed's inode
        while(size < size_total && strlen(file_name) != curr_dir->name_len && strncmp(file_name, curr_dir->name, curr_dir->name_len) != 0) {
            // add rec len to current size
            size += curr_dir->rec_len;
            // set the previous rec len
            prev_rec_len = curr_dir->rec_len;
            // set the curr dir to the next directory
            curr_dir = (void*)curr_dir + curr_dir->rec_len;
            // set file inode
            file_inode = curr_dir->inode;
        }
    }
    // rec len at the current dir
    int curr_rec_len = curr_dir->rec_len;
    // go back one directory in the block
    curr_dir = (void*)curr_dir - prev_rec_len;
    // add the prev rec len to this one
    curr_dir->rec_len += curr_rec_len;
    // get the inode of the file
    struct ext2_inode* f_inode = (struct ext2_inode*)(inodes + sizeof(struct ext2_inode) * file_inode);
    // delete time is now
    f_inode->i_dtime = (unsigned int)time(NULL);
    // remove link
    f_inode->i_links_count--;
    // get the bitmap
    char* inode_bitmap = (char*) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
    // clear out the inode
    set_bitmap_at_index(inode_bitmap,file_inode-1,0);
    // free inodes count up by 1
    gd->bg_free_inodes_count++;
}
