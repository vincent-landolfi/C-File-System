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

int get_next_dir(char* dir) {
    // index var
    int i = 1;
    // length of dir var
    int dir_length = strlen(dir);
    // base case, no paths left
    if (dir_length == 1) {
        // next dir is 0
        return 0;
    }
    // go through dir string, looking for /
    while(i < dir_length && dir[i] != '/') {
        // increment index
        i++;
    }
    // return the next /
    return i;
}

int get_dirs_inode(char* dir, struct ext2_inode* inodes){
    // current inode location
    int inode_loc = EXT2_ROOT_INO;
    // current position in the dir string
    int curr_dir_position;
    // while there are still more dirs to go through in the dir string
    while((curr_dir_position = get_next_dir(dir)) > 1) {
        // char array to hold current dir
        char curr_dir[curr_dir_position+1];
        // index var
        int i;
        // loop through remaining path, copy values to curr dir
        for(i = 0; i < curr_dir_position + 1; i++) {
            // copy values
            curr_dir[i] = dir[i + 1];
        }
        // trailing 0
        curr_dir[curr_dir_position - 1] = 0;
        // add offset to dir string
        dir += curr_dir_position;
        // trailing 0
        dir[strlen(dir)] = 0;
        // get inode location of current directory
        inode_loc = get_inode_loc(curr_dir,inode_loc-1,inodes);
        // check if error
        if (inode_loc == FAILED || inode_loc == NOT_DIRECTORY) {
            // no entry
            return inode_loc;
        } 
    }
    // return inode location
    return inode_loc - 1;
}

int get_inode_loc(char* dir, int loc, struct ext2_inode* inodes) {
    // loop counter
    int i;
    // go through all the inode pointers
    for (i = 0; i < NUM_INODE_POINTERS; i++) {
        // instatiate size
        int size = 0;
        // get the block number
        int block_num = inodes[loc].i_block[i];
        // if block number is 0
        if (block_num == 0) {
            // break out of loop
            break;
        }
        // get size of inode
        int size_total = inodes[loc].i_size;
        // get the directory from the block number
        struct ext2_dir_entry_2* curr_dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
        // while we haven't gone through the entire inode
        while(size < size_total) {
            // add rec len to current size
            size += curr_dir->rec_len;
            // if curr dir is a directory, has same name is inputted dir, and has same length as inputted dir
            if (curr_dir->file_type == EXT2_FT_DIR && strlen(dir) == curr_dir->name_len 
                && strncmp(dir, curr_dir->name, curr_dir->name_len) == 0) {
                // new inode location is location of current directorys inode
                return curr_dir->inode;
            } else if (strlen(dir) == curr_dir->name_len && strncmp(dir, curr_dir->name, curr_dir->name_len) == 0) {
                // new inode location is location of current directorys inode
                //return curr_dir->inode;
                return NOT_DIRECTORY;
            }
            // set the curr dir to the next directory
            curr_dir = (void*)curr_dir + curr_dir->rec_len;
        }
    }
    // return location of inode
    return FAILED;
}

int get_file_symlink_inode(char* file_name,int parent_inode,struct ext2_inode* inodes_struct) {
    // loop counter
    int i;
    // size counter
    int size = 0;
    // go through all the inode pointers
    for (i = 0; i < NUM_INODE_POINTERS; i++) {
        // get the block number
        int block_num = inodes_struct[parent_inode].i_block[i];
        // if block number is 0
        if (block_num == 0) {
            // break out of loop
            break;
        }
        // get size of inode
        int size_total = inodes_struct[parent_inode].i_size;
        // get the directory from the block number
        struct ext2_dir_entry_2* curr_dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
        // while we haven't gone through the entire inode and we havent found the child to be
        // removed's inode
        while(size < size_total) {
            // add rec len to current size
            size += curr_dir->rec_len;
            // set the curr dir to the next directory
            curr_dir = (void*)curr_dir + curr_dir->rec_len;
            if (strlen(file_name) == curr_dir->name_len && strncmp(file_name, curr_dir->name, curr_dir->name_len) == 0) {
                return curr_dir->inode;
            }
        }
    }
    // failed
    return FAILED;
}

