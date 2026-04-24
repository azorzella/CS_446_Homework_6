#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/mman.h>

#define BLKSIZE 4048

const int root_inode_number = 2;  // 0 is "invalid", 1 is reserved for badsectors data
const int badsectors_inode_number = 1;  // 1 for badsector data
const int invalid_inode_number = 0;  // 0 is "invalid"

const int root_datablock_number = 0;  // 1 because 0 reserved for badsectors data

typedef struct _block_t {
  char data[BLKSIZE];
} block_t;

typedef struct _inode_t {
  //int uid;
  //int gid;
  //char rwx;
  int size;  // of file in bytes
  int blocks;  // blocks allocated to this file - used or not
  //struct timeval atime;
  //struct timeval mtime;
  struct timeval ctime;  // creation time
  //int links_count;  // hardlinks
  block_t* data[15];  // 12 direct, 1 single-indirect, 1 double-indirect, 1 triple-indirect
} inode_t;

typedef struct _dirent_t {
  int inode;  // entry's inode number
  char file_type;  // entry's file type (0: unknown, 1: file, 2: directory)
  int name_len;  // entry's name length ('\0'-excluded)
  char name[255];  // entry's name ('\0'-terminated)
} dirent_t;

typedef struct _superblock_info_t {
  int blocks;  // total number of filesystem data blocks
  char name[255];  // name identifier of filesystem
} superblock_info_t;

union superblock_t {
  block_t block;  // ensures superblock_t size matches block_t size (1 block)
  superblock_info_t superblock_info;  // to access the superblock info
};

typedef struct _groupdescriptor_info_t {
  inode_t* inode_table;  // location of inode_table (first inode_t in region)
  block_t* block_data;  // location of block_data (first block_t in region)
} groupdescriptor_info_t;

union groupdescriptor_t {
  block_t block;  // ensures groupdescriptor_t size matches block_t size (1 block)
  groupdescriptor_info_t groupdescriptor_info;  // to access the groupdescriptor info
};

typedef struct _myfs_t {
  union superblock_t super;  // superblock
  union groupdescriptor_t groupdescriptor;  // groupdescriptor
  block_t bmap;  // (free/used) block bitmap
  block_t imap;  // (free/used) inode bitmap
} myfs_t;

myfs_t* my_mkfs(int size, int maxfiles);
void my_dumpfs(myfs_t* myfs);
void dump_dirinode(myfs_t* myfs, int inode_number, int level);
void my_crawlfs(myfs_t* myfs);
void my_creatdir(myfs_t* myfs, int cur_dir_inode_number, const char* new_dirname);

int roundup(int x, int y) {
  return x == 0 ? 0 : 1 + ((x - 1) / y);
}


int main(int argc, char *argv[]){

  inode_t* cur_dir_inode = NULL;

  myfs_t* myfs = my_mkfs(100*BLKSIZE, 10);

  // create 2 dirs inside [/] (root dir)
  int cur_dir_inode_number = 2;  // root inode
  my_creatdir(myfs, cur_dir_inode_number, "mystuff");  // will be inode 3
  my_creatdir(myfs, cur_dir_inode_number, "homework");  // will be inode 4

  // Extensive Test
  my_creatdir(myfs, cur_dir_inode_number, "socrates");  // will be inode 5
  my_creatdir(myfs, cur_dir_inode_number, "steam");  // will be inode 6
  my_creatdir(myfs, cur_dir_inode_number, "linux");  // will be inode 7

  cur_dir_inode_number = 6;
  my_creatdir(myfs, cur_dir_inode_number, "stephensausage");  // will be inode 8
  my_creatdir(myfs, cur_dir_inode_number, "baba");  // will be inode 9
  my_creatdir(myfs, cur_dir_inode_number, "thewitness");  // will be inode 10

  cur_dir_inode_number = 8;
  my_creatdir(myfs, cur_dir_inode_number, "favorites");  // will be inode 11
  cur_dir_inode_number = 9;
  my_creatdir(myfs, cur_dir_inode_number, "saves");  // will be inode 12
  
  cur_dir_inode_number = 7;
  my_creatdir(myfs, cur_dir_inode_number, "debian");  // will be inode 13
  my_creatdir(myfs, cur_dir_inode_number, "ubuntu");  // will be inode 14
  my_creatdir(myfs, cur_dir_inode_number, "amogos");  // will be inode 15
  my_creatdir(myfs, cur_dir_inode_number, "arch");  // will be inode 16
  my_creatdir(myfs, cur_dir_inode_number, "kubuntu");  // will be inode 17
  my_creatdir(myfs, cur_dir_inode_number, "mint");  // will be inode 18
  
  cur_dir_inode_number = 11;
  my_creatdir(myfs, cur_dir_inode_number, "tower");  // will be inode 19
  cur_dir_inode_number = 8;
  my_creatdir(myfs, cur_dir_inode_number, "saves");  // will be inode 20
  
  // // create 1 dir inside [/homework] dir
  // cur_dir_inode_number = 4;
  // my_creatdir(myfs, cur_dir_inode_number, "assignment5");  // will be inode 5

  // // create 1 dir inside [/homework/assignment5] dir
  // cur_dir_inode_number = 5;
  // my_creatdir(myfs, cur_dir_inode_number, "mycode");  // will be inode 6

  // // create 1 dir inside [/homework/mystuff] dir
  // cur_dir_inode_number = 3;
  // my_creatdir(myfs, cur_dir_inode_number, "mydata");  // will be inode 7

  // Don't forget to uncomment this:
  printf("\nDumping filesystem structure:\n");
  my_dumpfs(myfs);

  printf("\nCrawling filesystem structure:\n");
  my_crawlfs(myfs);

  // destroy filesystem
  free(myfs);

  return 0;
}

