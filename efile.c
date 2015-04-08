// filename ************** eFile.h *****************************
// Middle-level routines to implement a solid-state disk 
// Jonathan W. Valvano 3/16/11
#include "efile.h"
#include "edisk.h"
#include <string.h>

//Globals used to format the file system
DIRECTORY dir[DIRSIZE];
uint16_t FAT[256];
unsigned char FormatBuffer[BLOCKSIZE];

//Globals used by File Writing operations (Create, WOpen, Write)
unsigned char LastWBlock[BLOCKSIZE];
uint16_t LastWBlockNum;
char* FileWName;
unsigned char tempDir[BLOCKSIZE];  
int endOfFileIndex;
uint8_t buf[BLOCKSIZE]; 

//Globals used by File Reading operations (ROpen, Read)
unsigned char LastRBlock[BLOCKSIZE];
uint16_t LastRBlockNum;
char* FileRName; 
unsigned char tempDirRead[BLOCKSIZE];
unsigned char FATReadBuf[BLOCKSIZE];
int ReadingPos=0;


//Globals for redirect flag
int RedirectFlag = 0;
	/*
    Block 1  	   Index(Byte#)		 Value			Actual Block # in File Data
											0							x							x
											1							2							2 + 16 = 18
											2							3							3 + 16 = 19
											3							4							20
										...							...
										 255						256						
		Block 2		 				0							257
											1							258
											2							259
		...							...							...
		Block 16					0							3840
		...							...							...
										 255							0
		Given a FAT index (a.k.a. startBlock/endBlock for a file), divide by 256 to get the FAT Block that
		the index refers to plus 1. Ex: StartBlock of File A is 2000. 2000/256 = 7.8125. 
		Therefore the index refers to block 8 in the FAT. 
		
		To transform the StartBlock into an index into 
		a 512 byte array, modulo divide block number by 256. Ex:
		2000%256 = 208. 
		
		Step 1: BlockNumber/256 + 1 = Block of FAT that contains the corresponding element
		Step 2: BlockNumber%256 = byte number within that block that contains the corresponding element
		Step 3: BlockNumber + SizeOfFAT = Corresponding Block in File
		*/

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating
int eFile_Init(void){
	int status = 0;
	status = eDisk_Init(0);// initialize file system
	return status;
} 

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){
	
	int i,j,k;
	int status = 0;
	
	/**********Format the Directory**********/
	strcpy(dir[FREE].name,"FREE");   //First directory entry is "FREE"
	dir[FREE].startFAT = 1; // FAT index corresponding to the start of the file space
	dir[FREE].endFAT = 4000;  // FAT index corresponding to the end of the file space
	 
	//Fill in a blank directory
	for(i = FREE+1; i < DIRSIZE; i++)
	{
		strcpy(dir[i].name,"");
		dir[i].startFAT = 0;
		dir[i].endFAT = 0;
	}
	// Convert array of structures (directory) into an array of 512 bytes
	char* ptr;
	for(i=0; i<DIRSIZE; i++){
			ptr = &FormatBuffer[i*DIRENTRYSIZE];
			strcpy(ptr,dir[i].name);
			ptr = ptr + 8;
			*ptr++ = dir[i].startFAT>>8;
			*ptr++ = dir[i].startFAT & 0xFF;
			*ptr++ = dir[i].endFAT>>8;
			*ptr = dir[i].endFAT & 0xFF;
	}
	// Write the directory to Disk
	status |= eDisk_WriteBlock(FormatBuffer,DIRECTBLOCK);
	
	/*******************Format the FAT***********************/
	//1st Block
	for(i = FATSTART; i < 256; i++)
	{
		FAT[i] = i+1; 		
	}
	
	for(j=0; j<BLOCKSIZE; j++){
		FormatBuffer[j] = FAT[j/2]>>8;
		FormatBuffer[++j] = FAT[j/2] & 0xFF;
	}
	status |= eDisk_WriteBlock(FormatBuffer,1);
	
	//2-15 blocks
	uint16_t temp = 256;
	for(k = 2; k < 16; k++)
	{
		for(i = 0; i < 256; i++)
		{
			FAT[i] = temp++; 		
		}
		
		for(j=0; j<BLOCKSIZE; j++){
			FormatBuffer[j] = FAT[j/2]>>8;
			FormatBuffer[++j] = FAT[j/2] & 0xFF;
		}
		status |= eDisk_WriteBlock(FormatBuffer,k);
	}
	
	//16th block
	for(i = 0; i < 159; i++)
	{
			FAT[i] = temp++; 		
	}
	FAT[160] = 0;
	for(i=161; i<256; i++){
		FAT[i]=0;
	}
	for(j=0; j<BLOCKSIZE; j++){
		FormatBuffer[j] = FAT[j/2]>>8;
		FormatBuffer[++j] = FAT[j/2] & 0xFF;
	}
	status |= eDisk_WriteBlock(FormatBuffer,16);
	
	return status;
} 

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( char name[]){  // create new file, make it empty 
	
	int startBlock,i;
	int FATBlock, FATIndex, nextBlock;
	int status = 0;

	status |= eDisk_ReadBlock(tempDir,DIRECTBLOCK);
	for(i = 0; i < BLOCKSIZE; i += DIRENTRYSIZE)
	{
		if(!strcmp(&tempDir[i], ""))
		{
			strcpy(&tempDir[i], name);
			break;
		}
	}
	if(i==BLOCKSIZE) status = 1; //no more room in directory;
	
	//Free space starts with startBlock
	startBlock = (tempDir[8]<<8) + tempDir[9]&0xFF;
	
	// startblock of file in directory
	tempDir[i+NAMESIZE] = startBlock >> 8; // store high byte
	tempDir[i+NAMESIZE+1] = startBlock & 0x00FF; // store low byte 
	
	// endblock of file in directory
	tempDir[i+NAMESIZE+2] = startBlock >> 8; // store high byte
	tempDir[i+NAMESIZE+3] = startBlock & 0x00FF; // store low byte 
	
	// need to change startBlock of Free to be FAT[startBlock] to point to next available block
	FATIndex = startBlock%256*2;				//Index within FAT Block that contains the second block of the Free List. 
	FATBlock = startBlock/256+1;			//Block # for corresponding FAT block
	status |= eDisk_ReadBlock(buf,FATBlock);	//Read the FAT block
	nextBlock = (buf[FATIndex]<<8) + buf[FATIndex+1]&0xFF;		//Get the next Block in the Free List
	buf[FATIndex] = 0;				//Set the contents of that block equal to null (the file contains only one block)
	buf[FATIndex+1] = 0;
	status |= eDisk_WriteBlock(buf,FATBlock);
	tempDir[NAMESIZE] = nextBlock >> 8; //Update directory for free list
	tempDir[NAMESIZE+1] = nextBlock & 0xFF;
	status |= eDisk_WriteBlock(tempDir,DIRECTBLOCK);
	
	//Write zeroes to the first block in the new file
	for(i=0; i<BLOCKSIZE; i++){
		buf[i]=0xFF;
	}
	status |= eDisk_WriteBlock(buf,startBlock+FATSIZE);				
	return status;
}


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: end index into the block that matches 'name' in the directory else returns -1
uint16_t eFile_WOpen(char name[]){      // open a file for writing 
	
	int i;
	uint16_t startBlock, endBlock, status = 0;
	FileWName = name;
	status |= eDisk_ReadBlock(tempDir,DIRECTBLOCK);
	
	// search directory for matching file char
	for(i = 0; i < BLOCKSIZE; i += DIRENTRYSIZE)
	{
		if(!strcmp(name,&tempDir[i]))
		{
			startBlock = i + NAMESIZE;
			break;
		}
	}
	endOfFileIndex = startBlock + 2;
	endBlock = startBlock + 2; // index of endblock
	endBlock = (tempDir[endBlock]<<8) + tempDir[endBlock+1];
	LastWBlockNum = endBlock+FATSIZE;
	status |= eDisk_ReadBlock(LastWBlock,endBlock+FATSIZE);
	
	return status; // block on the filesystem that was written to most recently for this file
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data){
	int startBlock,i,j;
	int FATBlock, FATIndex, nextBlock;
	int FATPrevBlk, FATPrevIndex;
	int endFreeBlock;
	int status = 0;
	status |= eDisk_ReadBlock(LastWBlock,LastWBlockNum);
	for(j=0; j<512; j++){
		if(LastWBlock[j]==0xFF){
			LastWBlock[j]=data;
			status |= eDisk_WriteBlock(LastWBlock,LastWBlockNum);
			break;
		}
	}  
	if(j==512){
		status |= eDisk_WriteBlock(LastWBlock,LastWBlockNum);
		startBlock = (tempDir[8]<<8) + tempDir[9]&0xFF;  //start of free space
		endFreeBlock = (tempDir[10]<<8) + tempDir[11]&0xFF;  //start of free space
		if(startBlock==endFreeBlock) return 1;
		FATPrevBlk = (LastWBlockNum-FATSIZE)/256+1;
		FATPrevIndex = (LastWBlockNum-FATSIZE)%256*2;
		// need to change startBlock of Free to be FAT[startBlock] to point to next available block
		FATIndex = startBlock%256*2;				//Index within FAT Block that contains the second block of the Free List. 
		FATBlock = startBlock/256+1;			//Block # for corresponding FAT block
		status |= eDisk_ReadBlock(buf,FATBlock);	//Read the FAT block
		nextBlock = (buf[FATIndex]<<8) + buf[FATIndex+1]&0xFF;		//Get the next Block in the Free List
		buf[FATIndex] = 0;				//Set the contents of that block equal to null (the file contains only one block)
		buf[FATIndex+1] = 0;
		buf[FATPrevIndex] = startBlock>>8;
		buf[FATPrevIndex+1] = startBlock&0xFF;
		status |= eDisk_WriteBlock(buf,FATBlock);
		tempDir[NAMESIZE] = nextBlock >> 8; //Update directory for free list
		tempDir[NAMESIZE+1] = nextBlock & 0xFF;
		// endblock in directory
		tempDir[endOfFileIndex] = startBlock >> 8; // store high byte
		tempDir[endOfFileIndex+1] = startBlock & 0x00FF; // store low byte
		status |= eDisk_WriteBlock(tempDir,DIRECTBLOCK);
		
		//Write zeroes to the first block in the new file
		buf[0] = data;
		for(i=1; i<BLOCKSIZE; i++){
			buf[i]=0xFF;
		}
		status |= eDisk_WriteBlock(buf,startBlock+FATSIZE);
		LastWBlockNum = startBlock+FATSIZE;
		
	}  
	return status;
}

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void){
	
} 


