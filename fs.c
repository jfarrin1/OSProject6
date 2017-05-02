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

//global flags
int isMounted = 0;
int* inodeBitmap;
int* blockBitmap;

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
	if (isMounted){
		return 0;
	} else {
	union fs_block block;
	disk_read(0,block.data);

	int size = disk_size();
	int nInodes = ceil((double)size/(double)10);
	int oldnInodes = block.super.ninodeblocks;

	//clear inodes
	int i;
	union fs_block blank;
	struct fs_inode blank_inode;
	blank_inode.isvalid = 0;
	blank_inode.size = 0;
	for(i=0;i<5;i++)
	{
		blank_inode.direct[i] = 0;
	}
	blank_inode.indirect = 0;
	for(i=0;i<128;i++)
	{
		blank.inode[i] = blank_inode;
	}
	for(i=1;i<=oldnInodes;i++)
	{
		disk_write(1,blank.data);
	}
	//change superblock
	union fs_block superman;
	superman.super.magic = FS_MAGIC;
	superman.super.nblocks = size;
	superman.super.ninodeblocks = nInodes;
	superman.super.ninodes = nInodes * 128;
	disk_write(0,superman.data);

	return 1;
	}
}

void fs_debug()
{
	union fs_block block;

	disk_read(0,block.data);
	int numInodes = block.super.ninodeblocks;

	printf("superblock:\n");
	if(block.super.magic == FS_MAGIC){
		printf("    magic number is legit\n");
	}
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
				if(numDirect ==0) printf("\n");
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
	if(!isMounted){
	int i,j,k;
	union fs_block block;
	disk_read(0,block.data);
	if(block.super.magic == FS_MAGIC){
		isMounted = 1;
		inodeBitmap = malloc(block.super.ninodes * sizeof(int));
		blockBitmap = malloc(block.super.nblocks * sizeof(int));
		for(k=0; k<block.super.nblocks;k++){
			blockBitmap[k] = 0;
		}
		blockBitmap[0] =1; //superblock
		int numInodeBlocks = block.super.ninodeblocks;
		for(j=1; j<=numInodeBlocks; j++){
			disk_read(j,block.data);
			for(i=0; i<128; i++){
				if(block.inode[i].isvalid ==1){
					inodeBitmap[i+128*(j-1)] = 1;
					int numBlocks = ceil((double)block.inode[i].size/(double)4096);
					int numDirect = numBlocks;
					int numIndirect = 0;
					if (numBlocks>5){
						numDirect= 5;
						numIndirect = numBlocks-5;
					}
					for(k=0; k<numDirect; k++){
						blockBitmap[block.inode[i].direct[k]] = 1;
					}
					if(numIndirect){
						disk_read(block.inode[i].indirect,block.data);
					}
					for(k=0; k<numIndirect; k++){
						blockBitmap[block.pointers[k]] = 1;
					}
					if(numIndirect){
						disk_read(j,block.data);
					}
				} else {
					inodeBitmap[i+128*(j-1)] = 0;
				}
			}
		}
		return 1;
	} else {
		return 0;
	}
	}
	return 0;
}

int fs_create()
{
	int i;
	if(isMounted){
		union fs_block block;
		disk_read(0,block.data);
		int numInodes = block.super.ninodes;
		struct fs_inode blank_inode;
		blank_inode.isvalid = 1;
		blank_inode.size = 0;
		for(i=0;i<5;i++){
			blank_inode.direct[i] = 0;
		}
		blank_inode.indirect = 0;
		printf("mounting");
		//find free inode
		for(i=1; i<numInodes; i++){
			printf("createng");
			if(inodeBitmap[i] == 0){
				printf("inode num: %d",i);
				int iBlock = i/127+1;
				int index = i%127;
				disk_read(iBlock, block.data);
				block.inode[index] = blank_inode;
				inodeBitmap[i] = 1;
				disk_write(iBlock,block.data);
				return 1;
			}
		}
	}
	return 0; //return 0 if not mounted or if no free inodes
}

