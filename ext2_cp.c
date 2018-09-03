#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "helper.h"
#include <string.h>
#include <errno.h>
#include <time.h>

unsigned char *disk;

int main(int argc, char **argv) {
    // if not 4 args
    if (argc != 4) {
        // usage error
        fprintf(stderr,"Usage: ext2_cp <img file name> <source file path> <target file abs path>\n");
        // exit
        exit(1);
    }
    // open img file for rd wr
    int img = open(argv[1], O_RDWR);
    // file pointer
    FILE *src_file = fopen(argv[2],"r");
    // if we didnt find the file
    if (src_file == NULL) {
        // error msg
        fprintf(stderr,"Source file does not exist.\n");
        // no entry
        return ENOENT;
    }
    // seek to the end of the file
    fseek(src_file, 0, SEEK_END);
    // get the file size
    int file_size = ftell(src_file);
    // rewind the file pointer
    rewind(src_file);
    // get length of abs path
    int abs_path_len = strlen(argv[3]);
    // new string for abs_path
    char abs_path[abs_path_len];
    // copy abs path into string
    memcpy(abs_path,argv[3],abs_path_len);
    // check if abs_path is an absolute path
    if (abs_path[0] != '/') {
        // error msg
        fprintf(stderr,"Please provide an absolute target path.\n");
        // exit
        exit(EXIT_FAILURE);
    }
    // get rid of the last slash, if not root dir
    if (abs_path[abs_path_len - 1] == '/' && strcmp(abs_path,"/") != 0) {
        // trailing 0
        abs_path[abs_path_len - 1] = 0;
    }
    // read disk into memory
    disk = mmap(NULL, NUM_BLOCKS * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, img, 0);
    // if map failed
    if (disk == MAP_FAILED) {
        // error
        perror("mmap error");
        // exit
        exit(EXIT_FAILURE);
    }
    // get the number of blocks required for the file
    int file_blocks = file_size / EXT2_BLOCK_SIZE + 1;
    // save the original number of file blocks
    int orig_file_blocks = file_blocks;
    // if the original blocks are more than 12
    if (orig_file_blocks > NUM_DIRECT_POINTERS) {
        // need one more pointer block
        file_blocks++;
    }
    // group descriptor
    struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
    // inodes
    void* inodes = (disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
    // get the bitmap
    char* inode_bitmap = (char*) (disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
    // get the inode of the target path
    int target_inode = get_dirs_inode(abs_path,inodes);
    // check if exists
    if (target_inode == FAILED) {
        // error msg
        fprintf(stderr,"Target path does not exist.\n");
        // no entry
        return ENOENT;
    // target path is not a directory
    } else if (target_inode == NOT_DIRECTORY) {
        // error msg
        fprintf(stderr,"Target path is not a directory.\n");
        // exit
        return ENOTDIR;
    }
    // get a free inode for the file being copied
    int free_inode = get_free_inode_or_block(gd,INODES);
    // check if found
    if (free_inode == NOT_FOUND) {
        // error msg
        fprintf(stderr,"No free inodes available.\n");
        // return no space error
        return ENOSPC;
    }
    // copy over the src path to a string
    int src_path_len = strlen(argv[2]);
    // empty string to copy abs path to
    char src_path[src_path_len + 1];
    // copy the abs path to the string
    memcpy(src_path,argv[2],src_path_len);
    // file name length
    int file_name_length = 0;
    // couner var
    int i;
    // go through the source path
    for (i = 0; i < src_path_len; i++) {
        // pass a slash, reset
        if (src_path[i] == '/') {
            // reset
            file_name_length = 0;
        // didnt pass a slash
        } else {
            // counter the file name's length
            file_name_length++;
        }
    }
    // string to hold parent
    char* src_parent = strdup("");
    // string to hold filename
    char* src_name = strdup("");
    // split the path into parent and name
    split_dir(src_path,src_parent,src_name);
    // the path to where the file is going
    char* src_file_target_path = strdup("");
    // copy over all the strings to make the path
    strcpy(src_file_target_path,strcat(strcat(abs_path,"/"),src_name));
    // trailing 0
    src_file_target_path[file_name_length + abs_path_len + 1] = 0;
    // trailing 0 on src_name
    src_name[file_name_length] = '\0';
    // var to store the blocks that were use
    int used_blocks[NUM_DIRECT_POINTERS + 1];
    // counter var
    int k;
    // go through used blocks
    for (k = 0;k < NUM_DIRECT_POINTERS + 1;k++) {
        // 0 out the vals
        used_blocks[i] = 0;
    }
    write_file_content(gd,inodes,src_file,file_size,orig_file_blocks,used_blocks);
    // set the default values of the inode using our func
    set_inode(inodes,free_inode,EXT2_S_IFREG,file_size,1,file_blocks*2,used_blocks[0]-1);
    // get the free inode that we're going to use for the file
    struct ext2_inode* file_inode = (struct ext2_inode*)(inodes + sizeof(struct ext2_inode) * free_inode);
    // go through the used blocks
    for (i = 1;i < file_blocks; i++) {
        // set the block number to the block that we used
        file_inode->i_block[i] = used_blocks[i];
    }
    // if we need to use the indirect pointer
    if (orig_file_blocks > NUM_DIRECT_POINTERS) {
        // set the indirect pointer
        file_inode->i_block[SINGLE_INDIRECT_POINTER] = used_blocks[SINGLE_INDIRECT_POINTER];
    }
    // let the inode bitmap know that this inode is taken
    set_bitmap_at_index(inode_bitmap,free_inode,1);
    // took up an inode
    gd->bg_free_inodes_count--;
    // get parent inode
    struct ext2_inode* target_parent = (struct ext2_inode*)(inodes + sizeof(struct ext2_inode) * target_inode);
    // another link
    target_parent->i_links_count++;
    // add the directory entry to the parent
    add_dir_entry(src_name,gd,target_inode,free_inode,inodes,target_parent,EXT2_FT_REG_FILE);
}
