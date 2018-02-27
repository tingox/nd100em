/*
 * nd100em - ND100 Virtual Machine
 *
 * Copyright (c) 2006-2008 Roger Abrahamsson
 *
 * This file is originated from the nd100em project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the nd100em
 * distribution in the file COPYING); if not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "nd100.h"
#include "mon.h"

void mon_0(){
	MON_RUN=0;
	gPC++;  // IF ERROR RETURN DONT COUNT UP
}

void mon_1(){
	char slask;
	slask = getchar();
//	slask = mygetc();
	printf("%c",slask);
	switch (slask)  {
		case '#' : gA=0x008D;  break;
		case ' ' : gA=0x0080 | ' ';  break;
		case ';' : gA=0x0080 | ';';  break;
		case ',' : gA=0x0080 | ',';  break;
		case '=' : gA=0x0080 | '=';  break;
		case '/' : gA=0x0080 | '/';  break;
		case '*' : gA=0x0080 | '*';  break;
		case ')' : gA=0x0080 | ')';  break;
		case '1' : gA=0x0080 | '1';  break;
		case '2' : gA=0x0080 | '2';  break;
		case '4' : gA=0x0080 | '4';  break;
		case '7' : gA=0x0080 | '7';  break;
		case '8' : gA=0x0080 | '8';  break;
		case 10  : gA=0x008D;  break;
		case 'C'    : gA=0x0080 | 'C';  break;
		case 'E'    : gA=0x0080 | 'E';  break;
		case 'F'    : gA=0x0080 | 'F';  break;
		case 'I'    : gA=0x0080 | 'I';  break;
		case 'J'    : gA=0x0080 | 'J';  break;
		case 'L'    : gA=0x0080 | 'L';  break;
		case 'O'    : gA=0x0080 | 'O';  break;
		case 'Q'    : gA=0x0080 | 'Q';  break;
		case 'R'    : gA=0x0080 | 'R';  break;
		case 'T'    : gA=0x0080 | 'T';  break;
		case 'W'    : gA=0x0080 | 'W';  break;
		case 'X'    : gA=0x0080 | 'X';  break;
		default    : gA=slask;
	}
//	if (trace) fprintf(tracefile,"MONINPT: %c\n",(gA&0x007F));
	if  (trace & 0x01) fprintf(tracefile,
		"#o (i,d) #v# (\"%d\",\"MONINPT: %c\");\n",
		(int)instr_counter,(gA&0x007F));
	gReg->reg[CurrLEVEL][_P]++;  // IF ERROR RETURN DONT COUNT UP
}

void mon_2(){ /* MON 2 OUTBT  T=Filenumber; A=Byte;  Ret A=errorcode */
	char ch=gA & 0x007F;
	switch (ch) {
		case 12: break;
		default: printf("%c",ch);break;
	}
	if (ch !=10 && ch != 13 && ch != 12) {
//		if (trace) fprintf(tracefile,"MONOUTBT: C=%c (decimal=%d) T=%06o A=%06o\n",ch,ch,gT,gA);
		if  (trace & 0x01) fprintf(tracefile,
			"#o (i,d) #v# (\"%d\",\"MONOUTBT: C=%c (decimal=%d) T=%06o A=%06o\");\n",
			(int)instr_counter,ch,ch,gT,gA);
	} else {
//		if (trace) fprintf(tracefile,"MONOUTBT: (decimal=%d) T=%06o A=%06o\n",ch,gT,gA);
		if  (trace & 0x01) fprintf(tracefile,
			"#o (i,d) #v# (\"%d\",\"MONOUTBT: (decimal=%d) T=%06o A=%06o\");\n",
			(int)instr_counter,ch,gT,gA);
	}
	gReg->reg[CurrLEVEL][_P]++;  // IF ERROR RETURN DONT COUNT UP
}

void mon_3(){
}

void mon_4(){
}

void mon_5(){ /* MON 5 RDISK  T=blocknumber X=coreadress; Ret A=errorcode */
}

void mon_47(){ /* SBRK - Special MONcall for MAC Assembler for setting breakpoints */
}

/*
 * TIME
 * Returns current internal time.
 * AD = time in basic time units (normally 20 milliseconds).
 */
void mon_11(){
}

/*
 * ERMSG
 * IN: A = Error number
 */
void mon_64(){ /* ERMSG - Types an explanatory error message */
// Not really implemented yet
//	fprintf("%s : %o\n","ERMSG",gA);
	if  (trace & 0x01) fprintf(tracefile,
		"#o (i,d) #v# (\"%d\",\"ERMSG - %o (ermsg not quite done yet)\");\n",
		(int)instr_counter,gA);
}

void mon_117(){
}

void mon_notyet() {
//	if (trace) fprintf(tracefile,"MONITOR CALL NOT IMPLEMENTED YET!!!\n");
	if  (trace & 0x01) fprintf(tracefile,
		"#o (i,d) #v# (\"%d\",\"MONITOR CALL NOT IMPLEMENTED YET!!!\");\n",
		(int)instr_counter);

	// ERROR RETURN DONT COUNT UP
}


void mon (unsigned char monnum) {
	monarr[monnum]();	/* call using a function pointer from the array
				this way we are as flexible as possible as we
				implement mon calls. */
}

/*
// MON 0
// MON 1 INBT  T=filenumber;  A=byter;  Ret A=errorcode
// MON 2 OUTBT  T=Filenumber; A=Byte;  Ret A=errorcode
// MON 5 RDISK  T=blocknumber X=coreadress; Ret A=errorcode
// MON 6 WDISK  T=blocknumber X=coreadress; Ret A=errorcode
// MON 7 RPAGE  T=filnumber X=coreadress A=blocknumber; Ret A=errorc
// MON 10 WPAGE  T=filenumber X=coreadress A=Blocknumebr; Ret A=errorcode
// MON 16 MON 16 GET TERM TYP
// MON 27 RTDSC???
// MON 32 WRITE STRING terminated by...' X=Adress in memory
// MON 33 ALTON, PED using this
// MON 34 ALTOFF, PED using this
// MON 41 ROBJE  T=filenumber A=adress memory; Ret A=errorcode
// MON 42 OPEN
// MON 43 CLOSE  T=filenumber ; Ret A=errorcode
// MON 76 SETBL : SET BLOCK SIZE
*/