int fs_delete( int inumber )
{
	int iBlock = inumber/127 +1;
	int i,k;
	int index = inumber%127;
	union fs_block block;
	disk_read(iBlock,block.data);
	if(block.inode[index].isvalid){
		//empty inode block
		struct fs_inode blank_inode;
		blank_inode.isvalid = 0;
		blank_inode.size = 0;
		for(i=0;i<5;i++)
		{
			blank_inode.direct[i] = 0;
		}
		blank_inode.indirect = 0;
		int currBlock;
		//release all data and change bitmap
		int numBlocks = ceil((double)block.inode[index].size/(double)4096);
		int numDirect = numBlocks;
		int numIndirect = 0;
		if (numBlocks>5){
			numDirect= 5;
			numIndirect = numBlocks-5;
		}
		for(k=0; k<numDirect; k++){
			currBlock = block.inode[index].direct[k];
			blockBitmap[currBlock] = 0;
		}
		if(numIndirect){
			disk_read(block.inode[index].indirect,block.data);
		}
		for(k=0; k<numIndirect; k++){
			currBlock = block.pointers[k];
			blockBitmap[currBlock] = 0;
		}
		if(numIndirect){
			disk_read(iBlock,block.data);
			blockBitmap[block.inode[index].indirect] = 0;
		}
		//delte inode
		block.inode[index] = blank_inode;
		disk_write(iBlock, block.data);
		inodeBitmap[inumber] = 0;
		return 1;
	} else {
		return 0;
	}
}

int fs_getsize( int inumber )
{
	int block_num = inumber/127 + 1;
	union fs_block block;
	disk_read(block_num, block.data);
	int index = inumber%127;
	if(block.inode[index].isvalid){
		int size = block.inode[index].size;
		return size;
	}
	else{
		return -1;
	}
}

int fs_read( int inumber, char *data, int length, int offset )
{
	int iBlock = inumber/127 +1;
	int index = inumber%127;
	int endBlock,numDirect=0, numIndirect=0;
	int i,count=0, readSize;
	int startBlock = offset/DISK_BLOCK_SIZE;
	int remainder = offset%DISK_BLOCK_SIZE;
	union fs_block block;
	union fs_block tempBlock;
	union fs_block indBlock;
	disk_read(iBlock,block.data);
	if(block.inode[index].isvalid){
		if(offset > block.inode[index].size) return 0;
		int endPoint = length+offset;
		if (endPoint > block.inode[index].size){
			endPoint = block.inode[index].size;
		}
		endBlock = ceil((double)endPoint/(double)DISK_BLOCK_SIZE);
		printf("start: %d	end: %d	",startBlock,endBlock);
		if(startBlock >=5){
			numIndirect = endBlock;
		} else if(endBlock > POINTERS_PER_INODE){
			numDirect = POINTERS_PER_INODE;
			numIndirect = endBlock-numDirect;
		} else {
			numDirect = endBlock;
		}
		printf("direct end: %d	indirect end: %d",numDirect, numIndirect);
		for(i=startBlock; i<=numDirect; i++){
			if(i==endBlock){
				readSize = remainder;
			} else {
				readSize = DISK_BLOCK_SIZE;
			}
			disk_read(block.inode[index].direct[i],tempBlock.data);
			memcpy(data+index,tempBlock.data, readSize);
			count += readSize;
		}
		if(numIndirect){
			disk_read(block.inode[index].indirect, indBlock.data);
		}
		for(i=0; i<=numIndirect; i++){
			if(i==endBlock){
				readSize = remainder;
			} else {
				readSize = DISK_BLOCK_SIZE;
			}
			disk_read(indBlock.pointers[i],tempBlock.data);
			memcpy(data+index,tempBlock.data, readSize);
			count += readSize;
		}
		return 1;
	} else {
		printf("not valid\n");
		return 0;
	}
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
