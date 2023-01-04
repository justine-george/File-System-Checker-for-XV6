// File System Checking
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>

#include "types.h"
#include "fs.h"

#define BLOCK_SIZE (BSIZE)

// function declarations
void validateRule1(char *addr, struct superblock *sb);
void validateRule2(char *addr, struct superblock *sb);
void validateRule3(char *addr, struct superblock *sb);
void validateRule4(char *addr, struct superblock *sb);
void validateRule5(char *addr, struct superblock *sb);
void validateRule6(char *addr, struct superblock *sb);
void validateRule7_8(char *addr, struct superblock *sb);
void validateRule9(char *addr, struct superblock *sb);
void validateRule10(char *addr, struct superblock *sb);
void validateRule11_12(char *addr, struct superblock *sb);

// main function
int main(int argc, char *argv[]) {
  int r, fsfd;
  char *addr;
  struct superblock *sb;
  struct stat st;
  
  // print proper usage of the program if no argument is passed
  if(argc < 2) {
    fprintf(stderr, "Usage: sample fs.img ...\n");
    exit(1);
  }

  // open the image file
  fsfd = open(argv[1], O_RDONLY);
  if(fsfd < 0) {
    fprintf(stderr, "image not found\n");
    exit(1);
  }
  
  // use fstat to get the file size
  r = fstat(fsfd, &st);
  if (r != 0) {
		perror("stat");
		exit(1);
	}

  // memory map image file
  addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);
  if (addr == MAP_FAILED) {
    perror("mmap failed");
    exit(1);
  }

  // read the super block
  sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);

  // validate rules 1 through 12
  validateRule1(addr, sb);
  validateRule2(addr, sb);
  validateRule3(addr, sb);
  validateRule4(addr, sb);
  validateRule5(addr, sb);
  validateRule6(addr, sb);
  validateRule7_8(addr, sb);
  validateRule9(addr, sb);
  validateRule10(addr, sb);
  validateRule11_12(addr, sb);

  exit(0);
}

/*
Rule 1:
  Not one of the valid types (T_FILE, T_DIR, T_DEV). 
  print ERROR: bad inode.
*/
void validateRule1(char *addr, struct superblock *sb) {
  int i;
  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  
  // iterate through all inodes
  for (i = 1; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->type == 0) // inode not in use
      continue;
    if (rInodeP->type == 1 || rInodeP->type == 2 || rInodeP->type == 3) // in use inodes
      continue;
    else {
      /*
      Rule 1:
        Not one of the valid types (T_FILE, T_DIR, T_DEV). 
        print ERROR: bad inode.
      */
      fprintf(stderr, "ERROR: bad inode.\n");
      exit(1);
    }
  }
}

/*
Rule 2:
  If the direct block is used and is invalid, print 
  ERROR: bad direct address in inode.
*/
void validateRule2(char *addr, struct superblock *sb) {
  int i;
  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  
  // get the final block used for bitmap
  // valid data blocks should be in the range (lastBitmapBlock, sb->ninodes)
  uint lastBitmapBlock = BBLOCK(IBLOCK(sb->ninodes - 1), sb->ninodes);

  // iterate through all inodes
  for (i = 1; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1 || rInodeP->type == 2 || rInodeP->type == 3) { // in use inodes
      if (rInodeP->size == 0)
        continue;
      // check direct blocks
      for (i = 0; i < NDIRECT; i++) {
        if (rInodeP->addrs[i] == 0) // unallocated
          continue;
        // within valid range
        if (rInodeP->addrs[i] > lastBitmapBlock && rInodeP->addrs[i] < sb->nblocks)
          continue;
        /*
        Rule 2:
          If the direct block is used and is invalid, print 
          ERROR: bad direct address in inode.
        */
        fprintf(stderr, "ERROR: bad direct address in inode.\n");
        exit(1);
      }
      // check indirect blocks
      uint indAddr = rInodeP->addrs[NDIRECT];
      if (indAddr == 0) // unallocated
        continue;
      else if (indAddr <= lastBitmapBlock || indAddr >= sb->nblocks) {
        /*
        Rule 2:
          if the indirect block is in use and is invalid, print 
          ERROR: bad indirect address in inode.
        */
        fprintf(stderr, "ERROR: bad indirect address in inode.\n");
        exit(1);
      } else {
        uint *indTemp = (uint *) (addr + indAddr * BLOCK_SIZE);
        for (i = 0; i < NINDIRECT; i++, indTemp++) {
          if ((*indTemp) == 0) // not allocated
            continue;
          // within valid range
          if (((*indTemp) > lastBitmapBlock) && ((*indTemp) < sb->nblocks))
            continue;
          /*
          Rule 2:
            if the indirect block is in use and is invalid, print 
            ERROR: bad indirect address in inode.
          */
          fprintf(stderr, "ERROR: bad indirect address in inode.\n");
          exit(1);
        }
      }
    }
  }
}