myfs_t* my_mkfs(int size, int maxfiles) {
  int num_data_blocks = roundup(size, BLKSIZE);

  int num_inode_table_blocks = roundup(maxfiles*sizeof(inode_t), BLKSIZE);  // Note: not quite, inode should
                                                                            // not be split between blocks

  size_t fs_size = sizeof(myfs_t) +  // superblock_t + groupdescriptor_t + block bitmap + inode bitmap
                   num_inode_table_blocks * sizeof(block_t) +  // inode_table
                   num_data_blocks * sizeof(block_t);  // data_blocks

  void *ptr;
  //int retval;
  //retval = posix_memalign(&ptr, /*(size_t) sysconf(_SC_PAGESIZE)*/BLKSIZE, fs_size);
  //if (retval) {
  //  printf("posix_memalign failed\n");
  //  return NULL;
  //}
  ptr = malloc(fs_size);
  if (ptr == NULL) {
    printf("malloc failed\n");
    return NULL;
  }
  if (mlock(ptr, size)) {
    printf("mlock failed\n");
    free(ptr);
    return NULL;
  }

  myfs_t* myfs = (myfs_t*)ptr;

  // superblock
  void *super_ptr = calloc(BLKSIZE, sizeof(char));
  // read-in (not required, we are creating filesystem for first time, also zeroed because using calloc)
  union superblock_t* super = (union superblock_t*)super_ptr;
  super->superblock_info.blocks = num_data_blocks;
  strcpy(super->superblock_info.name, "MYFS");
  // write out to fs
  memcpy((void*)&myfs->super, super_ptr, BLKSIZE);

  // groupdescriptor
  void *groupdescriptor_ptr = calloc(BLKSIZE, sizeof(char));
  // read-in (not required, we are creating filesystem for first time, also zeroed because using calloc)
  union groupdescriptor_t* groupdescriptor = (union groupdescriptor_t*)groupdescriptor_ptr;
  groupdescriptor->groupdescriptor_info.inode_table = (inode_t*)((char*)ptr +
                                                                 sizeof(myfs_t));
  groupdescriptor->groupdescriptor_info.block_data = (block_t*)((char*)ptr +
                                                                sizeof(myfs_t) +
                                                                num_inode_table_blocks * sizeof(block_t));
  // write out to fs
  memcpy((void*)&myfs->groupdescriptor, groupdescriptor_ptr, BLKSIZE);

  // create filesystem root

  // inode
  void *inodetable_ptr = calloc(BLKSIZE, sizeof(char));
  // read-in (not required, we are creating filesystem for first time, also zeroed because using calloc)
  inode_t* inodetable = (inode_t*)inodetable_ptr;
  inodetable[root_inode_number].size = 2 * sizeof(dirent_t);  // will contain 2 direntries ('.' and '..') at initialization
  inodetable[root_inode_number].blocks = 1;  // will only take up 1 block (for just 2 direntries: '.' and '..') at initialization
  for (uint i=1; i<15; ++i)  // initialize all data blocks to NULL (1 data block only needed at initialization)
    inodetable[root_inode_number].data[i] = NULL;
  inodetable[root_inode_number].data[0] = &(groupdescriptor->groupdescriptor_info.block_data[root_datablock_number]);
  // write out to fs
  memcpy((void*)groupdescriptor->groupdescriptor_info.inode_table, inodetable_ptr, BLKSIZE);

  // data (dir)
  void *dir_ptr = calloc(BLKSIZE, sizeof(char));
  // read-in (not required, we are creating filesystem for first time, also zeroed because using calloc)
  dirent_t* dir = (dirent_t*)dir_ptr;
  // dirent '.'
  dirent_t* root_dirent_self = &dir[0];
  {
  root_dirent_self->name_len = 1;
  root_dirent_self->inode = root_inode_number;
  root_dirent_self->file_type = 2;
  strcpy(root_dirent_self->name, ".");
  }
  // dirent '..'
  dirent_t* root_dirent_parent = &dir[1];
  {
  root_dirent_parent->name_len = 2;
  root_dirent_parent->inode = root_inode_number;
  root_dirent_parent->file_type = 2;
  strcpy(root_dirent_parent->name, "..");
  }
  // write out to fs
  memcpy((void*)(inodetable[root_inode_number].data[0]), dir_ptr, BLKSIZE);

  // data  bitmap
  void* bmap_ptr = calloc(BLKSIZE, sizeof(char));
  // read-in (not required, we are creating filesystem for first time, also zeroed because using calloc)
  block_t* bmap = (block_t*)bmap_ptr;
  bmap->data[root_datablock_number / 8] |= 0x1<<(root_datablock_number % 8);
  // write out to fs
  memcpy((void*)&myfs->bmap, bmap_ptr, BLKSIZE);

  // inode  bitmap
  void* imap_ptr = calloc(BLKSIZE, sizeof(char));
  // read-in (not required, we are creating filesystem for first time, also zeroed because using calloc)
  block_t* imap = (block_t*)imap_ptr;
  imap->data[root_inode_number / 8] |= 0x1<<(root_inode_number % 8);
  imap->data[badsectors_inode_number / 8] |= 0x1<<(badsectors_inode_number % 8);
  imap->data[invalid_inode_number / 8] |= 0x1<<(invalid_inode_number % 8);
  // write out to fs
  memcpy((void*)&myfs->imap, imap_ptr, BLKSIZE);

  return myfs;
}