//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){
	int status = 0;
	status |= eDisk_WriteBlock(LastWBlock,LastWBlockNum);
} // close the file for writing

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char name[]){
	int i,status=0;
	uint16_t startBlock, endBlock;
	FileRName = name;
	status |= eDisk_ReadBlock(tempDirRead,DIRECTBLOCK);
	
	// search directory for matching file char
	for(i = 0; i < BLOCKSIZE; i += DIRENTRYSIZE)
	{
		if(!strcmp(name,&tempDirRead[i]))
		{
			startBlock = i + NAMESIZE;
			break;
		}
	}
	if(i>=BLOCKSIZE) return 1;
	startBlock = (tempDirRead[startBlock]<<8) + tempDirRead[startBlock+1];
	LastRBlockNum = startBlock+FATSIZE;
	status |= eDisk_ReadBlock(LastRBlock,startBlock+FATSIZE);
	
	return status; 
}      
   
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){
	int lastBlock,FATIndex,nextBlock,FATBlock,status=0;
	if(LastRBlock[ReadingPos]==0xFF){
		return 1;
	}else{
		*pt = LastRBlock[ReadingPos++];
	}  
	if(ReadingPos==512){
		lastBlock = LastRBlockNum - FATSIZE;
		FATBlock = lastBlock/256+1;
		FATIndex = (lastBlock%256)*2;
		status |= eDisk_ReadBlock(FATReadBuf,FATBlock);
		nextBlock = (FATReadBuf[FATIndex]<<8) + FATReadBuf[FATIndex+1]&0xFF;		//Get the next Block in the file
		if(nextBlock!=0){
			status |= eDisk_ReadBlock(LastRBlock,nextBlock+FATSIZE);
		}
		else return 1;
		//LastRBlockNum = nextBlock + FATSIZE;
		ReadingPos=0;
	}
	return status;
}       // get next byte 
                              
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){
	ReadingPos = 0;
} // close the file for writing