/*
Rule 3:
  Root directory exists, its inode number is 1, and the parent of the root 
  directory is itself. If not, print ERROR: root directory does not exist.
*/
void validateRule3(char *addr, struct superblock *sb) {
  int i, direntCount;
  // flags to validate rules
  bool parentItself = false, rootDir = false, rootDirInum = false;
  struct dirent *de;
  
  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  rInodeP++; // go to root inode
  direntCount = rInodeP->size/sizeof(struct dirent);

  // check root inode
  if (rInodeP->addrs != 0) {
    de = (struct dirent *) (addr + (rInodeP->addrs[0])*BLOCK_SIZE);
    if (de != 0) {
      rootDir = true;
      if (de->inum == 1)
        rootDirInum = true;
      // iterate through directory entries of the root inode
      for (i = 0; i < direntCount; i++,de++) {
        // ensure inode number of dirent .. is 1
        if (strcmp(de->name,"..") == 0 && de->inum == 1) {
          parentItself = true;
          break;
        }
      }
    }
  }
  if (!rootDir || !rootDirInum || !parentItself) {
    /*
    Rule 3:
      Root directory exists, its inode number is 1, and the parent of the root 
      directory is itself. If not, print ERROR: root directory does not exist.
    */
    fprintf(stderr, "ERROR: root directory does not exist.\n");
    exit(1);
  }
}

/*
Rule 4:
  Each directory contains . and .. entries, and the . entry points to 
  the directory itself. If not, print ERROR: directory not properly formatted.
*/
void validateRule4(char *addr, struct superblock *sb) {
  int i, j, k, direntCount;
  bool currEntry, parentEntry, currPToItself;
  struct dirent *de;
  
  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  direntCount = rInodeP[ROOTINO].size/sizeof(struct dirent);

  // iterate through all inodes
  for (i = 1; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->addrs == 0) // unallocated
      continue;
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1) { // directory inode
      if (rInodeP->addrs != 0) {
        currEntry = false;
        parentEntry = false;
        currPToItself = false;
        // check direct blocks
        for (j = 0; j < NDIRECT; j++) {
          if (rInodeP->addrs[j] == 0) // unallocated
            continue;
          de = (struct dirent *) (addr + (rInodeP->addrs[j])*BLOCK_SIZE);
          if (de != 0) {
            // iterate through dirents of the inode
            for (k = 0; k < direntCount; k++,de++) {
              if (de == 0 || de->inum == 0)
                continue;
              if (strcmp(de->name,".") == 0) {
                currEntry = true;
                // dirent . should have rInodeP's inode number (i-1)
                if (de->inum == i - 1)
                  currPToItself = true;
              }
              if (strcmp(de->name,"..") == 0)
                parentEntry = true;
              if (currEntry && parentEntry && currPToItself)
                break;
            }
          }
          if (currEntry && parentEntry && currPToItself)
            break;
        }
        // check indirect blocks
        uint indAddr = rInodeP->addrs[NDIRECT];
        if (indAddr == 0) { // unallocated
          if (!currEntry || !parentEntry || !currPToItself) {
            /*
            Rule 4:
              Each directory contains . and .. entries, and the . entry points to 
              the directory itself. If not, print ERROR: directory not properly formatted.
            */
            fprintf(stderr, "ERROR: directory not properly formatted.\n");
            exit(1);
          } else
            continue;
        }
        uint *indTemp = (uint *) (addr + indAddr * BLOCK_SIZE);
        for (j = 0; j < NINDIRECT; j++, indTemp++) {
          if ((*indTemp) == 0) // not allocated
            continue;
          de = (struct dirent *) (addr + (*indTemp)*BLOCK_SIZE);
          if (de != 0) {
            // iterate through dirents of the inode
            for (k = 0; k < direntCount; k++,de++) {
              if (de == 0 || de->inum == 0)
                continue;
              if (strcmp(de->name,".") == 0) {
                currEntry = true;
                // dirent . should have rInodeP's inode number (i-1)
                if (de->inum == i - 1)
                  currPToItself = true;
              }
              if (strcmp(de->name,"..") == 0)
                parentEntry = true;
              if (currEntry && parentEntry && currPToItself)
                break;
            }
          }
          if (currEntry && parentEntry && currPToItself)
            break;
        }
        if (!currEntry || !parentEntry || !currPToItself) {
          /*
          Rule 4:
            Each directory contains . and .. entries, and the . entry points to 
            the directory itself. If not, print ERROR: directory not properly formatted.
          */
          fprintf(stderr, "ERROR: directory not properly formatted.\n");
          exit(1);
        }
      }
    }
  }
}