void my_dumpfs(myfs_t* myfs) {
  printf("superblock_info.name: %s\n", myfs->super.superblock_info.name);
  printf("superblock_info.blocks: %d\n", myfs->super.superblock_info.blocks);
  printf("groupdescriptor_info.inode_table: %p\n", myfs->groupdescriptor.groupdescriptor_info.inode_table);
  printf("groupdescriptor_info.block_data: %p\n", myfs->groupdescriptor.groupdescriptor_info.block_data);
  for (size_t byte = 0; byte < BLKSIZE; ++byte) {
    for (uint bit = 0; bit < 8; ++bit) {
      if (myfs->imap.data[byte] & (0x1 << bit)) {
        int inode_number = byte*8 + bit;
        if (inode_number < root_inode_number) {  // 0, 1 inode numbers are reserved for "invalid" and badsectors data, skip
          continue;
        }
        if (inode_number == root_inode_number) {  // root inode is known to be a directory, safely assume data holds direntries
          printf("  ROOT inode %d occupied - initialized filesystem found!\n", inode_number);
          dump_dirinode(myfs, inode_number, 0);
        }
      }
    }
  }
}

#define LEVEL_TAB for(int o=0; o<4*level; ++o){ printf(" "); }
#define NEXTLEVEL_TAB for(int o=0; o<4*(level+1); ++o){ printf(" "); }
void dump_dirinode(myfs_t* myfs, int inode_number, int level) {
  inode_t* inodetable = myfs->groupdescriptor.groupdescriptor_info.inode_table;
  LEVEL_TAB printf("  inode.size: %d\n", inodetable[inode_number].size);
  LEVEL_TAB printf("  inode.blocks: %d\n", inodetable[inode_number].blocks);
  for (uint block_nr = 0; block_nr < 11; ++block_nr) {  // 12 first blocks are direct (only deal with direct blocks for simplicity)
    if (inodetable[inode_number].data[block_nr] != NULL) {
      int num_direntries = inodetable[inode_number].size / sizeof(dirent_t);  // get current number of direntries
      LEVEL_TAB printf("    num_direntries: %d\n", num_direntries);
      dirent_t* direntries = (dirent_t*)(inodetable[inode_number].data[block_nr]);
      for (size_t dirent_num = 0; dirent_num < num_direntries; ++dirent_num) {
        LEVEL_TAB printf("    direntries[%ld].inode: %d\n", dirent_num, direntries[dirent_num].inode);
        LEVEL_TAB printf("    direntries[%ld].name: %s\n", dirent_num, direntries[dirent_num].name);
        LEVEL_TAB printf("    direntries[%ld].file_type: %s\n", dirent_num, (int)direntries[dirent_num].file_type==2?"folder":(int)direntries[dirent_num].file_type==1?"file":"unknown");
        if ((int)direntries[dirent_num].file_type == 2) {  // folder type
          if (strcmp(direntries[dirent_num].name,".") && strcmp(direntries[dirent_num].name,"..")) {  // don't dump direntries for slef ('.') and parent ('..'), these are cycles
            NEXTLEVEL_TAB printf("  inode %d occupied:\n", direntries[dirent_num].inode);
            dump_dirinode(myfs, direntries[dirent_num].inode, level+1);
          }
        }
        if ((int)direntries[dirent_num].file_type == 1) {  // file type
            LEVEL_TAB printf("    FILE:\n");
            LEVEL_TAB printf("    inode.size: %d\n", inodetable[direntries[dirent_num].inode].size);
            LEVEL_TAB printf("    inode.blocks: %d\n", inodetable[direntries[dirent_num].inode].blocks);
            LEVEL_TAB printf("    inode.data[0]: "); for (int i=0; i<inodetable[direntries[dirent_num].inode].size; ++i) { printf("%c", ((char*)inodetable[direntries[dirent_num].inode].data[0])[i]); } printf("\n"); // only print out block[0] for simplicity
        }
      }
    }
  }
}

