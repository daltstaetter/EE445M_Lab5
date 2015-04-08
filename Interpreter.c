// Interpreter.c
// Runs on LM4F120/TM4C123
// Tests the UART0 to implement bidirectional data transfer to and from a
// computer running HyperTerminal.  This time, interrupts and FIFOs
// are used.
// Daniel Valvano
// September 12, 2013
// Modified by Kenneth Lee, Dalton Altstaetter 2/9/2015

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2014
   Program 5.11 Section 5.6, Program 3.10

 Copyright 2014 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// U0Rx (VCP receive) connected to PA0
// U0Tx (VCP transmit) connected to PA1
#include <stdio.h>
#include <stdint.h>
#include "PLL.h"
#include "UART.h"
#include "ST7735.h"
#include "ADC.h"
#include <rt_misc.h>
#include <string.h>
#include "OS.h"
#include "ifdef.h"
#include "efile.h"
//#define INTERPRETER

void Interpreter(void);




//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}
//debug code
int main1(void){
  char i;
  char string[20];  // global to assist in debugging
  uint32_t n;

  PLL_Init();               // set system clock to 50 MHz
  UART_Init();              // initialize UART
  OutCRLF();
  for(i='A'; i<='Z'; i=i+1){// print the uppercase alphabet
    UART_OutChar(i);
  }
  OutCRLF();
  UART_OutChar(' ');
  for(i='a'; i<='z'; i=i+1){// print the lowercase alphabet
    UART_OutChar(i);
  }
  OutCRLF();
  UART_OutChar('-');
  UART_OutChar('-');
  UART_OutChar('>');
  while(1){
    UART_OutString("InString: ");
    UART_InString(string,19);
    UART_OutString(" OutString="); UART_OutString(string); OutCRLF();

    UART_OutString("InUDec: ");  n=UART_InUDec();
    UART_OutString(" OutUDec="); UART_OutUDec(n); OutCRLF();

    UART_OutString("InUHex: ");  n=UART_InUHex();
    UART_OutString(" OutUHex="); UART_OutUHex(n); OutCRLF();

  }
}

int main2(void){
	PLL_Init();
	Output_Init();
	ST7735_Message(0,0,"Hello World",45);
	ST7735_Message(1,8,"Hello World",45);
	ST7735_Message(0,0,"A",12345);
	while(1){;}
}
#define PE4  (*((volatile unsigned long *)0x40024040))
	// 1) format 
// 2) directory 
// 3) print file
// 4) delete file
// execute   eFile_Init();  after periodic interrupts have started
#ifdef INTERPRETER
void Interpreter(void){
	char input_str[30];
	char ch;
	int input_num,i,device,line;
	int freq, numSamples;
	UART_Init();              // initialize UART
	OutCRLF();
	OutCRLF();
	
	//Print Interpreter Menu
	printf("Debugging Interpreter Lab 1\n\r");
	printf("Commands:\n\r");
	printf("LCD\n\r");
	printf("OS-K - Kill the Interpreter\n\r");
	#ifdef PROFILER
	printf("PROFILE - get profiling info for past events\n\r");
	#endif
	printf("FORMAT - format the file system\n\r");
	printf("LS - prints directory\n\r");
	printf("CAT - prints file contents");
	printf("RM - delete\n\r");
	printf("TOUCH - create a file");
	printf("INIT - initialize file system");
	printf("WRT - write to a file");
	
	while(1){
		//PE4^=0x10;
		printf("\n\rEnter a command:\n\r");
		for(i=0;input_str[i]!=0;i++){input_str[i]=0;}		//Flush the input_str
		UART_InString(input_str,30);
		if(!strcmp(input_str,"LCD")){
			printf("\n\rMessage to Print: ");
			for(i=0;input_str[i]!=0;i++){input_str[i]=0;}		//Flush the input_str
			UART_InString(input_str,30);
			printf("\n\rNumber to Print: ");
			input_num=UART_InUDec();
			printf("\n\rDevice to Print to: ");
			device = UART_InUDec();
			printf("\n\rLine to Print to: ");
			line = UART_InUDec();
			ST7735_Message(device,line,input_str,input_num);
		} else if(!strcmp(input_str,"OS-K")){
			OS_Kill();
			#ifdef PROFILER
		} else if(!strcmp(input_str,"PROFILE")){
			printf("\n\rThreadAddress\tThreadAction\tThreadTime\n\r");
			for(i=0; i<PROFSIZE; i++){
				printf("%lu\t\t%lu\t\t%lu\n\r",(unsigned long)ThreadArray[i],ThreadAction[i],ThreadTime[i]/80000);
			}
			#endif 
		} else if(!strcmp(input_str,"FORMAT")){
				eFile_Format();
		} else if(!strcmp(input_str,"LS")){
			  eFile_Directory(&UART_OutChar);
		} else if(!strcmp(input_str,"CAT")){
				printf("\n\rFile to View: ");
				for(i=0;input_str[i]!=0;i++){input_str[i]=0;}		//Flush the input_str
				UART_InString(input_str,30);
				if(eFile_ROpen(input_str))
				{					
					printf("\n\rError or File does not exist");
					continue;
				}
				while(!eFile_ReadNext(&ch)){
					printf("%c",ch);
				}
				eFile_RClose();
				
		} else if(!strcmp(input_str,"RM")){
			printf("\n\rFile to Delete: ");
			for(i=0;input_str[i]!=0;i++){input_str[i]=0;}		//Flush the input_str
			UART_InString(input_str,30);
			if(eFile_Delete(input_str)){
				printf("\n\rError or File does not exist");
			}
		} else if(!strcmp(input_str,"TOUCH")){
			printf("\n\rFile to Create: ");
			for(i=0;input_str[i]!=0;i++){input_str[i]=0;}		//Flush the input_str
			UART_InString(input_str,30);
			if(eFile_Create(input_str)){
				printf("\n\rError or No room left");
			}
		} else if(!strcmp(input_str, "INIT")){
			eFile_Init();
		} else if(!strcmp(input_str, "WRT")){
			printf("\n\rFile to Write: ");
			for(i=0;input_str[i]!=0;i++){input_str[i]=0;}		//Flush the input_str
			UART_InString(input_str,30);
			eFile_RedirectToFile(input_str);
			for(i=0;input_str[i]!=0;i++){input_str[i]=0;}		//Flush the input_str
			UART_InString(input_str,30);
			printf("%s", input_str);
			eFile_EndRedirectToFile();
		}
		else{
			printf("\n\rInvalid Command. Try Again\n\r");
		}
		//OS_Sleep(1000);
	}
}
#endif