/*
Rule 5:
  For in-use inodes, each block address in use is also marked in use in the 
  bitmap. If not, print ERROR: address used by inode but marked free in bitmap.
*/
void validateRule5(char *addr, struct superblock *sb) {
  int i, j;
  uint inodeBlockCount = (sb->ninodes/IPB) + 1;
  uint lastBitmapBlock = BBLOCK(IBLOCK(sb->ninodes - 1), sb->ninodes);
  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  // root inode's bitmap block address
  char* bitmapBlock = (char*) ((char *)rInodeP + inodeBlockCount * BLOCK_SIZE);
  char bitMask[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

  // iterate through all inodes
  for (i = 1; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->addrs == 0) // unallocated
      continue;
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1 || rInodeP->type == 2 || rInodeP->type == 3) { // in use inode
      // check direct blocks
      for (j = 0; j < NDIRECT; j++) {
        if (rInodeP->addrs[j] == 0) // unallocated
          continue;
        // within valid range
        if (rInodeP->addrs[j] <= lastBitmapBlock || rInodeP->addrs[j] >= sb->nblocks)
          continue;
        // bitwise addition that returns bitmapBlock status of rInodeP->addrs[j] block
        uint isPresentBitmap = *(bitmapBlock+ (rInodeP->addrs[j])/8) & bitMask[(rInodeP->addrs[j])%8];
        // If bitmap is set as free, throw error
        if (!isPresentBitmap) {
          /*
          Rule 5:
            For in-use inodes, each block address in use is also marked in use in the 
            bitmap. If not, print ERROR: address used by inode but marked free in bitmap.
          */
          fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
          exit(1);
        }
      }
      // check indirect blocks
      uint indAddr = rInodeP->addrs[NDIRECT];
      if (indAddr == 0) // unallocated
        continue;
      else {
        uint *indTemp = (uint *) (addr + indAddr * BLOCK_SIZE);
        for (j = 0; j < NINDIRECT; j++, indTemp++) {
          if ((*indTemp) == 0) // not allocated
            continue;
          // bitwise addition that returns bitmapBlock status of (*indTemp) block
          uint isPresentBitmap = *(bitmapBlock+ (*indTemp)/8) & bitMask[(*indTemp)%8];
          // If bitmap is set as free, throw error
          if (!isPresentBitmap) {
            /*
            Rule 5:
              For in-use inodes, each block address in use is also marked in use in the 
              bitmap. If not, print ERROR: address used by inode but marked free in bitmap.
            */
            fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
            exit(1);
          }
        }
      }
    }
  }
}