#define LEVEL_TREE for(int o=0; o<2*level; ++o){ printf(" "); } printf("|"); for(int o=0; o<2*level; ++o){ printf("_"); }
void crawl_dirinode(myfs_t* myfs, int inode_number, int level) {
  inode_t* inodetable = myfs->groupdescriptor.groupdescriptor_info.inode_table;
  for (uint block_nr = 0; block_nr < 11; ++block_nr) {  // 12 first blocks are direct (only deal with direct blocks for simplicity)
    if (inodetable[inode_number].data[block_nr] != NULL) {
      int num_direntries = inodetable[inode_number].size / sizeof(dirent_t);  // get current number of direntries
      dirent_t* direntries = (dirent_t*)(inodetable[inode_number].data[block_nr]);
      for (size_t dirent_num = 0; dirent_num < num_direntries; ++dirent_num) {
        LEVEL_TREE printf("_ %s\n", direntries[dirent_num].name);
        if ((int)direntries[dirent_num].file_type == 2) {  // folder type
          if (strcmp(direntries[dirent_num].name,".") && strcmp(direntries[dirent_num].name,"..")) {  // don't dump direntries for slef ('.') and parent ('..'), these are cycles
            crawl_dirinode(myfs, direntries[dirent_num].inode, level+1);
          }
        }
      }
    }
  }
}

void my_crawlfs(myfs_t* myfs) {
  for (size_t byte = 0; byte < BLKSIZE; ++byte) {
    for (uint bit = 0; bit < 8; ++bit) {
      if (myfs->imap.data[byte] & (0x1 << bit)) {
        int inode_number = byte*8 + bit;
        if (inode_number < root_inode_number) {  // 0, 1 inode numbers are reserved for "invalid" and badsectors data, skip
          continue;
        }
        if (inode_number == root_inode_number) {  // root inode is known to be a directory, safely assume data holds direntries
          printf("/\n");
          crawl_dirinode(myfs, inode_number, 0);
        }
      }
    }
  }
}

