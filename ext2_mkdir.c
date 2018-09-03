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
    // if right amount of arguments
    if (argc != 3) {
        // error msg
        fprintf(stderr,"Usage: ext2_mkdir <img file name> <directory>\n");
        // exit
        exit(1);
    }
    // get the length of the dir path
    int dir_length = strlen(argv[2]);
    // create array of char to hold dir
    char dir[dir_length+1];
    // copy it to the string
    memcpy(dir,argv[2],dir_length);
    // ending 0
    dir[dir_length] = 0;
    // check if it ends in slash
    if (dir[strlen(argv[2]) - 1] == '/') {
        // strip out the slash
        dir[strlen(argv[2]) - 1] = 0;
    }
    // make sure we have an absolute path
    if (dir[0] != '/') {
        // error msg
        fprintf(stderr,"Error: Please provide an absolute path.\n");
        // exit
        exit(EXIT_FAILURE);
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
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    // group descriptor
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
    // inodes
    void* inodes = (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    // get the inode for the given path
    int dir_inode = get_dirs_inode(dir, inodes);
    // if we found the dir, or a file with the same name
    if (dir_inode != NOT_DIRECTORY && dir_inode != FAILED) {
        // error msg
        fprintf(stderr,"Directory already exists.\n");
        // exists error
        return EEXIST;
    }
    // init dir to make
    char* dir_to_mk = strdup("");
    // init parent dir
    char* parent_dir = strdup("");
    // split the dir into parent and dir to make
    split_dir(dir,parent_dir,dir_to_mk);
    // get inode of parent inode
    int parent_inode = get_dirs_inode(parent_dir,inodes);
    // make sure the parent directory exists
    if (parent_inode == FAILED) {
        // error msg
        fprintf(stderr,"Parent directory not found.\n");
        // exit
        return ENOENT;
    } else if (parent_inode == NOT_DIRECTORY) {
        // error msg
        fprintf(stderr,"Inputted parent directory is not a directory.\n");
        // exit
        return ENOTDIR;
    }
    // get the bitmap
    char* inode_bitmap = (char*) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
    // get free inode
    int free_inode = get_free_inode_or_block(gd,INODES);
    // check if failed
    if (free_inode == FAILED) {
        // error msg
        fprintf(stderr,"No free inodes available.\n");
        // no space
        return ENOSPC;
    }
    // get the bitmap
    char* block_bitmap = (char*) (disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
    // get free block
    int free_block = get_free_inode_or_block(gd,BLOCKS);
    // check if failed
    if (free_block == FAILED) {
        // error msg
        fprintf(stderr,"No free inodes available.\n");
        // no space
        return ENOSPC;
    }
    // set the new inode
    set_inode(inodes, free_inode, EXT2_S_IFDIR, EXT2_BLOCK_SIZE, 2, 2, free_block);
    // set the inode to in use
    set_bitmap_at_index(inode_bitmap,free_inode,1);
    // decrement number of free inodes
    sb->s_free_inodes_count--;
    gd->bg_free_inodes_count--;
    // get the directory entry at the free block
    struct ext2_dir_entry_2* mk_dir = (void*)(disk + EXT2_BLOCK_SIZE * (free_block + 1));
    // set the dir for self
    set_dir(mk_dir,".",NUM_DIRECT_POINTERS,EXT2_FT_DIR,free_inode);
    // move to parent
    struct ext2_dir_entry_2* mk_dir_parent = (void*)mk_dir + mk_dir->rec_len;
    // set the parent dir
    set_dir(mk_dir_parent,"..",EXT2_BLOCK_SIZE - NUM_DIRECT_POINTERS,EXT2_FT_DIR,parent_inode);
    // set the block to in use
    set_bitmap_at_index(block_bitmap,free_block,1);
    // decrement number of free blocks
    sb->s_free_blocks_count--;
    gd->bg_free_blocks_count--;
    // get parent inode
    struct ext2_inode* parent = (struct ext2_inode*)(inodes + sizeof(struct ext2_inode) * parent_inode);
    // another link
    parent->i_links_count++;
    // add the directory entry to the parent
    int entry = add_dir_entry(dir_to_mk,gd,parent_inode,free_inode,inodes,parent,EXT2_FT_DIR);
    // if we didnt find the dir
    if (entry == NOT_FOUND) {
        // one less free block
        sb->s_free_blocks_count--;
        gd->bg_free_blocks_count--;
    }
    return 0;
}
