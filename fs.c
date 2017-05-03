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
				printf("inode: %d\n",i +(j-1)*127);
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
			blockBitmap[j] = 1;
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
						blockBitmap[block.inode[i].indirect] =1;
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
	int block_num = inumber/127 + 1;
//	printf("inode is in block: %d\n",block_num);
	union fs_block block;
	disk_read(block_num,block.data);
	int index = inumber%127;
	int currBlock,i;
	//int bytesWeCanRead;
	int count=0;
	int bytesToRead;
	int end_point = offset+length;
	if (block.inode[index].size < end_point) end_point = block.inode[index].size;
//	int last_block = end_point/4096;
	int final_block = block.inode[index].size/4096;
	int block_on = (offset+count)/4096;
	int endRead = block.inode[index].size-(final_block)*4096;
//	printf("Last read will read %d bytes\n",endRead);
	int block_counter = 0;
//	printf("Total # of blocks to read: %d\n",last_block);
	int blockOffset = offset/4096; //# of blocks to skip
//	printf("Offset is: %d\n",offset);
//	printf("Due to offset we will skip: %d block(s)\n",blockOffset);
//	int blockReadOffsetIndex = offset%4096;
//	printf("Due to offset, index will start at: %d\n",blockReadOffsetIndex);
	int totalBlocksOffset=0;
	int indirect_block = block.inode[index].indirect;
	if(offset >= block.inode[index].size)
	{
		return 0;
	}
	if(block.inode[index].isvalid == 1)
	{
		int numBlocks = ceil((double)block.inode[index].size/(double)4096);
		int numDirect = numBlocks;
		int numIndirect = 0;
		if(numBlocks > 5)
		{
			numDirect = 5;
			numIndirect = numBlocks -5;
		}
//		printf("Number of Direct Blocks = %d\n",numDirect);
		//go through direct blocks
		for(i=0;i<numDirect;i++)
		{
			disk_read(block_num,block.data);
			currBlock = block.inode[index].direct[i];
//			printf("Going to direct data block: %d\n",currBlock);
			disk_read(currBlock,block.data);
			if(blockOffset > i)
			{
	//			printf("Skipping block %d due to offset\n",currBlock);
				totalBlocksOffset++;
			}
			else
			{
				
					block_on = (offset+count)/4096;
					if(block_on == final_block)
					{
//						printf("Found last block: %d\n",currBlock);
						bytesToRead = endRead;
					}
					else
					{
						bytesToRead = length-count;
						if(bytesToRead >= 4096)
						{
							bytesToRead = 4096;
						}
					}
//					printf("We will read %d bytes from this block\n",bytesToRead);
					memcpy(data+count,block.data,bytesToRead);
					count+= bytesToRead;
//					printf("Total bytes read: %d\n",count);

//				}
			}
			block_counter++;
		}
		//count indirect blocks
			disk_read(indirect_block,block.data);
//		printf("=================================\n");
//		printf("Going through indirect blocks, # of indirect blocks is: %d\n",numIndirect);
		for(i=0;i<numIndirect;i++)
		{
			disk_read(indirect_block,block.data);
			currBlock = block.pointers[i];
//			printf("Going to indirect block: %d\n",currBlock);
			disk_read(currBlock,block.data);
			if(blockOffset > totalBlocksOffset)
			{
//				printf("Skipping block %d due to offset\n",currBlock);
				totalBlocksOffset++;
			}
			else
			{
					block_on = (offset+count)/4096;
//					printf("Done with offset, going to read block: %d\n",currBlock);
					bytesToRead = length-count;
//					printf("length is: %d\n",length);
//					printf("count is: %d\n",count);
					if(block_on == final_block)
					{
//						printf("Found last block we will read: %d\n",currBlock);
						bytesToRead = endRead;
					}
					else
					{
						if(bytesToRead >= 4096)
						{
							bytesToRead = 4096;
						}
					}
//					printf("We will read %d bytes from this block\n",bytesToRead);
					memcpy(data+count,block.data,bytesToRead);
					count+= bytesToRead;
//					printf("Total bytes read: %d\n",count);

			}
			block_counter++;

		}
		return count;
	} //done reading inodes
	return 0;
}

int findFreeBlock(){
	int i;
	union fs_block block;
	disk_read(0, block.data);
	for(i=block.super.ninodeblocks+1; i <block.super.nblocks; i++){
		if(blockBitmap[i] ==0){
			blockBitmap[i] =1;
			return i;
		}
	}
	return -1;
}

int FreeSpaceLeft(){
	int i,c =0;;
	union fs_block block;
	disk_read(0, block.data);
	for(i=block.super.ninodeblocks+1; i<block.super.nblocks;i++){
		if(blockBitmap[i] ==0){
			c++;
		}
	}
	printf("total space left: %d\n", c*4096);
	return (c*4096);
}