void my_creatdir(myfs_t* myfs, int cur_dir_inode_number, const char* new_dirname) {
  // ..............................................
  // Part 1: Find the First Free inode
  // Find the first available inode block index
  // and flag it as used
  // ..............................................

  block_t *imap = malloc(sizeof(block_t));
  memcpy(imap, &(myfs->imap), sizeof(block_t));

  int first_available_inode_index = -1;

  for (size_t byte = 0; byte < BLKSIZE; ++byte) {
    if(first_available_inode_index >= 0) {
      break;
    }

    for (uint bit = 0; bit < 8; ++bit) {
      if ((imap->data[byte] & (0x1 << bit)) == 0) {
        first_available_inode_index = byte*8 + bit;
        imap->data[byte] |= 0x1 << bit;
        break;
      }
    }
  }

  if(first_available_inode_index < 0) {
    printf("No available inode found\n");
    free(imap);
    return;
  }

  // Write back
  memcpy(&myfs->imap, imap, sizeof(block_t));
  free(imap);

  // ..............................................
  // Part 2: Find the First Free Block of Memory
  // Find the first available bmap block index
  // and flag it as used
  // ..............................................

  block_t *bmap = malloc(sizeof(block_t));
  memcpy(bmap, &(myfs->bmap), sizeof(block_t));

  int first_available_bnode_index = -1;

  for (size_t byte = 0; byte < BLKSIZE; ++byte) {
    if(first_available_bnode_index >= 0) {
      break;
    }

    for (uint bit = 0; bit < 8; ++bit) {
      if ((bmap->data[byte] & (0x1 << bit)) == 0) {
        first_available_bnode_index = byte*8 + bit;
        bmap->data[byte] |= 0x1 << bit;
        break;
      }
    }
  }

  if(first_available_bnode_index < 0) {
    printf("No available bnode found\n");
    free(bmap);
    return;
  }

  // Write back
  memcpy(&myfs->bmap, bmap, sizeof(block_t));
  free(bmap);

  // ..............................................
  // Part 3: Create a New Directory
  // Create a new directory and add it to the
  // filesystem
  // ..............................................

  // Copy the filesystem's inode table
  inode_t* inode_table = malloc(sizeof(inode_t) * (first_available_inode_index + 1));
  memcpy(inode_table, myfs->groupdescriptor.groupdescriptor_info.inode_table, sizeof(inode_t) * (first_available_inode_index));

  // Locally cache the parent and new directory inodes
  inode_t* parent_dir_inode = &inode_table[cur_dir_inode_number];
  inode_t* new_dir_inode = &inode_table[first_available_inode_index];

  // Increase the parent directory's size by the size of a directory
  parent_dir_inode->size += sizeof(dirent_t);

  // Initialize the new directory
  new_dir_inode->size = sizeof(dirent_t) * 2;
  new_dir_inode->blocks = 1;  
  new_dir_inode->data[0] = &(myfs->groupdescriptor.groupdescriptor_info.block_data[first_available_bnode_index]);

  memcpy(myfs->groupdescriptor.groupdescriptor_info.inode_table, inode_table, sizeof(inode_t) * (first_available_inode_index + 1));
  
  // 4
  int parent_inode_num = cur_dir_inode_number;
  inode_t *parent_inode = &inode_table[parent_inode_num];
  dirent_t* copy_new_dir_to = (dirent_t*)(parent_inode->data[0]);
  
  dirent_t* new_dirent_t = malloc(sizeof(dirent_t));
  {
    new_dirent_t->name_len = strlen(new_dirname);
    strcpy(new_dirent_t->name, new_dirname);
    new_dirent_t->file_type = 2;
    new_dirent_t->inode = first_available_inode_index;
  }
  
  parent_inode_num = cur_dir_inode_number;
  parent_inode = &inode_table[parent_inode_num];

  int parent_dir_count = parent_inode->size / sizeof(dirent_t);
  int new_dir_index = parent_dir_count - 1;
  memcpy(&(copy_new_dir_to[new_dir_index]), new_dirent_t, sizeof(dirent_t));

  // 5
  // As done above, allocate BLKSIZE worth of memory, all zeroed out
  void *dir_contents_ptr = calloc(BLKSIZE, sizeof(char));
  // Cast the region to directory entry
  dirent_t* dir_contents = (dirent_t*)dir_contents_ptr;
  
  dirent_t* self_ref = &dir_contents[0];
  {
    self_ref->name_len = 1;
    self_ref->inode = first_available_inode_index;
    self_ref->file_type = 2;
    strcpy(self_ref->name, ".");
  }
  
  dirent_t* parent_ref = &dir_contents[1];
  {
    parent_ref->name_len = 2;
    parent_ref->inode = cur_dir_inode_number;
    parent_ref->file_type = 2;
    strcpy(parent_ref->name, "..");
  }

  memcpy(&(myfs->groupdescriptor.groupdescriptor_info.block_data[first_available_bnode_index]), dir_contents, sizeof(dirent_t) * 2);

  free(inode_table);
}