// filename ************** eFile.h *****************************
// Middle-level routines to implement a solid-state disk 
// Jonathan W. Valvano 3/16/11
#include "efile.h"
#include "edisk.h"
#include <string.h>

DIRECTORY dir[DIRSIZE];
uint16_t FAT[FATSIZE];



//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating
int eFile_Init(void){} // initialize file system

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){
	
	int i,j,k;
	int status;
	char buffer[BLOCKSIZE];
	
	status = 0;
	
	strcpy(dir[FREE].name,"FREE");
	for(i = 4; i < NAMESIZE; i++)
	{
		dir[FREE].name[i] = 0; // set end of name string to null
	}
	
	dir[FREE].startFAT = 1; // block corresponding to the start of the file space
	dir[FREE].endFAT = 47;
	
	for(i = FREE+1; i < DIRSIZE; i++)
	{
		for(j=0; j < NAMESIZE; j++)
		{
			dir[i].name[j] = 0;
		}
		dir[i].startFAT = 0;
		dir[i].endFAT = 0;
	}
	
	// Write block corresponding to directory to the 512 byte block
	for(i = 0; i < BLOCKSIZE; i += NAMESIZE+sizeof(uint16_t)+1)
	{
		for(j = 0; j < DIRSIZE; j++)
		{
			strcpy(&buffer[i],dir[j].name);
			buffer[i+NAMESIZE] = dir[j].startFAT;
			buffer[i+NAMESIZE+sizeof(uint16_t)] = dir[j].endFAT;
		}
	}
	
	// write block to disk
	status |= eDisk_WriteBlock(buffer,DIRECTBLOCK);
	
	// link all free blocks together
	for(i = FATSTART; i < FATSIZE-1; i++)
	{
		FAT[i] = FAT[i+1]; 
	}
	FAT[FATSIZE-1] = 0;
	
	// write the first 15 full blocks
	for(i = FATSTART; i < 16; i++)
	{
		// 3840 = 512*15/2
		for(j = FATSTART; j < 3840; )
		{
			for(k = 0; k < BLOCKSIZE; k += 2)
			{ // store as 512 byte array
				buffer[k] = FAT[j] >> 8; // copy top byte
				buffer[k+1] = FAT[j++] & 0x00FF; // copy low byte
			}
			status |= eDisk_WriteBlock(buffer,i);
		}
	}
	
	// write the last block that is fragmented
	for(j = 3840; j < FATSIZE; )
	{
		for(k = 0; k < BLOCKSIZE; k += 2)
		{ // store as 512 byte array
			buffer[k] = FAT[j] >> 8; // store top byte
			buffer[k+1] = FAT[j++] & 0x00FF; // store low byte
		}
		status |= eDisk_WriteBlock(buffer,FATEND);
	}
	
	return status;
} // erase disk, add format

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( char name[]){  // create new file, make it empty 
	
	int startBlock,i;
	uint8_t buf[BLOCKSIZE];
	char tempDir[BLOCKSIZE];
	
	eDisk_ReadBlock(tempDir,DIRECTBLOCK);
	
	// search Directory for the first null entry
	for(i = 0; i < BLOCKSIZE; i += DIRENTRYSIZE)
	{
		if(!strcmp(tempDir, NULL))
		{// store new file 
			strcpy(&tempDir[i], name);
			break;
		}
	}
	
	// need to read whole directory in, modify the entries then rewrite
	startBlock = eFile_WOpenFront("FREE", buf);
	
	// startblock in directory
	tempDir[i+NAMESIZE] = startBlock >> 8; // store high byte
	tempDir[i+NAMESIZE+1] = startBlock & 0x00FF; // store low byte 
	
	// endblock in directory
	tempDir[i+NAMESIZE+2] = startBlock >> 8; // store high byte
	tempDir[i+NAMESIZE+3] = startBlock & 0x00FF; // store low byte 
	
	// need to change startBlock of Free to be FAT[startBlock] to point to next available block
	eDisk_ReadBlock(
	
	// create file in directory & on disk at this block 
	
	
	
}


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: end index into the block that matches 'name' in the directory else returns -1
uint16_t eFile_WOpen(char name[], uint8_t* buf){      // open a file for writing 
	
	int i;
	uint16_t startBlock, endBlock, status;
	char tempBuf[BLOCKSIZE];
	
	
	status = eDisk_ReadBlock(tempBuf,DIRECTBLOCK);
	
	if(status == 1)
	{
		return -1;
	}
	
	// search directory for matching file char
	for(i = 0; i < BLOCKSIZE; i++)
	{
		if(!strcmp(name,&tempBuf[i]))
		{
			startBlock = strlen(&tempBuf[i]) + 1;
			break;
		}
	}
	endBlock = startBlock + 2; // index of endblock
	endBlock = tempBuf[endBlock] << 8 + tempBuf[endBlock+1];
	
	return endBlock; // block on the filesystem that was written to most recently for this file
}

//---------- eFile_WOpenFront-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: starting index into the block that matches 'name' in the directory else returns -1
uint16_t eFile_WOpenFront(char name[], uint8_t* buf){      // open a file for writing 
	
	int i;
	uint16_t startBlock, status;
	char tempBuf[BLOCKSIZE];
	
	status = eDisk_ReadBlock(tempBuf,DIRECTBLOCK);
	
	if(status == 1)
	{
		return -1;
	}
	
	// search directory for matching file char
	for(i = 0; i < BLOCKSIZE; i++)
	{
		if(!strcmp(name,&tempBuf[i]))
		{
			startBlock = strlen(&tempBuf[i]) + 1;
			break;
		}
	}
	
	startBlock = tempBuf[startBlock] << 8 + tempBuf[startBlock+1];
	
	return startBlock; // block on the filesystem that was written to most recently for this file
}


//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data){}  

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void){} 


//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){} // close the file for writing

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char name[]){}      // open a file for reading 
   
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){}       // get next byte 
                              
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){} // close the file for writing

//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void(*fp)(unsigned char)){;  }

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( char name[]){}  // remove this file 

//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(char *name){}

//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void){}