/*
Rule 6:
  For blocks marked in-use in bitmap, the block should actually be in-use in an 
  inode or indirect block somewhere. If not, print ERROR: bitmap marks block 
  in use but it is not in use.
*/
void validateRule6(char *addr, struct superblock *sb) {
  int i, j;
  uint inodeBlockCount = (sb->ninodes/IPB) + 1;
  uint firstDataBlock = BBLOCK(IBLOCK(sb->ninodes - 1), sb->ninodes) + 1;
  char bitMask[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  // root inode's bitmap block address
  char* bitmapBlock = (char*) ((char *)rInodeP + inodeBlockCount * BLOCK_SIZE);
  
  uint isBlockUsed[sb->nblocks]; // keep track of used datablocks
  memset(isBlockUsed, 0, sb->nblocks * sizeof(uint)); // set all values to zero

  // First iterate through all inodes, mark used datablocks in isBlockUsed
  for (i = 0; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1 || rInodeP->type == 2 || rInodeP->type == 3) { // in use inode
      // check direct blocks
      for (j = 0; j < NDIRECT; j++) {
        if (rInodeP->addrs[j] == 0) // unallocated
          continue;
        // mark datablock as used
        isBlockUsed[rInodeP->addrs[j] - firstDataBlock] = 1;
      }
      // check indirect blocks
      uint indAddr = rInodeP->addrs[NDIRECT];
      if (indAddr == 0) // unallocated
        continue;
      // mark datablock as used
      isBlockUsed[indAddr - firstDataBlock] = 1;
      uint *indTemp = (uint *) (addr + indAddr * BLOCK_SIZE);
      for (j = 0; j < NINDIRECT; j++, indTemp++) {
        if ((*indTemp) == 0) // not allocated
          continue;
        // mark datablock as used
        isBlockUsed[(*indTemp) - firstDataBlock] = 1;
      }
    }
  }
  // Now iterate through datablocks to find any possible discrepancies with bitmapBlock data
  for (i = 0; i < sb->nblocks; i++) {
    uint isPresentBitmap = *(bitmapBlock+ (firstDataBlock + i)/8) & bitMask[(firstDataBlock + i)%8];
    // if datablock is not used, but marked in bitmap as used
    if (isBlockUsed[i] == 0 && isPresentBitmap) {
      /*
      Rule 6:
        For blocks marked in-use in bitmap, the block should actually be in-use in an 
        inode or indirect block somewhere. If not, print ERROR: bitmap marks block 
        in use but it is not in use.
      */
      fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
      exit(1);
    }
  }
}

/*
Rule 7:
  For in-use inodes, each direct address in use is only used once. If not, 
  print ERROR: direct address used more than once.

Rule 8:
  For in-use inodes, each indirect address in use is only used once. If not, 
  print ERROR: indirect address used more than once.
*/
void validateRule7_8(char *addr, struct superblock *sb) {
  int i, j;
  uint firstDataBlock = BBLOCK(IBLOCK(sb->ninodes - 1), sb->ninodes) + 1;
  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);

  uint isBlockUsed[sb->nblocks]; // keep track of used datablocks
  memset(isBlockUsed, 0, sb->nblocks * sizeof(uint)); // set all values to zero
  
  // Iterate through all inodes, mark used datablocks in isBlockUsed[]
  for (i = 0; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1 || rInodeP->type == 2 || rInodeP->type == 3) { // in use inode
      // check direct blocks
      for (j = 0; j < NDIRECT; j++) {
        if (rInodeP->addrs[j] == 0) // unallocated
          continue;
        // mark data block as used
        if (isBlockUsed[rInodeP->addrs[j] - firstDataBlock] == 1) { // already used
          /*
          Rule 7:
            For in-use inodes, each direct address in use is only used once. If not, 
            print ERROR: direct address used more than once.
          */
          fprintf(stderr, "ERROR: direct address used more than once.\n");
          exit(1);
        } else
          isBlockUsed[rInodeP->addrs[j] - firstDataBlock] = 1;
      }
      // check indirect blocks
      uint indAddr = rInodeP->addrs[NDIRECT];
      if (indAddr == 0) // unallocated
        continue;
      // mark data block as used
      if (isBlockUsed[indAddr - firstDataBlock] == 1) { // already used
        /*
        Rule 8:
          For in-use inodes, each indirect address in use is only used once. If not, 
          print ERROR: indirect address used more than once.
        */
        fprintf(stderr, "ERROR: indirect address used more than once.\n");
        exit(1);
      } else
        isBlockUsed[indAddr - firstDataBlock] = 1;
      uint *indTemp = (uint *) (addr + indAddr * BLOCK_SIZE);
      for (j = 0; j < NINDIRECT; j++, indTemp++) {
        if ((*indTemp) == 0) // not allocated
          continue;
        // mark data block as used
        if (isBlockUsed[(*indTemp) - firstDataBlock] == 1) { // already used
          /*
          Rule 8:
            For in-use inodes, each indirect address in use is only used once. If not, 
            print ERROR: indirect address used more than once.
          */
          fprintf(stderr, "ERROR: indirect address used more than once.\n");
          exit(1);
        } else
          isBlockUsed[(*indTemp) - firstDataBlock] = 1;
      }
    }
  }
}