void print_ls(int dir_inode,struct ext2_inode* inodes,int flag) {
    // counter
    int i;
    // go through inode pointers
    for (i = 0; i < NUM_INODE_POINTERS; i++) {
        // file name
        char* file_name;
        // instantiate size
        int size = 0;
        // block number
        int block_num = inodes[dir_inode].i_block[i];
        // block num 0
        if (block_num == 0) {
            // break out of loop
            break;
        }
        // total size
        int size_total = inodes[dir_inode].i_size;
        // get the dir from the block number
        struct ext2_dir_entry_2* dir = (struct ext2_dir_entry_2*)(disk + EXT2_BLOCK_SIZE * block_num);
        // while we havent gone through the whole inode table
        while (size < size_total) {
            // set file name
            file_name = dir->name;
            // for loop counter
            int j;
            // if no -a and file name is not . or .., or -a
            if ((!flag && strcmp(file_name,".") != 0 && strcmp(file_name,"..") != 0) || flag) {
                // go through file name for dir
                for (j = 0;j < dir->name_len; j++) {
                    // print chars from dir
                    printf("%c",file_name[j]);
                }
                // if we printed any chars
                if (j > 0) {
                    // new line
                    printf("\n");
                }
            }
            // add the curr dirs reclen to the size
            size += dir->rec_len;
            // next dir
            dir = (void*)dir + dir->rec_len;
        }
    }
}

