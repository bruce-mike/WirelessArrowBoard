/******************************************************************************/
/* RETARGET.C: 'Retarget' layer for target-dependent low level functions      */
/******************************************************************************/
/* This file is part of the uVision/ARM development tools.                    */
/* Copyright (c) 2005-2006 Keil Software. All rights reserved.                */
/* This software may only be used under the terms of a valid, current,        */
/* end user licence from KEIL for a compatible version of KEIL software       */
/* development tools. Nothing else gives you the right to use this software.  */
/******************************************************************************/

#include <stdio.h>
#include <rt_misc.h>

#pragma import(__use_no_semihosting_swi)


extern int  sendchar(int ch);  /* in serial.c */


struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;




// BEGIN FUNCTION DEFS


// ****************************
// ****************************
// ****************************
int fputc(int ch, FILE *f) {
	
  return (sendchar(ch));
	
}



// ****************************
// ****************************
// ****************************
int ferror(FILE *f) {
  /* Your implementation of ferror */
	// we don't use it here.  No files are opened or created
  return EOF;
}



// ****************************
// ****************************
// ****************************
// called by printf()?  Retargeting printf is done by redirecting 
// _ttywrch()'s implementation here - could be to I2C, LCD, etc.  Here we 
// target the serial port via sendchar()/UART0

void _ttywrch(int ch) {
	// replace with your own function to redirect printf() elsewhere.
	// LCD/SPI/UART, etc.
  sendchar(ch);
	
}



// ****************************
// ****************************
// ****************************
// not used - embedded code does not exit.
// compile out?  What happens to unused functions?
// use microlib to reduce code size - does this affect code operation?
// not supposed to, but....optimizing away certain behaviour can happen
void _sys_exit(int return_code) {
	
	label:  goto label;  /* endless loop */
	
}