/*
Rule 9:
  For all inodes marked in use, each must be referred to in at least one 
  directory. If not, print ERROR: inode marked use but not found in a directory.
*/
void validateRule9(char *addr, struct superblock *sb) {
  int i, j, k, direntCount;
  struct dirent *de;
  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  direntCount = rInodeP[ROOTINO].size/sizeof(struct dirent);

  uint isInodeInDir[sb->ninodes]; // tracks every inode found in directories
  memset(isInodeInDir, 0, sb->ninodes * sizeof(uint)); // set all values to zero
  
  // Iterate through all directory inodes
  for (i = 0; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->addrs == 0) // unallocated
      continue;
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1) { // directory
      if (rInodeP->addrs != 0) {
        // check direct blocks
        for (j = 0; j < NDIRECT; j++) {
          if (rInodeP->addrs[j] == 0) // unallocated
            continue;
          de = (struct dirent *) (addr + (rInodeP->addrs[j])*BLOCK_SIZE);
          if (de != 0) {
            // iterate through dirents of the inode
            for (k = 0; k < direntCount; k++,de++) {
              if (de == 0 || de->inum == 0)
                continue;
              isInodeInDir[de->inum]++; // increment reference count for this inode
            }
          }
        }
        // check indirect blocks
        uint indAddr = rInodeP->addrs[NDIRECT];
        if (indAddr == 0) // unallocated
          continue;
        uint *indTemp = (uint *) (addr + indAddr * BLOCK_SIZE);
        for (j = 0; j < NINDIRECT; j++, indTemp++) {
          if ((*indTemp) == 0) // not allocated
            continue;
          de = (struct dirent *) (addr + (*indTemp)*BLOCK_SIZE);
          if (de != 0) {
            // iterate through dirents of the inode
            for (k = 0; k < direntCount; k++,de++) {
              if (de == 0 || de->inum == 0)
                continue;
              isInodeInDir[de->inum]++; // increment reference count for this inode
            }
          }
        }
      }
    }
  }
  // Iterate through all inodes, check isInodeInDir[i] to see if it is referenced
  rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  for (i = 0; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1 || rInodeP->type == 2 || rInodeP->type == 3) { // in use inode
      if (isInodeInDir[i] == 0) { // used inode, but not found in a directory
        /*
        Rule 9:
          For all inodes marked in use, each must be referred to in at least one 
          directory. If not, print ERROR: inode marked use but not found in a directory.
        */
        fprintf(stderr, "ERROR: inode marked use but not found in a directory.\n");
        exit(1);
      }
    }
  }
}

/*
Rule 10:
  For each inode number that is referred to in a valid directory, it is 
  actually marked in use. If not, print ERROR: inode referred to in 
  directory but marked free.
*/
void validateRule10(char *addr, struct superblock *sb) {
  int i, j, k, direntCount;
  struct dirent *de;
  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  direntCount = rInodeP[ROOTINO].size/sizeof(struct dirent);
  
  uint isInodeInUse[sb->ninodes]; // tracks every inode in use
  memset(isInodeInUse, 0, sb->ninodes * sizeof(uint)); // set all values to zero
  
  // Iterate through all inodes, set isInodeInUse[i] for every inode in use
  for (i = 0; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1 || rInodeP->type == 2 || rInodeP->type == 3) // in use inode
      isInodeInUse[i]++; // increment isInodeInUse for this inode
  }
  // Iterate through all directory inodes
  rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  for (i = 0; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->addrs == 0) // unallocated
      continue;
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1) { // directory
      if (rInodeP->addrs != 0) {
        // check direct blocks
        for (j = 0; j < NDIRECT; j++) {
          if (rInodeP->addrs[j] == 0) // unallocated
            continue;
          de = (struct dirent *) (addr + (rInodeP->addrs[j])*BLOCK_SIZE);
          if (de != 0) {
            // iterate through dirents of the inode
            for (k = 0; k < direntCount; k++,de++) {
              if (de == 0 || de->inum == 0)
                continue;
              if (isInodeInUse[de->inum] == 0) { // if inode is not used but found in directory
                /*
                Rule 10:
                  For each inode number that is referred to in a valid directory, it is 
                  actually marked in use. If not, print ERROR: inode referred to in 
                  directory but marked free.
                */
                fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
                exit(1);
              }
            }
          }
        }
        // check indirect blocks
        uint indAddr = rInodeP->addrs[NDIRECT];
        if (indAddr == 0) // unallocated
          continue;
        uint *indTemp = (uint *) (addr + indAddr * BLOCK_SIZE);
        for (j = 0; j < NINDIRECT; j++, indTemp++) {
          if ((*indTemp) == 0) // not allocated
            continue;
          de = (struct dirent *) (addr + (*indTemp)*BLOCK_SIZE);
          if (de != 0) {
            // iterate through dirents of the inode
            for (k = 0; k < direntCount; k++,de++) {
              if (de == 0 || de->inum == 0)
                continue;
              if (isInodeInUse[de->inum] == 0) { // if inode is not used but found in directory
                /*
                Rule 10:
                  For each inode number that is referred to in a valid directory, it is 
                  actually marked in use. If not, print ERROR: inode referred to in 
                  directory but marked free.
                */
                fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
                exit(1);
              }
            }
          }
        }
      }
    }
  }
}