//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void(*fp)(unsigned char)){
			int i,j,status=0;
			char name[8];
			unsigned char startBlockHi,startBlockLo, endBlockHi,endBlockLo;
			status |= eDisk_ReadBlock(tempDirRead,DIRECTBLOCK);  
			for(i = 0; i < BLOCKSIZE; i += DIRENTRYSIZE)
			{
				strcpy(name,&tempDirRead[i]);
				if(!strcmp(name,""))
				{
					continue;
				}
				for(j=i; tempDir[j]!=0; j++){
					(*fp)(tempDir[j]);
				}
				(*fp)('\n');
				(*fp)('\r');
			}
}

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( char name[]){
	int i,status=0;
	 uint16_t StartOfFileBlck,EndOfFileBlck,EndOfFreeBlk;
	 uint16_t FATBlock,FATIndex;
	 //attach attach front of file to end of FREE list
	 //FAT[EndofFree]=startofFile
	//EndofFree = EndofFile
	 status |= eDisk_ReadBlock(tempDir,DIRECTBLOCK);
	 EndOfFreeBlk= (tempDir[NAMESIZE+2]<<8) + tempDir[NAMESIZE+3]&0xFF;  //start of free space
	 for(i = 0; i < BLOCKSIZE; i += DIRENTRYSIZE)
	{
		if(!strcmp(name,&tempDirRead[i]))
		{
			StartOfFileBlck = (tempDir[(i + NAMESIZE)]<<8)+tempDir[(i+NAMESIZE+1)]&0xFF;
			EndOfFileBlck = (tempDir[(i + NAMESIZE+2)]<<8)+tempDir[(i+NAMESIZE+3)]&0xFF;
			break;
		}
	}
	FATBlock = EndOfFreeBlk/256 + 1;
	FATIndex = EndOfFreeBlk%256*2;
	status |= eDisk_ReadBlock(buf,FATBlock);
	buf[FATIndex] = EndOfFileBlck>>8;
	buf[FATIndex+1]=EndOfFileBlck&0xFF;
	status |= eDisk_WriteBlock(buf,FATBlock);
	EndOfFreeBlk = EndOfFileBlck;
	tempDir[NAMESIZE+2] = EndOfFreeBlk>>8;
	tempDir[NAMESIZE+3] = EndOfFreeBlk&0xFF;
	tempDir[i]=0;
	tempDir[i+NAMESIZE] = 0;
	tempDir[i+NAMESIZE+1]=0;
	tempDir[i+NAMESIZE+2]=0;
	tempDir[i+NAMESIZE+3]=0;
	status |= eDisk_WriteBlock(tempDir,DIRECTBLOCK);
	return status;
}  // remove this file 

//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(char *name){
		RedirectFlag = 1;
		return eFile_WOpen(name);
	
}

//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void){
		RedirectFlag = 0;
		return eFile_WClose();
		
}