int get_free_inode_or_block(struct ext2_group_desc* gd,int type) {
    // size
    int size;
    // start
    void* bitmap_start;
    // getting free block
    if (type == BLOCKS) {
        // number of blocks
        size = NUM_BLOCKS;
        // start at block bitmap
        bitmap_start = (disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
    // getting free inode
    } else {
        // number of inodes
        size = NUM_INODES;
        // start at block bitmap
        bitmap_start = (disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
    }
    // loop counter
    int i;
    // go through the entire map
    for (i = 0; i < size; i++) {
        // get the current byte
        char* curr_byte = bitmap_start + (i/8);
        // if the byte is 0, i.e. its free
        if (!((*curr_byte >> (i%8)) & 1)) {
            // return its index
            return i;
        }
    }
    // not found
    return NOT_FOUND;
}

void set_dir(struct ext2_dir_entry_2* dir, char* name, int rec_len,int type,int free_inode) {
    // set name
    strcpy(dir->name,name);
    // length
    dir->name_len = strlen(name);
    // inode is free inode
    dir->inode = free_inode + 1;
    // type
    dir->file_type = type;
    // rec_len
    dir->rec_len = rec_len;
}

void set_inode(void* inodes, int free_inode, int mode, int size, int links,int blocks, int free_block) {
    // get the inode
    struct ext2_inode* dir_inode = inodes + sizeof(struct ext2_inode) * free_inode;
    // directory
    dir_inode->i_mode = mode;
    // one block worth
    dir_inode->i_size = size;
    // 2 links . and ..
    dir_inode->i_links_count = links;
    // 2 blocks . and ..
    dir_inode->i_blocks = blocks;
    // point to the blocks
    dir_inode->i_block[0] = free_block + 1;
}

void split_dir(char* dir,char* parent,char* last_in_path) {
    // loop counter
    int i;
    // index for the last directory/file
    int path_count = 0;
    // loop through directory chars
    for (i = 0; i < strlen(dir); i++) {
        // copy directory chars to parent string
        parent[i] = dir[i];
        // copy directory chars to last in path string (file/dir)
        last_in_path[path_count] = dir[i];
        // if we get a slash
        if (dir[i] == '/') {
            // loop counter
            int j;
            // loop through last in path
            for (j = 0; j < strlen(dir); j++) {
                // clear out the string
                last_in_path[j] = '\0';
            }
            // reset the path count, start at [0] again for the last in path
            path_count = -1;
        }
        // increase the index for the last_in_path setter
        path_count++;
    }
    // trailing 0, to get rid of the last dir/file in the path,
    // which will give us only the parent
    parent[strlen(parent) - strlen(last_in_path) - 1] = 0;
    // if the parent was completely cleared out
    if (parent[0] == 0) {
        // its the root directory
        parent[0] = '/';
        // stop the string right after the slash
        parent[1] = 0;
    }
}

void set_bitmap_at_index(char* start_index,int offset, int val) {
    // get current byte using offset
    char* curr_byte = start_index + (offset/8);
    // set the value of the curretn byte
    *curr_byte = ( (*curr_byte & ~(1 << (offset%8))) | (val << (offset%8)));
}

int add_dir_entry(char* dir,struct ext2_group_desc* gd,int parent_idx, int inode_idx,void* inodes,struct ext2_inode* parent,int file_type) {
    // loop counter
    int i;
    // get the inode for the dir
    //struct ext2_inode* dir_inode = (struct ext2_inode*)(inodes + sizeof(struct ext2_inode) * inode_idx);
    // size we need
    int needed_size;
    // go through all the inode pointers
    for (i = 0; i < NUM_INODE_POINTERS; i++) {
        // instatiate size
        int size = 0;
        // get the block number
        int block_num = parent->i_block[i];
        // if block number is 0
        if (!block_num) {
            // break out of loop
            break;
        }
        // get size of inode
        int size_total = EXT2_BLOCK_SIZE;
        // get the directory from the block number
        struct ext2_dir_entry_2* curr_dir = (void*)(disk + EXT2_BLOCK_SIZE * block_num);
        // while we haven't gone through the entire inode
        while(size < size_total) {
            // add rec len to current size
            size += curr_dir->rec_len;
            // needed size with byte adjustment
            needed_size = byte_align(strlen(dir)) + (2 * BYTE_SIZE) + byte_align(curr_dir->name_len);
            // if curr dir is a directory, has same name is inputted dir, and has same length as inputted dir
            if (size == EXT2_BLOCK_SIZE && curr_dir->rec_len >= needed_size) {
                // old rec len
                int old_rec_len = curr_dir->rec_len;
                // new rec len
                int new_rec_len = byte_align(curr_dir->name_len) + BYTE_SIZE;
                // set rec len by byte aligning
                curr_dir->rec_len = new_rec_len;
                // move to next dir
                curr_dir = (void*)curr_dir + curr_dir->rec_len;
                // rec len is old rec len - new
                curr_dir->rec_len = old_rec_len - new_rec_len;
                // name
                memcpy(curr_dir->name,dir,strlen(dir));
                // name length
                curr_dir->name_len = strlen(dir);
                // use the given inode
                curr_dir->inode = inode_idx + 1;
                // its a dir
                curr_dir->file_type = file_type;
                // get the inode for this dir
                struct ext2_inode* curr_inode = (inodes + sizeof(struct ext2_inode) * curr_dir->inode);
                // increase number of links
                curr_inode->i_links_count++;
                // return success
                return FOUND;
            }
            // set the curr dir to the next directory
            curr_dir = (void*)curr_dir + curr_dir->rec_len;
        }
    }
    // didnt find our dir
    // get a free block
    int free_block = get_free_inode_or_block(gd, BLOCKS);
    // check for error
    if (free_block == NOT_FOUND) {
        // error msg
        fprintf(stderr,"Not enough free blocks.\n");
        // exit
        exit(ENOSPC);
    }
    // get dir at free block
    struct ext2_dir_entry_2* free_dir = (void*) (disk + EXT2_BLOCK_SIZE * free_block);
    // set inode
    free_dir->inode = inode_idx + 1;
    // len is dirs name len
    free_dir->name_len = strlen(dir);
    // take up the whole block
    free_dir->rec_len = EXT2_BLOCK_SIZE;
    // directory
    free_dir->file_type = EXT2_FT_DIR;
    // store in the name
    memcpy((void*)free_dir + BYTE_SIZE, dir, strlen(dir));
    // get block bitmap
    char* bitmap = (char*) (disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
    // set block as taken
    set_bitmap_at_index(bitmap,free_block,1);
    // update parent data
    parent->i_blocks += 2;
    // another block
    parent->i_size += EXT2_BLOCK_SIZE;
    // set the next block
    parent->i_block[(parent->i_size / EXT2_BLOCK_SIZE)] = free_block + 1;
    // not found
    return NOT_FOUND;
}

int byte_align(int num) {
    // rounds up to the nearest 4
    return (num + 3) / 4 * 4;
}

void write_file_content(struct ext2_group_desc* gd, struct ext2_inode* inodes,FILE* src_file,int file_size,int orig_file_blocks,int* used_blocks) {
    // start index
    char* start_index = (char*)(disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
    // need to store the leftover file size to copy
    int file_leftover = file_size;
    // loop counter
    int i = 0;
    // while we havent wrote the whole file, and there is file leftover
    while (i < orig_file_blocks && file_leftover > 0) {
        // get the next free index
        int free_index = get_free_inode_or_block(gd,BLOCKS);
        // store the used block
        used_blocks[i] = free_index;
        // check for errors
        if (free_index == FAILED) {
            // error msg
            fprintf(stderr,"No free blocks available.\n");
            // no space
            exit(ENOSPC);
        }
        // update bitmap that the spot is taken, i.e. value of 1
        set_bitmap_at_index(start_index, free_index, 1);
        // took up a block
        gd->bg_free_blocks_count--;
        // if there is more than a block's worth of data to store
        if (file_leftover > EXT2_BLOCK_SIZE) {
            // copy in a block's worth of data
            fread(disk + EXT2_BLOCK_SIZE * (free_index + 1), EXT2_BLOCK_SIZE, 1, src_file);
            // block's worth of data no longer left over
            file_leftover -= EXT2_BLOCK_SIZE;
        // less than a block's worth of data left
        } else {
            // copy in the remaining data
            fread(disk + EXT2_BLOCK_SIZE * (free_index + 1), file_leftover, 1, src_file);
            // nothing remaining
            file_leftover = 0;
        }
        i++;
    }
    // if we have to deal with indirect pointers
    if (orig_file_blocks > NUM_DIRECT_POINTERS) {
        // get another free block
        int free_block = get_free_inode_or_block(gd,BLOCKS);
        // no free block
        if (free_block == FAILED) {
            // error msg
            fprintf(stderr,"No free blocks left.\n");
            // no space
            exit(ENOSPC);
        }
        // store the block in the used blocks array
        used_blocks[SINGLE_INDIRECT_POINTER] = free_block;
        // take up a space in the bitmap for the master pointer
        set_bitmap_at_index(start_index, free_block, 1);
        // took up a block
        gd->bg_free_blocks_count--;
        // get the block
        int *indirect = (int *)(disk + EXT2_BLOCK_SIZE * (free_block + 1));
        // counter
        int i;
        // go through the blocks the indirect block points to
        for (i = 0; i < NUM_DIRECT_POINTERS; i++) {
            // get the next free block
            indirect[i] = get_free_inode_or_block(gd,BLOCKS);
            // no free block
            if (indirect[i] == FAILED) {
                // error msg
                fprintf(stderr,"No free blocks left.\n");
                // no space
                exit(ENOSPC);
            }
            // if there is more than a block's worth of data to store
            if (file_leftover > EXT2_BLOCK_SIZE) {
                // copy in a block's worth of data
                fread(disk + EXT2_BLOCK_SIZE * indirect[i], EXT2_BLOCK_SIZE, 1, src_file);
                // block's worth of data no longer left over
                file_leftover -= EXT2_BLOCK_SIZE;
            // less than a block's worth of data left
            } else {
                // copy in the remaining data
                fread(disk + EXT2_BLOCK_SIZE * indirect[i], file_leftover, 1, src_file);
                // nothing remaining
                file_leftover = 0;
            }
        }
    }
}