/*
Rule 11:
  Reference counts (number of links) for regular files match the number of 
  times file is referred to in directories (i.e., hard links work correctly). 
  If not, print ERROR: bad reference count for file.

Rule 12:
  No extra links allowed for directories (each directory only appears in one 
  other directory). If not, print ERROR: directory appears more than once 
  in file system.
*/
void validateRule11_12(char *addr, struct superblock *sb) {
  int i, j, k, direntCount;
  struct dirent *de;
  struct dinode *rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  direntCount = rInodeP[ROOTINO].size/sizeof(struct dirent);

  uint inodeRefCount[sb->ninodes]; // keeps track of inode reference count 
  memset(inodeRefCount, 0, sb->ninodes * sizeof(uint)); // set all values to zero

  // Iterate through all directory inodes
  for (i = 0; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->addrs == 0) // unallocated
      continue;
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 1) { // directory
      if (rInodeP->addrs != 0) {
        // check direct blocks
        for (j = 0; j < NDIRECT; j++) {
          if (rInodeP->addrs[j] == 0) // unallocated
            continue;
          de = (struct dirent *) (addr + (rInodeP->addrs[j])*BLOCK_SIZE);
          if (de != 0) {
            // iterate through dirents of the inode
            for (k = 0; k < direntCount; k++,de++) {
              if (de == 0 || de->inum == 0 || 
                  strcmp(de->name,".") == 0 || strcmp(de->name,"..") == 0)
                continue;
              inodeRefCount[de->inum]++; // keep track of reference count
            }
          }
        }
        // check indirect blocks
        uint indAddr = rInodeP->addrs[NDIRECT];
        if (indAddr == 0) // unallocated
          continue;
        uint *indTemp = (uint *) (addr + indAddr * BLOCK_SIZE);
        for (j = 0; j < NINDIRECT; j++, indTemp++) {
          if ((*indTemp) == 0) // not allocated
            continue;
          de = (struct dirent *) (addr + (*indTemp)*BLOCK_SIZE);
          if (de != 0) {
            // iterate through dirents of the inode
            for (k = 0; k < direntCount; k++,de++) {
              if (de == 0 || de->inum == 0 ||
                  strcmp(de->name,".") == 0 || strcmp(de->name,"..") == 0)
                continue;
              inodeRefCount[de->inum]++; // keep track of reference count
            }
          }
        }
      }
    }
  }
  // Iterate through all inodes
  rInodeP = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);
  for (i = 0; i < sb->ninodes; i++, rInodeP++) {
    if (rInodeP->type == 0) // not in use
      continue;
    if (rInodeP->type == 2) { // in use file inode
      if (inodeRefCount[i] != rInodeP->nlink) {
        /*
        Rule 11:
          Reference counts (number of links) for regular files match the number of 
          times file is referred to in directories (i.e., hard links work correctly). 
          If not, print ERROR: bad reference count for file.
        */
        fprintf(stderr, "ERROR: bad reference count for file.\n");
        exit(1);
      }
    }
    if (rInodeP->type == 1) { // in use directory inode
      if (inodeRefCount[i] > 1) { // if directory is referenced more than once
        /*
        Rule 12:
          No extra links allowed for directories (each directory only appears in one 
          other directory). If not, print ERROR: directory appears more than once 
          in file system.
        */
        fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
        exit(1);
      }
    }
  }
}