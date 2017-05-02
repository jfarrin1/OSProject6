
#include "fs.h"
#include "disk.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int fs_format()
{
	return 0;
}

void fs_debug()
{
	union fs_block block;

	int size = disk_size();
	disk_read(0,block.data);
	int numInodes = block.super.ninodeblocks;

	printf("superblock:\n");
	printf("    %d magic\n",block.super.magic);
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);
	int i,j,k;
	for(j=1; j<=numInodes; j++){
		disk_read(j,block.data);
		for(i=0; i<128; i++){
			if(block.inode[i].isvalid ==1){
				printf("inode: %d\n",i);
				printf("    size: %d bytes\n",block.inode[i].size);
				int numBlocks = ceil((double)block.inode[i].size/(double)4096);
				int numDirect = numBlocks;
				int numIndirect = 0;
				if (numBlocks>5){
					numDirect= 5;
					numIndirect = numBlocks-5;
				}
				printf("	direct blocks:");
				for(k=0; k<numDirect; k++){
					printf(" %d",block.inode[i].direct[k]);
					if(k==numDirect-1) printf("\n");
				}
				if(numIndirect){
					printf("	indirect block: %d\n", block.inode[i].indirect);
					printf("	indirect data blocks:");
					disk_read(block.inode[i].indirect,block.data);
				}
				for(k=0; k<numIndirect; k++){
					printf(" %d",block.pointers[k]);
					if(k==numIndirect-1) printf("\n");
				}
				if(numIndirect){
					disk_read(j,block.data);
				}
			}
		}
	}
}

int fs_mount()
{
	/*
		check isValid, continue if it is
		look at size, and do size/4096 to see how much data is in the inode
		always round up on the size to get # blocks
		only read exact amount of data from disk block #
		so if there is size 40961 size, then read 4096 from block 1 and 1 from block 2
		if # blocks>5 then there is stuff in indirect
		again read only what you need from the idrection block 
	*/

	return 0;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