int fs_write( int inumber, const char *data, int length, int offset )
{
	if(isMounted){
		int freeSpace = FreeSpaceLeft();
		inodeBitmap[inumber] = 1;
		union fs_block block;
		union fs_block writeBlock;
		union fs_block indirectBlock;
		int block_num = inumber/127 +1;
		disk_read(block_num, block.data);
		int index = inumber%127;
		if (freeSpace>0 && block.inode[index].isvalid ==0){
		 block.inode[index].size = 0;
		 block.inode[index].isvalid=1;
		}
		int size = block.inode[index].size;
		int remainToWrite=length;;
		if (freeSpace > length){
			printf("you have enough space\n");
			block.inode[index].size = size+length;
			remainToWrite = length;
		} else {
			printf("not enough space\n");
			block.inode[index].size = size+freeSpace;
			remainToWrite = freeSpace;
			length = freeSpace;
		}
		int count=0;
 		int currBlock;
		int bytesToWrite = 4096;
		int indirect_num = block.inode[index].indirect;
		disk_read(indirect_num, indirectBlock.data);		//get indirect block
		//check if inode block's last data block isn't 100% filled in and fill it in if it isnt
		int currRemainder = size % DISK_BLOCK_SIZE; 					//how much is left on the last block
		if(currRemainder !=0){											//there is a partially filled block
//			int lastUsedBlock = ceil((double)size/(double)DISK_BLOCK_SIZE);
			bytesToWrite = DISK_BLOCK_SIZE - currRemainder; 
			int lastUsedBlock = size/DISK_BLOCK_SIZE;
			if (lastUsedBlock <5){
				int lastBlock = block.inode[index].direct[lastUsedBlock];
				printf("Direct Block Index: %d, block #: %d	writing: %d\n",lastUsedBlock,lastBlock,bytesToWrite); 
				disk_read(lastBlock, writeBlock.data);
				memcpy(writeBlock.data+currRemainder,data+count+offset,bytesToWrite);		//write to data block
				disk_write(lastBlock, writeBlock.data);					//write edited block back to disk
				count += bytesToWrite;									//update count of total written bytes
				remainToWrite -= bytesToWrite;							//update bytes left to write
			} else {
				int lastBlock = indirectBlock.pointers[lastUsedBlock-5];
				printf("Indirect Block Index: %d, block #: %d	writing: %d\n",lastUsedBlock-5,lastBlock,bytesToWrite); 
				disk_read(lastBlock, writeBlock.data);
				memcpy(writeBlock.data+currRemainder,data+count+offset,bytesToWrite);		//write to data block
				disk_write(lastBlock, writeBlock.data);					//write edited block back to disk
				count += bytesToWrite;									//update count of total written bytes
				remainToWrite -= bytesToWrite;							//update bytes left to write
			}
		}
//		if(remainToWrite > size-offset) remainToWrite = size-offset;
		while(remainToWrite >0){
			printf("bytes remaining: %d\n", remainToWrite);
			bytesToWrite = 4096;
			currBlock = (count+size)/4096;
			if (currBlock<5){
				//direct
				int freeBlock = findFreeBlock();
				if(freeBlock==-1){										//if no more free blocks, then write whatever we can back to disk
					printf("free block: %d",freeBlock);
					disk_write(block_num, block.data);
					disk_write(indirect_num, indirectBlock.data);
					return count;
				}
				block.inode[index].direct[currBlock] = freeBlock; 		//set direct pointer to freeBlock
				disk_read(freeBlock,writeBlock.data);					//read out this free block so we can change data
				if(remainToWrite < 4096) bytesToWrite = remainToWrite;	//decide how much to write
				printf("direct block %d, block # %d, bytes: %d\n",currBlock,freeBlock,bytesToWrite);
				memcpy(writeBlock.data,data+count+offset,bytesToWrite);		//write to data block
				disk_write(freeBlock, writeBlock.data);					//write edited block back to disk
				count += bytesToWrite;									//update count of total written bytes
				remainToWrite -= bytesToWrite;							//update bytes left to write
			} else {
				//indirect
				currBlock -=5;											//set block to be indirect pointer index
	/*			if(block.inode[index].indirect ==0){
					int freeBlock = findFreeBlock();
					if(freeBlock==-1){										//if no more free blocks, then write whatever we can back to disk
						disk_write(block_num, block.data);
						disk_write(indirect_num, indirectBlock.data);
						return count;
					} else {
						printf("creating indirect block #: %d\n", freeBlock);
						block.inode[index].indirect = freeBlock;
						disk_read(freeBlock, indirectBlock.data);
					}
				}*/

				int freeBlock = findFreeBlock();
				if(freeBlock==-1){										//if no more free blocks, then write whatever we can back to disk
					disk_write(block_num, block.data);
					disk_write(indirect_num, indirectBlock.data);
					return count;
				}
				indirectBlock.pointers[currBlock] = freeBlock;	 		//set indirect pointer to freeBlock
				disk_read(freeBlock,writeBlock.data);					//read out this free block so we can change data
				if(remainToWrite < 4096) bytesToWrite = remainToWrite;	//decide how much to write
				printf("indirect block %d, block # %d, bytes: %d\n",currBlock,freeBlock,bytesToWrite);
				memcpy(writeBlock.data,data+count+offset,bytesToWrite);	//write to data block
				disk_write(freeBlock, writeBlock.data);					//write edited block back to disk
				count += bytesToWrite;									//update count of total written bytes
				remainToWrite -= bytesToWrite;							//update bytes left to write
			}
		}
		disk_write(block_num, block.data);
		disk_write(indirect_num, indirectBlock.data);
		return count;
	} else {
		return 0;
	}
}


