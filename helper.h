#include "ext2.h"

#define NUM_BLOCKS 128
#define NUM_INODES 32
#define FAILED -1
#define NUM_INODE_POINTERS 15
#define NUM_DIRECT_POINTERS 12
#define NOT_DIRECTORY -2
#define BLOCKS 1
#define INODES 2
#define NOT_FOUND -1
#define FOUND 0
#define BYTE_SIZE 8
#define SUCCESS 0
#define SINGLE_INDIRECT_POINTER 12

int get_next_dir(char* dir);
int get_dirs_inode(char* dir, struct ext2_inode* inodes);
int get_inode_loc(char* dir, int loc, struct ext2_inode* inodes);
void print_ls(int dir_inode,struct ext2_inode* inodes,int flag);
void split_dir(char* dir,char* parent,char* last_in_path);
void set_bitmap_at_index(char* start_index,int offset, int val);
void set_inode(void* inodes, int free_inode, int mode, int size, int links,int blocks, int free_block);
void set_dir(struct ext2_dir_entry_2* dir, char* name, int rec_len,int type,int free_inode);
int add_dir_entry(char* dir,struct ext2_group_desc* gd,int parent_idx, int inode_idx,void* inodes,struct ext2_inode* parent,int file_type);
int byte_align(int num);
int get_free_inode_or_block(struct ext2_group_desc* gd,int type);
int get_file_symlink_inode(char* file_name,int parent_inode,struct ext2_inode* inodes_struct);
void write_file_content(struct ext2_group_desc* gd, struct ext2_inode* inodes,FILE* src_file,int file_size,int orig_file_blocks,int* used_blocks);