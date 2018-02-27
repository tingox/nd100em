/*
 * nd100em - ND100 Virtual Machine
 *
 * Copyright (c) 2006 Per-Olof Astrom
 * Copyright (c) 2006-2008 Roger Abrahamsson
 * Copyright (c) 2008 Zdravko
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

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include "nd100.h"
#include "cpu.h"


#define BUFSTRSIZE 24

struct termios savetty;

/* Interrupt thread synchronization */
sem_t sem_int;

/* mopc synchronization */
sem_t sem_mopc;

/* running cpu synchronization */
sem_t sem_run;

/* stopping cpu synchronization */
sem_t sem_stop;

/* Performance stuff */

double instr_counter;

float usertime,systemtime,totaltime;
struct rusage *used;

/* temporary string for misc data, basically a junk variable */
/* used by different trace stuff before sending off to tracing */
char trace_temp_str[256];

/* OpToStr
 * IN: pointer to string ,raw operand
 * OUT: Sets the string with the dissassembled operand and values
 */
void OpToStr(char *opstr, ushort operand) {
	ushort instr;
	char numstr[BUFSTRSIZE];
	char deltastr[BUFSTRSIZE];
	unsigned char nibble;
	char offset,delta;
	unsigned char relmode;
	bool isneg;

	offset = operand & 0x00ff;
	nibble = operand & 0x000f;
	relmode = (operand & 0x0700)>> 8;
	delta = (operand & 070);

	/* put offset into a string variable in octal with +/- sign for easy reading */
	((int)offset <0) ? (void)snprintf(numstr,BUFSTRSIZE,"-%o",-(int)offset) : (void)snprintf(numstr,BUFSTRSIZE,"%o",offset);

	/* ND110 delta offset for some instructions */
	(void)snprintf(deltastr,sizeof(deltastr),"%o",delta);

	instr=extract_opcode(operand);
	switch(instr) {
	case 0000000: /* STZ */
		(void)snprintf(opstr,BUFSTRSIZE,"STZ %s%s",relmode_str[relmode],numstr);
		break;
	case 0004000: /* STA */
		(void)snprintf(opstr,BUFSTRSIZE,"STA %s%s",relmode_str[relmode],numstr);
		break;
	case 0010000: /* STT */
		(void)snprintf(opstr,BUFSTRSIZE,"STT %s%s",relmode_str[relmode],numstr);
		break;
	case 0014000: /* STX */
		(void)snprintf(opstr,BUFSTRSIZE,"STX %s%s",relmode_str[relmode],numstr);
		break;
	case 0020000: /* STD */
		(void)snprintf(opstr,BUFSTRSIZE,"STD %s%s",relmode_str[relmode],numstr);
		break;
	case 0024000: /* LDD */
		(void)snprintf(opstr,BUFSTRSIZE,"LDD %s%s",relmode_str[relmode],numstr);
		break;
	case 0030000: /* STF */
		(void)snprintf(opstr,BUFSTRSIZE,"STF %s%s",relmode_str[relmode],numstr);
		break;
	case 0034000: /* LDF */
		(void)snprintf(opstr,BUFSTRSIZE,"LDF %s%s",relmode_str[relmode],numstr);
		break;
	case 0040000: /* MIN */
		(void)snprintf(opstr,BUFSTRSIZE,"MIN %s%s",relmode_str[relmode],numstr);
		break;
	case 0044000: /* LDA */
		(void)snprintf(opstr,BUFSTRSIZE,"LDA %s%s",relmode_str[relmode],numstr);
		break;
	case 0050000: /* LDT */
		(void)snprintf(opstr,BUFSTRSIZE,"LDT %s%s",relmode_str[relmode],numstr);
		break;
	case 0054000: /* LDX */
		(void)snprintf(opstr,BUFSTRSIZE,"LDX %s%s",relmode_str[relmode],numstr);
		break;
	case 0060000: /* ADD */
		(void)snprintf(opstr,BUFSTRSIZE,"ADD %s%s",relmode_str[relmode],numstr);
		break;
	case 0064000: /* SUB */
		(void)snprintf(opstr,BUFSTRSIZE,"SUB %s%s",relmode_str[relmode],numstr);
		break;
	case 0070000: /* AND */
		(void)snprintf(opstr,BUFSTRSIZE,"AND %s%s",relmode_str[relmode],numstr);
		break;
	case 0074000: /* ORA */
		(void)snprintf(opstr,BUFSTRSIZE,"ORA %s%s",relmode_str[relmode],numstr);
		break;
	case 0100000: /* FAD */
		(void)snprintf(opstr,BUFSTRSIZE,"FAD %s%s",relmode_str[relmode],numstr);
		break;
	case 0104000: /* FSB */
		(void)snprintf(opstr,BUFSTRSIZE,"FSB %s%s",relmode_str[relmode],numstr);
		break;
	case 0110000: /* FMU */
		(void)snprintf(opstr,BUFSTRSIZE,"FMU %s%s",relmode_str[relmode],numstr);
		break;
	case 0114000: /* FDV */
		(void)snprintf(opstr,BUFSTRSIZE,"FDV %s%s",relmode_str[relmode],numstr);
		break;
	case 0120000: /* MPY */
		(void)snprintf(opstr,BUFSTRSIZE,"MPY %s%s",relmode_str[relmode],numstr);
		break;
	case 0124000: /* JMP */
		(void)snprintf(opstr,BUFSTRSIZE,"JMP %s%s",relmode_str[relmode],numstr);
		break;
	case 0130000: /* JAP */
		(void)snprintf(opstr,BUFSTRSIZE,"JAP %s",numstr);
		break;
	case 0130400: /* JAN */
		(void)snprintf(opstr,BUFSTRSIZE,"JAN %s",numstr);
		break;
	case 0131000: /* JAZ */
		(void)snprintf(opstr,BUFSTRSIZE,"JAZ %s",numstr);
		break;
	case 0131400: /* JAF */
		(void)snprintf(opstr,BUFSTRSIZE,"JAF %s",numstr);
		break;
	case 0132000: /* JPC */
		(void)snprintf(opstr,BUFSTRSIZE,"JPC %s",numstr);
		break;
	case 0132400: /* JNC */
		(void)snprintf(opstr,BUFSTRSIZE,"JNC %s",numstr);
		break;
	case 0133000: /* JXZ */
		(void)snprintf(opstr,BUFSTRSIZE,"JXZ %s",numstr);
		break;
	case 0133400: /* JXN */
		(void)snprintf(opstr,BUFSTRSIZE,"JXN %s",numstr);
		break;
	case 0134000: /* JPL */
		(void)snprintf(opstr,BUFSTRSIZE,"JPL %s%s",relmode_str[relmode],numstr);
		break;
	case 0140000: /* SKP */
		(void)snprintf(opstr,BUFSTRSIZE,"SKP IF %s %s %s",skipregn_dst[(operand & 0x0007)],skiptype_str[((operand & 0x0700) >> 8)],skipregn_src[((operand & 0x0038) >> 3)]);
		break;
	case 0140120: /* ADDD */
		(void)snprintf(opstr,BUFSTRSIZE,"ADDD");
		break;
	case 0140121: /* SUBD */
		(void)snprintf(opstr,BUFSTRSIZE,"SUBD");
		break;
	case 0140122: /* COMD */
		(void)snprintf(opstr,BUFSTRSIZE,"COMD");
		break;
	case 0140123: /* TSET */
		(void)snprintf(opstr,BUFSTRSIZE,"TSET");
		break;
	case 0140124: /* PACK */
		(void)snprintf(opstr,BUFSTRSIZE,"PACK");
		break;
	case 0140125: /* UPACK */
		(void)snprintf(opstr,BUFSTRSIZE,"UPACK");
		break;
	case 0140126: /* SHDE */
		(void)snprintf(opstr,BUFSTRSIZE,"SHDE");
		break;
	case 0140127: /* RDUS */
		(void)snprintf(opstr,BUFSTRSIZE,"RDUS");
		break;
	case 0140130: /* BFILL */
		(void)snprintf(opstr,BUFSTRSIZE,"BFILL");
		break;
	case 0140131: /* MOVB */
		(void)snprintf(opstr,BUFSTRSIZE,"MOVB");
		break;
	case 0140132: /* MOVBF */
		(void)snprintf(opstr,BUFSTRSIZE,"MOVBF");
		break;
        case 0140133: /* VERSN - ND110 specific */
		if ((CurrentCPUType = ND100) || (CurrentCPUType = ND100CE) || (CurrentCPUType = ND100CX)) /* We are ND100 */
			break;
		else /* We are a ND110, print instruction */
			(void)snprintf(opstr,BUFSTRSIZE,"VERSN");
	case 0140134: /* INIT */
		(void)snprintf(opstr,BUFSTRSIZE,"INIT");
		break;
	case 0140135: /* ENTR */
		(void)snprintf(opstr,BUFSTRSIZE,"ENTR");
		break;
	case 0140136: /* LEAVE */
		(void)snprintf(opstr,BUFSTRSIZE,"LEAVE");
		break;
	case 0140137: /* ELEAV */
		(void)snprintf(opstr,BUFSTRSIZE,"ELEAV");
		break;
	case 0140300: /* SETPT */
		(void)snprintf(opstr,BUFSTRSIZE,"SETPT");
		break;
	case 0140301: /* CLEPT */
		(void)snprintf(opstr,BUFSTRSIZE,"CLEPT");
		break;
	case 0140302: /* CLNREENT */
		(void)snprintf(opstr,BUFSTRSIZE,"CLNREENT");
		break;
	case 0140303: /* CHREENT-PAGES */
		(void)snprintf(opstr,BUFSTRSIZE,"CHREENT-PAGES");
		break;
	case 0140304: /* CLEPU */
		(void)snprintf(opstr,BUFSTRSIZE,"CLEPU");
		break;
	case 0140200: /* USER0 */
		(void)snprintf(opstr,BUFSTRSIZE,"USER0");
		break;
	case 0140500: /* USER1 or ND110 instruction WGLOB */
		if ((CurrentCPUType = ND100) || (CurrentCPUType = ND100CE) || (CurrentCPUType = ND100CX)) /* We are ND100 */
			(void)snprintf(opstr,BUFSTRSIZE,"USER1");
		else
			(void)snprintf(opstr,BUFSTRSIZE,"WGLOB"); /* We are ND110 */
		break;
	case 0140501: /* RGLOB - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"RGLOB");
		break;
	case 0140502: /* INSPL - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"INSPL");
		break;
	case 0140503: /* REMPL - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"REMPL");
		break;
	case 0140504: /* CNREK - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"CNREK");
		break;
	case 0140505: /* CLPT  - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"CLPT");
		break;
	case 0140506: /* ENPT  - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"ENPT");
		break;
	case 0140507: /* REPT  - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"REPT");
		break;
	case 0140510: /* LBIT  - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"LBIT");
		break;
	case 0140513: /* SBITP - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"SBITP");
		break;
	case 0140514: /* LBYTP - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"LBYTP");
		break;
	case 0140515: /* SBYTP - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"SBYTP");
		break;
	case 0140516: /* TSETP - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"TSETP");
		break;
	case 0140517: /* RDUSP - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"RDUSP");
		break;
	case 0140600: /* EXR */
		(void)snprintf(opstr,BUFSTRSIZE,"EXR %s",skipregn_src[((operand & 0x0038) >> 3)]);
		break;
	case 0140700: /* USER2 */
		if ((CurrentCPUType = ND100) || (CurrentCPUType = ND100CE) || (CurrentCPUType = ND100CX)) /* We are ND100 */
			(void)snprintf(opstr,BUFSTRSIZE,"USER2");
		else
			(void)snprintf(opstr,BUFSTRSIZE,"LASB %s",deltastr); /* We are ND110 */
		break;
	case 0140701: /* SASB - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"SASB %s",deltastr);
		break;
	case 0140702: /* LACB - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"LACB %s",deltastr);
		break;
	case 0140703: /* SASB - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"SASB %s",deltastr);
		break;
	case 0140704: /* LXSB - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"LXSB %s",deltastr);
		break;
	case 0140705: /* LXCB - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"LXCB %s",deltastr);
		break;
	case 0140706: /* SZSB - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"SZSB %s",deltastr);
		break;
	case 0140707: /* SZCB - ND110 Specific */
		(void)snprintf(opstr,BUFSTRSIZE,"SZCB %s",deltastr);
		break;
	case 0141100: /* USER3 */
		(void)snprintf(opstr,BUFSTRSIZE,"USER3");
		break;
	case 0141200: /* RMPY */
		(void)snprintf(opstr,BUFSTRSIZE,"RMPY %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0141300: /* USER4 */
		(void)snprintf(opstr,BUFSTRSIZE,"USER4");
		break;
	case 0141500: /* USER5 */
		(void)snprintf(opstr,BUFSTRSIZE,"USER5");
		break;
	case 0141600: /* RDIV */
		(void)snprintf(opstr,BUFSTRSIZE,"RDIV %s",skipregn_src[((operand & 0x0038) >> 3)]);
		break;
	case 0141700: /* USER6 */
		(void)snprintf(opstr,BUFSTRSIZE,"USER6");
		break;
	case 0142100: /* USER7 */
		(void)snprintf(opstr,BUFSTRSIZE,"USER7");
		break;
	case 0142200: /* LBYT */
		/* NOTE : moved from old parsing, SKP part, might have introduced P++ probs here */
		(void)snprintf(opstr,BUFSTRSIZE,"LBYT");
		break;
	case 0142300: /* USER8 */
		(void)snprintf(opstr,BUFSTRSIZE,"USER8");
		break;
	case 0142500: /* USER9 */
		(void)snprintf(opstr,BUFSTRSIZE,"USER9");
		break;
	case 0142600: /* SBYT */
		/* NOTE : moved from old parsing, SKP part, might have introduced P++ probs here */
		(void)snprintf(opstr,BUFSTRSIZE,"SBYT");
		break;
	case 0142700: /* GECO - Undocumented instruction */
		(void)snprintf(opstr,BUFSTRSIZE,"GECO");
		break;
	case 0143100: /* MOVEW */
		(void)snprintf(opstr,BUFSTRSIZE,"MOVEW");
		break;
	case 0143200: /* MIX3 */
		(void)snprintf(opstr,BUFSTRSIZE,"MIX3");
		break;
	case 0143300: /* LDATX */
		(void)snprintf(opstr,BUFSTRSIZE,"LDATX");
		break;
	case 0143301: /* LDXTX */
		(void)snprintf(opstr,BUFSTRSIZE,"LDXTX");
		break;
	case 0143302: /* LDDTX */
		(void)snprintf(opstr,BUFSTRSIZE,"LDDTX");
		break;
	case 0143303: /* LDBTX */
		(void)snprintf(opstr,BUFSTRSIZE,"LDBTX");
		break;
	case 0143304: /* STATX */
		(void)snprintf(opstr,BUFSTRSIZE,"STATX");
		break;
	case 0143305: /* STZTX */
		(void)snprintf(opstr,BUFSTRSIZE,"STZTX");
		break;
	case 0143306: /* STDTX */
		(void)snprintf(opstr,BUFSTRSIZE,"STDTX");
		break;
	case 0143500: /* LWCS */
		(void)snprintf(opstr,BUFSTRSIZE,"LWCS");
		break;
	case 0143604: /* IDENT PL10 */
		(void)snprintf(opstr,BUFSTRSIZE,"IDENT PL10");
		break;
	case 0143611: /* IDENT PL11 */
		(void)snprintf(opstr,BUFSTRSIZE,"IDENT PL11");
		break;
	case 0143622: /* IDENT PL12 */
		(void)snprintf(opstr,BUFSTRSIZE,"IDENT PL12");
		break;
	case 0143643: /* IDENT PL13 */
		(void)snprintf(opstr,BUFSTRSIZE,"IDENT PL13");
		break;
	case 0144000: /* SWAP */
		(void)snprintf(opstr,BUFSTRSIZE,"SWAP %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0144100: /* SWAP CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"SWAP CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0144200: /* SWAP CM1 */
		(void)snprintf(opstr,BUFSTRSIZE,"SWAP CM1 %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0144300: /* SWAP CM1 CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"SWAP CM1 CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0144400: /* RAND */
		(void)snprintf(opstr,BUFSTRSIZE,"RAND %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0144500: /* RAND CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RAND CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0144600: /* RAND CM1 */
		(void)snprintf(opstr,BUFSTRSIZE,"RAND CM1 %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0144700: /* RAND CM1 CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RAND CM1 CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0145000: /* REXO */
		(void)snprintf(opstr,BUFSTRSIZE,"REXO %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0145100: /* REXO CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"REXO CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0145200: /* REXO CM1 */
		(void)snprintf(opstr,BUFSTRSIZE,"REXO CM1 %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0145300: /* REXO CM1 CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"REXO CM1 CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0145400: /* RORA */
		(void)snprintf(opstr,BUFSTRSIZE,"RORA %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0145500: /* RORA CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RORA CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0145600: /* RORA CM1 */
		(void)snprintf(opstr,BUFSTRSIZE,"RORA CM1 %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0145700: /* RORA CM1 CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RORA CM1 CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0146000: /* RADD */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0146100: /* RADD CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0146200: /* RADD CM1 */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD CM1 %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0146300: /* RADD CM1 CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD CM1 CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0146400: /* RADD AD1 */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD AD1 %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0146500: /* RADD AD1 CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD AD1 CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0146600: /* RADD AD1 CM1 */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD AD1 CM1 %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0146700: /* RADD AD1 CM1 CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD AD1 CM1 CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0147000: /* RADD ADC */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD ADC %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0147100: /* RADD ADC CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD ADC CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0147200: /* RADD ADC CM1 */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD ADC CM1 %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0147300: /* RADD ADC CM1 CLD */
		(void)snprintf(opstr,BUFSTRSIZE,"RADD ADC CM1 CLD %s %s",skipregn_src[((operand & 0x0038) >> 3)],skipregn_dst[(operand & 0x0007)]);
		break;
	case 0147400: /* NOOP */
	case 0147500: /* NOOP */
	case 0147600: /* NOOP */
	case 0147700: /* NOOP */
		(void)snprintf(opstr,BUFSTRSIZE,"ROP NOOP");
		break;
	case 0150000: /* TRA */
		(void)snprintf(opstr,BUFSTRSIZE,"TRA %s", intregn_r[nibble]);
		break;
	case 0150100: /* TRR */
		(void)snprintf(opstr,BUFSTRSIZE,"TRR %s", intregn_w[nibble]);
		break;
	case 0150200: /* MCL */
		(void)snprintf(opstr,BUFSTRSIZE,"MCL %s", intregn_w[nibble]);
		break;
	case 0150300: /* MST */
		(void)snprintf(opstr,BUFSTRSIZE,"MST %s", intregn_w[nibble]);
		break;
	case 0150400: /* OPCOM */
		(void)snprintf(opstr,BUFSTRSIZE,"OPCOM");
		break;
	case 0150401: /* IOF */
		(void)snprintf(opstr,BUFSTRSIZE,"IOF");
		break;
	case 0150402: /* ION */
		(void)snprintf(opstr,BUFSTRSIZE,"ION");
		break;
	case 0150404: /* POF */
		(void)snprintf(opstr,BUFSTRSIZE,"POF");
		break;
	case 0150405: /* PIOF */
		(void)snprintf(opstr,BUFSTRSIZE,"PIOF");
		break;
	case 0150406: /* SEX */
		(void)snprintf(opstr,BUFSTRSIZE,"SEX");
		break;
	case 0150407: /* REX */
		(void)snprintf(opstr,BUFSTRSIZE,"REX");
		break;
	case 0150410: /* PON */
		(void)snprintf(opstr,BUFSTRSIZE,"PON");
		break;
	case 0150412: /* PION */
		(void)snprintf(opstr,BUFSTRSIZE,"PION");
		break;
	case 0150415: /* IOXT */
		(void)snprintf(opstr,BUFSTRSIZE,"IOXT");
		break;
	case 0150416: /* EXAM */
		(void)snprintf(opstr,BUFSTRSIZE,"EXAM");
		break;
	case 0150417: /* DEPO */
		(void)snprintf(opstr,BUFSTRSIZE,"DEPO");
		break;
	case 0151000: /* WAIT */
		(void)snprintf(opstr,BUFSTRSIZE,"WAIT"); /* TODO:: number??*/
		break;
	case 0151400: /* NLZ*/
		(void)snprintf(opstr,BUFSTRSIZE,"NLZ %s",numstr);
		break;
	case 0152000: /* DNZ*/
		(void)snprintf(opstr,BUFSTRSIZE,"DNZ %s",numstr);
		break;
	case 0152400: /* SRB */ /* NOTE: These two seems to have bit req on 0-2 as well */
		(void)snprintf(opstr,BUFSTRSIZE,"SRB %o",(operand & 0x0078));
		break;
	case 0152600: /* LRB */ /* NOTE: These two seems to have bit req on 0-2 as well */
		(void)snprintf(opstr,BUFSTRSIZE,"LRB %o",(operand & 0x0078) >> 3);
		break;
	case 0153000: /* MON */
		(void)snprintf(opstr,BUFSTRSIZE,"MON %o",(operand & 0x00ff));
		break;
	case 0153400: /* IRW */
		(void)snprintf(opstr,BUFSTRSIZE,"IRW %o %s",(operand & 0x0078), regn_w[(operand & 0x0007)]);
		break;
	case 0153600: /* IRR */
		(void)snprintf(opstr,BUFSTRSIZE,"IRR %o %s",(operand & 0x0078), regn_w[(operand & 0x0007)]);
		break;
	case 0154000: /* SHT */
		/* negative value -> shift right  else shift left*/
		isneg = ((operand & 0x0020)>>5) ? 1:0;
//		offset = ((operand & 0x0020)>>5) ? (char)((operand & 0x003F) | 0x00C0) : (operand & 0x003F);
		offset = (isneg) ? (~((operand & 0x003F) | 0xFFC0)+1) : (operand & 0x003F);
		(isneg) ? (void)snprintf(numstr,BUFSTRSIZE,"SHR %o",offset) : (void)snprintf(numstr,BUFSTRSIZE,"%o",offset);
		(void)snprintf(opstr,BUFSTRSIZE,"SHT %s%s",shtype_str[((operand & 0x0600) >> 9)],numstr);
		break;
	case 0154200: /* SHD */
		/* negative value -> shift right  else shift left*/
		isneg = ((operand & 0x0020)>>5) ? 1:0;
		offset = (isneg) ? (~((operand & 0x003F) | 0xFFC0)+1) : (operand & 0x003F);
		(isneg) ? (void)snprintf(numstr,BUFSTRSIZE,"SHR %o",offset) : (void)snprintf(numstr,BUFSTRSIZE,"%o",offset);
//		offset = ((operand & 0x0020)>>5) ? (char)((operand & 0x003F) | 0x00C0) : (operand & 0x003F);
//		((int)offset <0) ? (void)snprintf(numstr,BUFSTRSIZE,"SHR %o",-(int)offset) : (void)snprintf(numstr,BUFSTRSIZE,"%o",offset);
		(void)snprintf(opstr,BUFSTRSIZE,"SHD %s%s",shtype_str[((operand & 0x0600) >> 9)],numstr);
		break;
	case 0154400: /* SHA */
		/* negative value -> shift right  else shift left*/
		isneg = ((operand & 0x0020)>>5) ? 1:0;
		offset = (isneg) ? (~((operand & 0x003F) | 0xFFC0)+1) : (operand & 0x003F);
		(isneg) ? (void)snprintf(numstr,BUFSTRSIZE,"SHR %o",offset) : (void)snprintf(numstr,BUFSTRSIZE,"%o",offset);
//		offset = ((operand & 0x0020)>>5) ? (char)((operand & 0x003F) | 0x00C0) : (operand & 0x003F);
//		((int)offset <0) ? (void)snprintf(numstr,BUFSTRSIZE,"SHR %o",-(int)offset) : (void)snprintf(numstr,BUFSTRSIZE,"%o",offset);
		(void)snprintf(opstr,BUFSTRSIZE,"SHA %s%s",shtype_str[((operand & 0x0600) >> 9)],numstr);
		break;
	case 0154600: /* SAD */
		/* negative value -> shift right  else shift left*/
		isneg = ((operand & 0x0020)>>5) ? 1:0;
		offset = (isneg) ? (~((operand & 0x003F) | 0xFFC0)+1) : (operand & 0x003F);
		(isneg) ? (void)snprintf(numstr,BUFSTRSIZE,"SHR %o",offset) : (void)snprintf(numstr,BUFSTRSIZE,"%o",offset);
//		offset = ((operand & 0x0020)>>5) ? (char)((operand & 0x003F) | 0x00C0) : (operand & 0x003F);
//		((int)offset <0) ? (void)snprintf(numstr,BUFSTRSIZE,"SHR %o",-(int)offset) : (void)snprintf(numstr,BUFSTRSIZE,"%o",offset);
		(void)snprintf(opstr,BUFSTRSIZE,"SAD %s%s",shtype_str[((operand & 0x0600) >> 9)],numstr);
		break;
	case 0160000: /* IOT */
		(void)snprintf(opstr,BUFSTRSIZE,"IOT %o",(operand & 0x07ff));
		break;
	case 0164000: /* IOX */
		(void)snprintf(opstr,BUFSTRSIZE,"IOX %o",(operand & 0x07ff));
		break;
	case 0170000: /* SAB */
		(void)snprintf(opstr,BUFSTRSIZE,"SAB %s",numstr);
		break;
	case 0170400: /* SAA */
		(void)snprintf(opstr,BUFSTRSIZE,"SAA %s",numstr);
		break;
	case 0171000: /* SAT */
		(void)snprintf(opstr,BUFSTRSIZE,"SAT %s",numstr);
		break;
	case 0171400: /* SAX */
		(void)snprintf(opstr,BUFSTRSIZE,"SAX %s",numstr);
		break;
	case 0172000: /* AAB */
		(void)snprintf(opstr,BUFSTRSIZE,"AAB %s",numstr);
		break;
	case 0172400: /* AAA */
		(void)snprintf(opstr,BUFSTRSIZE,"AAA %s",numstr);
		break;
	case 0173000: /* AAT */
		(void)snprintf(opstr,BUFSTRSIZE,"AAT %s",numstr);
		break;
	case 0173400: /* AAX */
		(void)snprintf(opstr,BUFSTRSIZE,"AAX %s",numstr);
		break;
	case 0174000: /* BSET ZRO */
	case 0174200: /* BSET ONE */
	case 0174400: /* BSET BCM */
	case 0174600: /* BSET BAC */
	case 0175000: /* BSKP ZRO */
	case 0175200: /* BSKP ONE */
	case 0175400: /* BSKP BCM */
	case 0175600: /* BSKP BAC */
	case 0176000: /* BSTC */
	case 0176200: /* BSTA */
	case 0176400: /* BLDC */
	case 0176600: /* BLDA */
	case 0177000: /* BANC */
	case 0177200: /* BAND */
	case 0177400: /* BORC */
	case 0177600: /* BORA */
		if (!(operand & 0x0007)) /* STS reg bits handling FIXME:: what if it is bit >7 & STS??*/
			(void)snprintf(opstr,BUFSTRSIZE,"%s %s",bop_str[((operand & 0x0780) >> 7)],bopstsbit_str[((operand & 0x0078) >> 3)]);
		else
			(void)snprintf(opstr,BUFSTRSIZE,"%s %o D%s",bop_str[((operand & 0x0780) >> 7)],(int)(operand & 0x0078), regn[(operand & 0x0007)]);
		break;
	default: /* UNDEF */ /* Some ND instruction codes is undefined unfortunately. */
		(void)snprintf(opstr,BUFSTRSIZE,"UNDEF");
		break;
	}
}

/* STZ
 */
void ndfunc_stz(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	MemoryWrite(0,eff_addr,UseAPT,2);
	gPC++;
}

/* STA
 */
void ndfunc_sta(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	trace_step(1,"(%06o)<=A",(int)eff_addr);
	MemoryWrite(gA,eff_addr,UseAPT,2);
	gPC++;
}

/* STT
 */
void ndfunc_stt(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	MemoryWrite(gT,eff_addr,UseAPT,2);
	gPC++;
}

/* STX
 */
void ndfunc_stx(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	MemoryWrite(gX,eff_addr,UseAPT,2);
	gPC++;
}

/* STD
 */
void ndfunc_std(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	MemoryWrite(gA,eff_addr + 0,UseAPT,2);
	MemoryWrite(gD,eff_addr + 1,UseAPT,2);
	gPC++;
}

/* STF
 */
void ndfunc_stf(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	MemoryWrite(gT,eff_addr + 0,UseAPT,2);
	MemoryWrite(gA,eff_addr + 1,UseAPT,2);
	MemoryWrite(gD,eff_addr + 2,UseAPT,2);
	gPC++;
}

/* STZTX
 */
void ndfunc_stztx(ushort operand){
	ushort temp;
	unsigned int fulladdress;
	temp = (operand & 0x0038)>>3;
	fulladdress = (((unsigned int)gT) <<16) | (ushort)(gX + temp);
	PhysMemWrite(0,fulladdress);
	/* TODO:: privilege check... */
	gPC++;
}

/* STATX
 */
void ndfunc_statx(ushort operand){
	ushort temp;
	unsigned int fulladdress;
	temp = (operand & 0x0038)>>3;
	fulladdress = (((unsigned int)gT) <<16) | (ushort)(gX + temp);
	PhysMemWrite(gA,fulladdress);
	/* TODO:: privilege check... */
	gPC++;
}

/* STDTX
 */
void ndfunc_stdtx(ushort operand){
	ushort temp;
	unsigned int fulladdress;
	temp = (operand & 0x0038)>>3;
	fulladdress = (((unsigned int)gT) <<16) | (ushort)(gX + temp);
	PhysMemWrite(gA,fulladdress);
	fulladdress++;
	PhysMemWrite(gD,fulladdress);
	/* TODO:: privilege check... */
	gPC++;
}

/* LDA
 */
void ndfunc_lda(ushort operand){
	if (trace) trace_pre(1,"A",(int)gA);
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	gA = MemoryRead(eff_addr,UseAPT);
	if (DISASM)
		disasm_set_isdata(eff_addr);
	trace_step(1,"A<=(%06o)",(int)eff_addr);
	gPC++;
	if (trace) trace_post(1,"A",(int)gA);
}

/* LDT
 */
void ndfunc_ldt(ushort operand){
	if (trace) trace_pre(1,"T",(int)gT);
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	gT = MemoryRead(eff_addr,UseAPT);
	if (DISASM)
		disasm_set_isdata(eff_addr);
	gPC++;
	if (trace) trace_post(1,"T",(int)gT);
}

/* LDX
 */
void ndfunc_ldx(ushort operand){
	if (trace) trace_pre(1,"X",(int)gX);
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	gX = MemoryRead(eff_addr,UseAPT);
	if (DISASM)
		disasm_set_isdata(eff_addr);
	gPC++;
	if (trace) trace_post(1,"X",(int)gX);
}

/* LDD
 */
void ndfunc_ldd(ushort operand){
	if (trace) trace_pre(2,"A",(int)gA,"D",(int)gD);
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	gA = MemoryRead(eff_addr + 0,UseAPT);
	gD = MemoryRead(eff_addr + 1,UseAPT);
	if (DISASM){
		disasm_set_isdata(eff_addr+0);
		disasm_set_isdata(eff_addr+1);
	}
	if (trace) trace_post(2,"A",(int)gA,"D",(int)gD);
	gPC++;
}

/* LDF
 */
void ndfunc_ldf(ushort operand){
	if (trace) trace_pre(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	gT = MemoryRead(eff_addr + 0,UseAPT);
	gA = MemoryRead(eff_addr + 1,UseAPT);
	gD = MemoryRead(eff_addr + 2,UseAPT);
	if (trace) trace_post(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	gPC++;
}

/* LDATX
 */
void ndfunc_ldatx(ushort operand){
	ushort temp;
	unsigned int fulladdress;
	temp = (operand & 0x0038)>>3;
	fulladdress = (((unsigned int)gT) <<16) | (ushort)(gX + temp);
	gA = PhysMemRead(fulladdress);
	/* TODO:: privilege check... */
	gPC++;
}

/* LDXTX
 */
void ndfunc_ldxtx(ushort operand){
	ushort temp;
	unsigned int fulladdress;
	temp = (operand & 0x0038)>>3;
	fulladdress = (((unsigned int)gT) <<16) | (ushort)(gX + temp);
	gX = PhysMemRead(fulladdress);
	/* TODO:: privilege check... */
	gPC++;
}

/* LDDTX
 */
void ndfunc_lddtx(ushort operand){
	ushort temp;
	unsigned int fulladdress;
	temp = (operand & 0x0038)>>3;
	fulladdress = (((unsigned int)gT) <<16) | (ushort)(gX + temp);
	gA = PhysMemRead(fulladdress);
	fulladdress++;
	gD = PhysMemRead(fulladdress);
	/* TODO:: privilege check... */
	gPC++;
}

/* LDBTX
 */
void ndfunc_ldbtx(ushort operand){
	ushort temp;
	unsigned int fulladdress;
	unsigned int result;
	/* Bit odd instruction. See ND-06.014 section 3.3.10 */
	/* "B: = 177000 ? ((EL) + (EL)) (? = inclusive OR)" */
	temp = (operand & 0x0038)>>3;
	fulladdress = (((unsigned int)gT) <<16) | (ushort)(gX + temp);
	/* Ok, lets get all accesses now into temp variable */
	temp = PhysMemRead(fulladdress);
	result = temp + temp;
	temp = PhysMemRead(result);
	gB = 0177000 | temp;
	/* TODO:: privilege check... */
	gPC++;
}

/* MIN
 */
void ndfunc_min(ushort operand){
	bool UseAPT;
	ushort temp;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	temp = MemoryRead(eff_addr,UseAPT);
	temp++;
	MemoryWrite(temp,eff_addr + 0,UseAPT,2);
	if(0 == temp)
		gPC ++;	/* Next instruction is skipped */
	gPC++;
}

/* ADD
 */
void ndfunc_add(ushort operand){
	bool UseAPT;
	ushort eff_word;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	eff_word = MemoryRead(eff_addr,UseAPT);
	gA = do_add(gA,eff_word,0);
	gPC++;
}

/* SUB
 */
void ndfunc_sub(ushort operand){
	bool UseAPT;
	ushort eff_word;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	eff_word = MemoryRead(eff_addr,UseAPT);
	gA = do_add(gA,~eff_word,1);
	gPC++;
}

/* AND
 */
void ndfunc_and(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	gA = gA & MemoryRead(eff_addr,UseAPT);
	gPC++;
}

/* ORA
 */
void ndfunc_ora(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	gA = gA | MemoryRead(eff_addr,UseAPT);
	gPC++;
}

/* FAD
 */
void ndfunc_fad(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	ushort a[3], b[3], r[3];
	int res;

	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	a[0] = gT;
	a[1] = gA;
	a[2] = gD;
	b[0] = MemoryRead(eff_addr + 0,UseAPT);
	b[1] = MemoryRead(eff_addr + 1,UseAPT);
	b[2] = MemoryRead(eff_addr + 2,UseAPT);
	if (trace) trace_pre(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	if (trace) trace_pre(3,"a+0",(int)b[0],"a+1",(int)b[1],"a+2",(int)b[2]);
	res=NDFloat_Add(a,b,r);
	gT = r[0];
	gA = r[1];
	gD = r[2];
	if (res == -1)
		setbit(_STS,_TG,1);
	if (trace) trace_post(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	gPC++;
}

/* FSB
 */
void ndfunc_fsb(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	ushort a[3], b[3], r[3];

	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	b[0] = gT;
	a[0] = gT;
	a[1] = gA;
	a[2] = gD;
	b[0] = MemoryRead(eff_addr + 0,UseAPT);
	b[1] = MemoryRead(eff_addr + 1,UseAPT);
	b[2] = MemoryRead(eff_addr + 2,UseAPT);
	if (trace) trace_pre(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	if (trace) trace_pre(3,"a+0",(int)b[0],"a+1",(int)b[1],"a+2",(int)b[2]);
	NDFloat_Sub(a,b,r);
	gT = r[0];
	gA = r[1];
	gD = r[2];
	if (trace) trace_post(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	gPC++;
}

/* FMU
 */
void ndfunc_fmu(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	ushort a[3], b[3], r[3];

	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	a[0] = gT;
	a[1] = gA;
	a[2] = gD;
	b[0] = MemoryRead(eff_addr + 0,UseAPT);
	b[1] = MemoryRead(eff_addr + 1,UseAPT);
	b[2] = MemoryRead(eff_addr + 2,UseAPT);
	if (trace) trace_pre(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	if (trace) trace_pre(3,"a+0",(int)b[0],"a+1",(int)b[1],"a+2",(int)b[2]);
	NDFloat_Mul(a,b,r);
	gT = r[0];
	gA = r[1];
	gD = r[2];
	if (trace) trace_post(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	gPC++;
}

/* FDV
 */
void ndfunc_fdv(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	ushort a[3], b[3], r[3];

	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	a[0] = gT;
	a[1] = gA;
	a[2] = gD;
	b[0] = MemoryRead(eff_addr + 0,UseAPT);
	b[1] = MemoryRead(eff_addr + 1,UseAPT);
	b[2] = MemoryRead(eff_addr + 2,UseAPT);
	if (trace) trace_pre(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	if (trace) trace_pre(3,"a+0",(int)b[0],"a+1",(int)b[1],"a+2",(int)b[2]);
	NDFloat_Div(a,b,r);
	gT = r[0];
	gA = r[1];
	gD = r[2];
	if (trace) trace_post(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	gPC++;
}

/* JMP
 */
void ndfunc_jmp(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	ushort old_gPC=gPC;

	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	gPC = eff_addr;
	if (DISASM)
		disasm_userel(old_gPC,gPC);

}

/* GECO
 */
void ndfunc_geco(ushort operand){
// TODO: This instruction is not documented in manuals, but test program looks for it, so do NOOP for now
	gPC++;
}

/* VERSN - ND110+
 * IN: uses A reg bit 11-8 as a bitfield to addess the byte of the version number read, the total is 16 bytes
 * so instruction has to be called 16 times, with incremented A each time.
 * OUT: Sets A, T, D
 */
void ndfunc_versn(ushort operand){
	/* STUB FUNCTION - TODO */
	CurrentCPURunMode = STOP; /* OK unimplemented function, lets stop CPU and end program that way */
	gPC++;
}

/* IOT
 * This is really an ND1 instruction
 */
void ndfunc_iot(ushort operand){
/* for now handle it as illegal instruction */
	illegal_instr(operand);
}

/* IOX
 */
void ndfunc_iox(ushort operand){

	if (trace) trace_pre(1,"A",(int)gA);
	io_op(operand & 0x07ff);
	gPC++;
	if (trace) {
		if (gT & 0x01)
			trace_step(1,"(IO:%06o)<=A",(int)(operand & 0x07ff));
		else {
			trace_step(1,"A<=(IO:%06o)",(int)(operand & 0x07ff));
			trace_post(1,"A",(int)gA);
		}
	}
}

/* IOXT
 */
void ndfunc_ioxt(ushort operand){
	if (trace) trace_pre(2,"A",(int)gA,"T",(int)gT);
	io_op(gT);
	gPC++;
	if (trace) {
		if (gT & 0x01)
			trace_step(1,"(IO:%06o)<=A",(int)gT);
		else {
			trace_step(1,"A<=(IO:%06o)",(int)gT);
			trace_post(1,"A",(int)gA);
		}
	}
}

/* SETPT
 */
void ndfunc_setpt(ushort operand){
	/* STUB FUNCTION - TODO */
	CurrentCPURunMode = STOP; /* OK unimplemented function, lets stop CPU and end program that way */
	gPC++;
}

/* CLEPT
 */
void ndfunc_clept(ushort operand){
	/* TODO: Checks for if we implement it or not in current CPUTYPE, priv checks, etc... */
	DoCLEPT();
	gPC++;
}

/* IDENT
 */
void ndfunc_ident(ushort operand){
	switch ((operand & 0x003f)) {
	case 004:
		DoIDENT(10);
		gPC++;
		break;
	case 011:
		DoIDENT(11);
		gPC++;
		break;
	case 022:
		DoIDENT(12);
		gPC++;
		break;
	case 043:
		DoIDENT(13);
		gPC++;
		break;
	default:
		illegal_instr(operand); /* Assume this is how we should hanle it.. TODO: Check!!! */
	}


}

/* OPCOM
 */
void ndfunc_opcom(ushort operand){
	MODE_OPCOM=1;
	gPC++;
}

/* IOF
 */
void ndfunc_iof(ushort operand){
	setbit_STS_MSB(_IONI,0);
	gPC++;
}

/* ION
 */
void ndfunc_ion(ushort operand){
	setbit_STS_MSB(_IONI,1);
	//:TODO: Check if there are higher levels pending!
	gPC++;
}

/* IRW
 */
void ndfunc_irw(ushort operand){
	ushort dr, temp;

	temp = ((operand & 0x0078) >> 3);
	dr = (operand & 0x0007);
	if (( temp == gPIL ) && (dr == 2)) /* P on current level, this becomes NOOP */
		;
	else
		if (!dr)
			gReg->reg[temp][dr] = (gReg->reg[temp][dr] & 0xFF00) | (gA & 0x00FF); /* Only touch lower 8 bits on STS */
		else
			gReg->reg[temp][dr] = gA;
	gPC++;
}

/* IRR
 */
void ndfunc_irr(ushort operand){
	gA = gReg->reg[((operand & 0x0078) >> 3)][(operand & 0x0007)];
	if ((operand & 0x0007) == 0)	/* clear top 8 bits as STS reg read */
		gA &= 0x00FF;
	gPC++;
}

/* EXAM
 */
void ndfunc_exam(ushort operand){
	unsigned int fulladdress;

	fulladdress = (((unsigned int)gA) <<16) | (ushort)gD;
	gT = PhysMemRead(fulladdress);
	/* TODO:: privilege check */
	gPC++;
}

/* DEPO
 */
void ndfunc_depo(ushort operand){
	unsigned int fulladdress;

	fulladdress = (((unsigned int)gA) <<16) | (ushort)gD;
	PhysMemWrite(gT,fulladdress);
	/* TODO:: privilege check */
	gPC++;
}

/* POF
 */
void ndfunc_pof(ushort operand){
	setbit_STS_MSB(_PONI,0);
	gPC++;
}

/* PIOF
 */
void ndfunc_piof(ushort operand){
	setbit_STS_MSB(_IONI,0);
	//:TODO: Check if there are higher levels pending!
	setbit_STS_MSB(_PONI,0);
	gPC++;
}

/* PON
 */
void ndfunc_pon(ushort operand){
	setbit_STS_MSB(_PONI,1);
	gPC++;
}

/* PION
 */
void ndfunc_pion(ushort operand){
	setbit_STS_MSB(_IONI,1);
	setbit_STS_MSB(_PONI,1);
	gPC++;
}

/* REX
 */
void ndfunc_rex(ushort operand){
	setbit_STS_MSB(_SEXI,0);
	gPC++;
}

/* SEX
 */
void ndfunc_sex(ushort operand){
	setbit_STS_MSB(_SEXI,1);
	gPC++;
}

/* AAA
 */
void ndfunc_aaa(ushort operand){
	ushort temp;

	temp = (ushort)(sshort)(char)(operand&0x00ff);
	gA=do_add(gA,temp,0);
	gPC++;
}

/* AAB
 */
void ndfunc_aab(ushort operand){
	ushort temp;

	temp = (ushort)(sshort)(char)(operand&0x00ff);
	gB=do_add(gB,temp,0);
	gPC++;
}

/* AAT
 */
void ndfunc_aat(ushort operand){
	ushort temp;

	temp = (ushort)(sshort)(char)(operand&0x00ff);
	gT=do_add(gT,temp,0);
	gPC++;
}

/* AAX
 */
void ndfunc_aax(ushort operand){
	ushort temp;

	temp = (ushort)(sshort)(char)(operand&0x00ff);
	gX=do_add(gX,temp,0);
	gPC++;
}

/* MON
 */
void ndfunc_mon(ushort operand){
	char offset;
	offset = (char)(operand & 0x00ff);

	gPC++;
	if(emulatemon)
		mon(offset);
	else {			/* :TODO: Check this as it is still untested */
		gReg->reg[14][_T]= (operand & 0x00ff);
		interrupt(14,1<<1); /* Monitor Call */
	}
}

/* SAA
 */
void ndfunc_saa(ushort operand){
	setreg(_A,(int)(char)(operand & 0x00ff));
	gPC++;
}

/* SAB
 */
void ndfunc_sab(ushort operand){
	setreg(_B,(int)(char)(operand & 0x00ff));
	gPC++;
}

/* SAT
 */
void ndfunc_sat(ushort operand){
	setreg(_T,(int)(char)(operand & 0x00ff));
	gPC++;
}

/* SAX
 */
void ndfunc_sax(ushort operand){
	setreg(_X,(int)(char)(operand & 0x00ff));
	gPC++;
}

/* SHT, SHD, SHA, SAD
 */
void ndfunc_shifts(ushort operand){
	ulong double_reg;

	switch((operand >> 7) & 0x03) {
	case 0: /* SHT */
		if (trace) trace_pre(1,"T",(int)gT);
		gT= ShiftReg(gT,operand);
		if (trace) trace_post(1,"T",(int)gT);
		break;
	case 1: /* SHD */
		if (trace) trace_pre(1,"D",(int)gD);
		gD= ShiftReg(gD,operand);
		if (trace) trace_post(1,"D",(int)gD);
		break;
	case 2: /* SHA */
		if (trace) trace_pre(1,"A",(int)gA);
		gA= ShiftReg(gA,operand);
		if (trace) trace_post(1,"A",(int)gA);
		break;
	case 3: /* SAD */
		if (trace) trace_pre(2,"A",(int)gA,"D",(int)gD);
		double_reg = ShiftDoubleReg(((ulong)gA<<16) | gD,operand);
		gA = double_reg >> 16;
		gD = double_reg & 0xFFFF;
		if (trace) trace_post(2,"A",(int)gA,"D",(int)gD);
		break;
	default:	/* can never reach here but... */
		break;
	}
	gPC++;
}

/* NLZ
 */
void ndfunc_nlz(ushort operand){
	if (trace) trace_pre(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	DoNLZ(operand & 0xFF);
	gPC++;
	if (trace) trace_post(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
}

/* DNZ
 */
void ndfunc_dnz(ushort operand){
	if (trace) trace_pre(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
	DoDNZ(operand & 0xFF);
	gPC++;
	if (trace) trace_post(3,"T",(int)gT,"A",(int)gA,"D",(int)gD);
}

/* SRB
 */
void ndfunc_srb(ushort operand){
			/* SRB */ /* NOTE: These two seems to have bit req on 0-2 as well */
	gPC++;		/* NOTE: We count up PC BEFORE as this is indicated by manual */
	DoSRB(operand);
}

/* LRB
 */
void ndfunc_lrb(ushort operand){
			/* SRB */ /* NOTE: These two seems to have bit req on 0-2 as well */
	gPC++;		/* NOTE: We count up PC BEFORE as this is indicated by manual */
	DoLRB(operand);
}

/* JAP
 */
void ndfunc_jap(ushort operand){
	ushort temp;
	ushort old_gPC = gPC;

	if(!((1<<15) & gA)) {
		temp = (ushort)(sshort)(char)(operand&0x00ff);
		gPC=do_add(gPC,temp,0); /* :TODO: need a non flagsetting add here instead */
		if (DISASM)
			disasm_userel(old_gPC,gPC);
	} else
		gPC++;
}

/* JAN
 */
void ndfunc_jan(ushort operand){
	ushort temp;
	ushort old_gPC = gPC;

	if((1<<15) & gA) {
		temp = (ushort)(sshort)(char)(operand&0x00ff);
		gPC=do_add(gPC,temp,0); /* :TODO: need a non flagsetting add here instead */
		if (DISASM)
			disasm_userel(old_gPC,gPC);
	} else
		gPC++;
}

/* JAZ
 */
void ndfunc_jaz(ushort operand){
	ushort temp;
	ushort old_gPC = gPC;

	if (gA == 0) {
		temp = (ushort)(sshort)(char)(operand&0x00ff);
		gPC=do_add(gPC,temp,0); /* :TODO: need a non flagsetting add here instead */
		if (DISASM)
			disasm_userel(old_gPC,gPC);
	} else
		gPC++;
}

/* JAF
 */
void ndfunc_jaf(ushort operand){
	ushort temp;
	ushort old_gPC = gPC;

	if (gA != 0) {
		temp = (ushort)(sshort)(char)(operand&0x00ff);
		gPC=do_add(gPC,temp,0); /* :TODO: need a non flagsetting add here instead */
		if (DISASM)
			disasm_userel(old_gPC,gPC);
	} else
		gPC++;
}

/* JPC
 */
void ndfunc_jpc(ushort operand){
	ushort temp;
	ushort old_gPC = gPC;

	gX++;
	if(!((1<<15) & gX)) {
		temp = (ushort)(sshort)(char)(operand&0x00ff);
		gPC=do_add(gPC,temp,0); /* :TODO: need a non flagsetting add here instead */
		if (DISASM)
			disasm_userel(old_gPC,gPC);
	} else
		gPC++;
}

/* JNC
 */
void ndfunc_jnc(ushort operand){
	ushort temp;
	ushort old_gPC = gPC;

	gX++;
	if(((1<<15) & gX)) {
		temp = (ushort)(sshort)(char)(operand&0x00ff);
		gPC=do_add(gPC,temp,0); /* :TODO: need a non flagsetting add here instead */
		if (DISASM)
			disasm_userel(old_gPC,gPC);
	} else
		gPC++;
}

/* JXZ
 */
void ndfunc_jxz(ushort operand){
	ushort temp;
	ushort old_gPC = gPC;

	if (gX == 0) {
		temp = (ushort)(sshort)(char)(operand&0x00ff);
		gPC=do_add(gPC,temp,0); /* :TODO: need a non flagsetting add here instead */
		if (DISASM)
			disasm_userel(old_gPC,gPC);
	} else
		gPC++;
}

/* JXN
 */
void ndfunc_jxn(ushort operand){
	ushort temp;
	ushort old_gPC = gPC;

	if((1<<15) & gX) {
		temp = (ushort)(sshort)(char)(operand&0x00ff);
		gPC=do_add(gPC,temp,0); /* :TODO: need a non flagsetting add here instead */
		if (DISASM)
			disasm_userel(old_gPC,gPC);
	} else
		gPC++;
}

/* JPL
 */
void ndfunc_jpl(ushort operand){
	bool UseAPT;
	ushort eff_addr;
	ushort old_gPC = gPC;

	eff_addr = New_GetEffectiveAddr(operand,&UseAPT);
	gL = gPC + 1;
	gPC = eff_addr;
	if (DISASM)
		disasm_userel(old_gPC,gPC);
}

/* SKP
 * Skip instructions, this one interleaves with other instructions so might need some extra checkings.
 */
void ndfunc_skp(ushort operand){
	gPC++;
	if(IsSkip(operand))
		gPC++;
}

/* BFILL
 * IN X and T registers point to address and number of bytes.
 * A contains the byte to write
 * Skip return on no error, X and T points to first byte after the filled field.
 *
 * TODO:: This routine MUST be able to be interrupted it seems. ND100 Ref Man, section 3.2.1.7.
 * Seems Bit 13 is used as an interrupt mark, see ND100 Ref Man, page 3-28 on MOVB instruction
 * Which means eventually we have to do this function reentrant. Yuck!! /Roger
 */
void ndfunc_bfill(ushort operand){
	ushort d1,d2,len,addr,i;
	ushort right = (gT & ((ushort)1<<15)) ? 1 : 0; /* Start with right byte? (LSB) */
	bool is_apt = (gT & ((ushort)1<<14)) ? true : false; /* Use APT or not? */
	ushort thebyte = gA & 0xff;
	len=gT & 0x0fff; /* Number of bytes to do */
	addr=gX; /* just in case we do 0 bytes */
	d1=gX;
	d2=gT;
	if (trace) trace_pre(2,"X",(int)gX,"T",(int)gT);
	if (trace) trace_step(1,"S:(%06o)-",(int)gX);
//	if (debug) fprintf(debugfile,"BFILL(ante): gX:%06o gT:%06o byte:%03o len:%d\n",gX,gT,thebyte,len);
	for (i=0;i<len;i++) {
		addr = d1 + ((i+right)>>1); /* Word adress of byte to write */
		MemoryWrite(thebyte,addr,is_apt,((i+right)&1));
	}
	gT &= 0x7000; /* Null number of bytes, as per manual, also null bit 15 */
	gT |= ((i+right) & 1)<<15; /* set bit 15 to point to next free byte */
	gX = d1 + ((i+right)>>1);
//	if (debug) fprintf(debugfile,"BFILL(post): gX:%06o gT:%06o addr:%d len:%d right:%d\n",gX,gT,addr,len,right);

	gPC++; /* This function has a SKIP return on no error, which is always? */
	if (trace) trace_step(1,"-E:(%06o)",(int)gX); /* -E:(%06o)<=%s */
	if (trace) trace_post(2,"X",(int)gX,"T",(int)gT);
	gPC++;
}

/* INIT
 * INIT instruction:
 * IN: nothing. uses PC.
 * ADDR  : INIT
 * ADDR+1: Stack demand
 * ADDR+2: Address of stack start
 * ADDR+3: Maximum stack size
 * ADDR+4: Flag
 * ADDR+5: Not used
 * ADDR+6: Error return
 * ADDR+7: Normal return
 */
void ndfunc_init(ushort operand){
	ushort demand,start,maxsize,flag;
/*
	if (debug) {
		fprintf(debugfile,"INIT(ante): PC+1(stack demands):%06o\n",MemoryRead(gPC+1,0));
		fprintf(debugfile,"INIT(ante): PC+2(stack start):%06o\n",MemoryRead(gPC+2,0));
		fprintf(debugfile,"INIT(ante): PC+3(max stack size):%06o\n",MemoryRead(gPC+3,0));
		fprintf(debugfile,"INIT(ante): PC+4(flag):%06o\n",MemoryRead(gPC+4,0));
		fprintf(debugfile,"INIT(ante): PC+5(not used):%06o\n",MemoryRead(gPC+5,0));
		fprintf(debugfile,"INIT(ante): PC+6(error return):%06o\n",MemoryRead(gPC+6,0));
		fprintf(debugfile,"INIT(ante): PC+7(return):%06o\n",MemoryRead(gPC+7,0));
	}
*/
	if (trace) trace_pre(2,"(gPC+1)",(int)MemoryRead(gPC+1,0),"(gPC+2)",(int)MemoryRead(gPC+2,0));
	if (trace) trace_pre(2,"(gPC+3)",(int)MemoryRead(gPC+3,0),"(gPC+4)",(int)MemoryRead(gPC+4,0));
	if (trace) trace_pre(2,"(gPC+6)",(int)MemoryRead(gPC+3,0),"(gPC+7)",(int)MemoryRead(gPC+4,0));
	demand = MemoryRead(gPC+1,0);
	start  = MemoryRead (gPC+2,0);
	maxsize = MemoryRead(gPC+3,0);
	flag = MemoryRead(gPC+4,0);
	if ((start+128+demand-122)>(start+maxsize)) { /* stack overflow */
		gPC+=6;
		return;
	}
	if ( (flag&0x01) !=( gReg->reg[gPIL][_STS] &0x01)) {
		gPC+=6;
		return;
	}
	MemoryWrite(gL+1,start,0,2); /* L+1 ==> LINK */
	trace_step(1,"LINK:(%06o)<=L+1",(int)start);
	trace_step(1,"L+1=%06o",(int)gL+1);
	MemoryWrite(gB,start+1,0,2); /* B   ==> PREVB */
	trace_step(1,"PREVB:(%06o)<=B",(int)start+1);
	trace_step(1,"B=%06o",(int)gB);
	MemoryWrite(start+maxsize,start+3,0,2); /* SMAX */
	trace_step(1,"SMAX:(%06o)<=MAX",(int)start+3);
	trace_step(1,"MAX=%06o",(int)start+maxsize);
	gB = start + 128; /* + 200 oct. */
	trace_step(1,"B<=%06o",(int)start+128);
	/*:TODO:  Flag */
	MemoryWrite(gB+demand-122,start+2,0,2); /* STP */
	trace_step(1,"STP:(%06o)<=B+demand-172",(int)start+2);
	trace_step(1,"B+demand-172=%06o",(int)gB+demand-122);
	gPC+=7;
	if (trace) trace_post(2,"gPC",gPC,"B",gB);
/*
	if (debug) {
		fprintf(debugfile,"INIT(post): LINK(%06o):L+1(%06o)\n",start,MemoryRead(start,0));
		fprintf(debugfile,"INIT(post): PREVB(%06o):old B(%06o)\n",start+1,MemoryRead(start+1,0));
		fprintf(debugfile,"INIT(post): STP(%06o):gB+demand-172(%06o)\n",start+2,MemoryRead(start+2,0));
		fprintf(debugfile,"INIT(post): SMAX(%06o):gB+demand-172(%06o)\n",start+3,MemoryRead(start+3,0));
		fprintf(debugfile,"INIT(post): reserved(%06o):(%06o)\n",start+4,MemoryRead(start+4,0));
		fprintf(debugfile,"INIT(post): ERRCODE(%06o):(%06o)\n",start+5,MemoryRead(start+5,0));
	}
*/
	return;
}

/* ENTR
 * IN: nothing. uses PC.
 * ADDR  : ENTR
 * ADDR+1: Stack demand
 * ADDR+2: Error return
 * ADDR+3: Normal return
 */
void ndfunc_entr(ushort operand){
	ushort oldB,demand,smax,stp;
	if (trace) trace_pre(2,"(gPC+1)",(int)MemoryRead(gPC+1,0),"(gPC+2)",(int)MemoryRead(gPC+2,0));
	if (trace) trace_pre(1,"(gPC+3)",(int)MemoryRead(gPC+3,0));
	demand = MemoryRead(gPC+1,0);
	smax = MemoryRead(gB-125,0); /* SMAX */
	if ((gB+demand-122)>(smax)) { /* stack overflow */
		gPC+=2;
		return;
	}
	stp = MemoryRead(gB-126,0); /* STP */
	oldB = gB;
	gB = stp + 128; /* Advance stack frame */
	MemoryWrite(gL+1,gB-128,0,2); /* L+1 ==> LINK */
	MemoryWrite(oldB,gB-127,0,2); /* B   ==> PREVB */
	MemoryWrite(smax,gB-125,0,2); /* SMAX */
	MemoryWrite(gB+demand-122,gB-126,0,2); /* STP */
	gPC+=3;
}

/* LEAVE
 */
void ndfunc_leave(ushort operand){
	gPC = MemoryRead(gB-128,0);
	gB = MemoryRead(gB-127,0);
}

/* ELEAV
 */
void ndfunc_eleav(ushort operand){
	ushort tmp;
	tmp = MemoryRead(gB-128,0)-1;
	MemoryWrite(tmp,gB-128,0,2); /* LINK */
	MemoryWrite(gA,gB-123,0,2); /* A ==> ERRCODE */
	gPC = MemoryRead(gB-128,0);
	gB = MemoryRead(gB-127,0);
}

/* LBYT
 */
void ndfunc_lbyt(ushort operand){
	ushort temp,mod,offset;
	offset=gX>>1; /* same as divide by 2 */
	mod=gX & 1; /* bit 0 is even/odd indicator */
	if (mod) { /* ODD BYTE = LOW */
		temp=MemoryRead(gT+offset,false) & 0x00FF;
	} else { /* EVEN BYTE = HIGH*/
		temp=(MemoryRead(gT+offset,false) & 0xFF00) >> 8;
	}
	gA= temp;
	if (DISASM)
		disasm_set_isdata(gT+offset);

	gPC++;
/*
	if (debug) fprintf(debugfile,"LBYT: mod(%d) addr:%d value:%d char(%c)\n",mod,gT+offset,gA,gA);
*/
}

/* SBYT
 */
void ndfunc_sbyt(ushort operand){
	ushort mod,offset;
	offset=gX>>1; /* same as divide by 2 */
	mod=gX & 1; /* bit 0 is even/odd indicator */
	if (mod) { /* ODD BYTE = LOW */
		MemoryWrite(gA & 0x00FF,gT+offset,false,1); /* Write LSB */
	} else { /* EVEN BYTE = HIGH*/
		MemoryWrite(gA & 0x00FF,gT+offset,false,0); /* Write MSB */
	}
	if (DISASM)
		disasm_set_isdata(gT+offset);

	gPC++;
/*
	if (debug) fprintf(debugfile,"SBYT: mod(%d) addr:%d value:%d char(%c)\n",mod,gT+offset,gA,gA);
*/
}

/* MIX3
 */
void ndfunc_mix3(ushort operand){
	int tmp;
//	if (debug) fprintf(debugfile,"MIX3(ante): gA:%06o\n",gA);
	tmp = (int)gA-1;
	tmp = tmp *3;
	gX = (ushort) tmp; /* TODO:: Should we have any overflow checking??? */
//	if (debug) fprintf(debugfile,"MIX3(post): gX:%06o\n",gX);

	gPC++;
}


void do_op(ushort operand){
	ushort instr;
	char numstr[BUFSTRSIZE];
	char offset;
	offset = (char)(operand & 0x00ff);

	/* put offset into a string variable in octal with +/- sign for easy reading */
	((int)offset <0) ? (void)snprintf(numstr,BUFSTRSIZE,"-%o",-(int)offset) : (void)snprintf(numstr,BUFSTRSIZE,"%o",offset);

	instr=extract_opcode(operand);

	instr_funcs[operand](operand);	/* call using a function pointer from the array
					this way we are as flexible as possible as we
					implement io calls. */
	prefetch();
}

void illegal_instr(ushort operand){
	interrupt(14,1<<4); /* Illegal Instruction */
	if (trace) trace_step(1,"CODE=%06o",(int)operand);
	gPC++;
}

void unimplemented_instr(ushort operand){
	CurrentCPURunMode = STOP; /* OK unimplemented function, lets stop CPU and end program that way */
	gPC++;
}

void prefetch(){
	ushort temp;
	temp = MemoryFetch(gPC,false);
	gReg->myreg_PFB = temp;
}

void regop(ushort operand) { /* SWAP RAND REXO RORA RADD RCLR EXIT RDCR RING RSUB */
	int RAD,CLD,CM1,tmp;
	ushort sr,dr,source;
	ushort old_gPC = gPC;

	RAD = ((operand & 0x0400) >> 10);
	CM1 = ((operand & 0x0080) >> 7);
	CLD = ((operand & 0x0040) >> 6);

	sr = ((operand & 0x0038) >> 3);
	dr = (operand & 0x0007);
	source = (sr==0) ? 0 : gReg->reg[CurrLEVEL][sr] & 0xFFFF; /* handles special case when sr=STS reg */

	gPC++;	/* Count up first, as if P is used, it's the value of the next instruction. */
	switch (RAD) {
		case 0 : /* Logical operation - SWAP RAND REXO RORA */
			if (dr != 0) {
				switch ((operand & 0x0300) >> 8) {
					case 0: /* SWAP */
						tmp=gReg->reg[CurrLEVEL][dr];	/* temp if we need to do the swap */
						gReg->reg[CurrLEVEL][dr] = (CM1) ? ~source : source;
						gReg->reg[CurrLEVEL][sr] = (CLD) ? 0 : (ushort) (tmp & 0xFFFF);
						break;
					case 1: /* RAND */
						gReg->reg[CurrLEVEL][dr] &= (CM1) ? ~source : source;
						gReg->reg[CurrLEVEL][dr] = (CLD) ? 0 : gReg->reg[CurrLEVEL][dr];
						break;
					case 2: /* REXO */ 
						gReg->reg[CurrLEVEL][dr] = (CLD) ? 
							( (CM1) ? ~source : source ) :
							( (CM1) ? gReg->reg[CurrLEVEL][dr] ^ ~source : gReg->reg[CurrLEVEL][dr] ^ source ) ;
						break;
					case 3: /* RORA */
						gReg->reg[CurrLEVEL][dr] = (CLD) ? 
							( (CM1) ? ~source : source ) :
							( (CM1) ? gReg->reg[CurrLEVEL][dr] | ~source : gReg->reg[CurrLEVEL][dr] | source ) ;
						break;
				}
			}
			break;
		case 1 : /* Arithmetic operation - RADD RCLR EXIT RDCR RINC RSUB */
			if (dr != 0) {
				tmp=gReg->reg[CurrLEVEL][dr];	/* use this insted of (dr) as we need to check for carry and things */
				switch ((operand & 0x0380) >> 7) {
					case 0: /* RADD */
						tmp = (CLD) ? source : do_add(gReg->reg[CurrLEVEL][dr],source,0);
						break;
					case 1: /* RADD CM1 */
						tmp = (CLD) ? ~source : do_add(gReg->reg[CurrLEVEL][dr],~source,0);
						break;
					case 2: /* RADD AD1 */
						tmp = (CLD) ? do_add(0,source,1) : do_add(gReg->reg[CurrLEVEL][dr],source,1);
						break;
					case 3: /* RADD AD1 CM1 */
						tmp = (CLD) ? do_add(0,~source,1)  : do_add(gReg->reg[CurrLEVEL][dr],~source,1);
						break;
					case 4: /* RADD ADC */
						tmp = (CLD) ? do_add(0,source,getbit(_STS,_C)) : do_add(gReg->reg[CurrLEVEL][dr],source,getbit(_STS,_C));
						break;
					case 5: /* RADD ADC CM1 */
						tmp = (CLD) ? do_add(0,~source,getbit(_STS,_C)) : do_add(gReg->reg[CurrLEVEL][dr],~source,getbit(_STS,_C));
						break;
					case 6: /* NOOP */
						break;
					case 7: /* NOOP */
						break;
				}
				gReg->reg[CurrLEVEL][dr]= (ushort)( tmp &0xFFFF);
			} else {
				setbit(_STS,_C,0);
			}
			break;
	}
	if ((DISASM) && (dr == _P)){
		disasm_userel(old_gPC,gPC);
	}

}

/* Calculates the effective address to use.
 * Uses MemoryRead to do this so we get the Page Table handling
 * done correctly.
 * See Manual ND.06.014, Page 34
 * NOTE:: See Table in Section 2.3.5.2. This indicates all MemoryRead bools should be true instead??
 */
ushort GetEffectiveAddr(ushort instr) {
	char disp = instr & 0xFF;
	ushort eff_addr;
	ushort res;
	switch((instr>>8) & 0x07) {
	case 0:	/* (P) + disp */
		res = gPC + disp;
		break;
	case 1:	/* (B) + disp */
		res = gB + disp;
		break;
	case 2:	/* ((P) + disp) */
		eff_addr =  gPC + disp;
		res = MemoryRead(eff_addr,true);
		break;
	case 3:	/* ((B) + disp) */
		eff_addr =  gB + disp;
		res = MemoryRead(eff_addr,false);
		break;
	case 4:	/* (X) + disp */
		res = gX + disp;
		break;
	case 5:	/* (B) + disp + (X) */
		res = gB + gX + disp;
		break;
	case 6:	/* ((P) + disp) + (X) */
		eff_addr =  gPC + disp;
		res = gX + MemoryRead(eff_addr,true);
		break;
	case 7:	/* ((B) + disp) + (X) */
		eff_addr =  gB + disp;
		res = gX + MemoryRead(eff_addr,false);
		break;
	}
	return(res);
}

/* Calculates the effective address to use.
 * Uses MemoryRead to do this so we get the Page Table handling
 * done correctly. Also sets the bool use_apt points to, to tell caller what PT
 * to use for the actual use of the address supplied
 * See Manual ND.06.014, Page 34
 */
ushort New_GetEffectiveAddr(ushort instr, bool *use_apt) {
	char disp = instr & 0xFF;
	ushort eff_addr;
	switch((instr>>8) & 0x07) {
	case 0:	/* (P) + disp */
		eff_addr = gPC + disp;
		*use_apt = false;
		break;
	case 1:	/* (B) + disp */
		eff_addr = gB + disp;
		*use_apt = true;
		break;
	case 2:	/* ((P) + disp) */
		eff_addr = gPC + disp;
		eff_addr = MemoryRead(eff_addr,false);
		*use_apt = true;
		break;
	case 3:	/* ((B) + disp) */
		eff_addr =  gB + disp;
		eff_addr = MemoryRead(eff_addr,true);
		*use_apt = true;
		break;
	case 4:	/* (X) + disp */
		eff_addr= gX + disp;
		*use_apt = true;
		break;
	case 5:	/* (B) + disp + (X) */
		eff_addr = gB + gX + disp;
		*use_apt = true;
		break;
	case 6:	/* ((P) + disp) + (X) */
		eff_addr =  gPC + disp;
		eff_addr = gX + MemoryRead(eff_addr,false);
		*use_apt = true;
		break;
	case 7:	/* ((B) + disp) + (X) */
		eff_addr =  gB + disp;
		eff_addr = gX + MemoryRead(eff_addr,true);
		*use_apt = true;
		break;
	}
	return eff_addr;
}

/*
 * DoMCL - Masked Clear
 *  Affected: Internal register specified
 *  (Only STS, PID & PIE possible)
 *  <IR> = <IR> & (~A)
 *
 * NOTE:: STS need to be checked.
 */
void DoMCL(ushort instr) {
	int s;
	switch(instr & 0x0F) {
	case 01:
		gReg->reg[CurrLEVEL][0] &= ~(gA & 0x00FF);
//		ushort reg_a = gA;
//		SystemSTS &= ~(reg_a & 0xF000);
//		gReg->reg[CurrLEVEL][0] &= ~(reg_a & 0x00FF);
		break;
	case 06:
		/* This affects interrupt, so do locking and checking. */
		if (trace) trace_pre(2,"PID",gPID,"A",gA);

		gPID &= ~gA;
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}

		checkPK();
		if (trace) trace_step(1,"PID {AND}{NOT} A",0);
		if (trace) trace_post(1,"PID",gPID);
		break;
	case 07:
		/* This affects interrupt, so do locking and checking. */
		if (trace) trace_pre(2,"PIE",gPIE,"A",gA);
		while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
			continue; /* Restart if interrupted by handler */
		gPIE &= ~gA;
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		checkPK();
		if (trace) trace_step(1,"PIE {AND}{NOT} A",0);
		if (trace) trace_post(1,"PIE",gPIE);
		break;
	default:
		/* :TODO: Check if we need to do illegal instruction handling */
		break;
	}
	gPC++;
}

/*
 * DoMST - Masked SET
 *  Affected: Internal register specified
 *  (Only STS, PID & PIE possible)
 *  <IR> = <IR> | (A)
 *
 * NOTE:: STS need to be checked.
 */
void DoMST(ushort instr) {
	int s;
	switch(instr & 0x0F) {
	case 01:
		gReg->reg[CurrLEVEL][0] |= (gA & 0x00ff);
//		ushort reg_a = gA;
//		SystemSTS |= reg_a & 0xF000;
//		gReg->reg[CurrLEVEL][0] |= reg_a & 0x00FF;
		break;
	case 06:
		/* This affects interrupt, so do locking and checking. */
		if (trace) trace_pre(2,"PID",gPID,"A",gA);
		while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
			continue; /* Restart if interrupted by handler */
		gPID |= gA;
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		checkPK();
		if (trace) trace_step(1,"PID {AND}{NOT} A",0);
		if (trace) trace_post(1,"PID",gPID);
		break;
	case 07:
		/* This affects interrupt, so do locking and checking. */
		if (trace) trace_pre(2,"PIE",gPIE,"A",gA);
		while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
			continue; /* Restart if interrupted by handler */
		gPIE |= gA;
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		checkPK();
		if (trace) trace_step(1,"PIE {AND}{NOT} A",0);
		if (trace) trace_post(1,"PIE",gPIE);
		break;
	default:
		/* :TODO: Check if we need to do illegal instruction handling */
		break;
	}
	gPC++;
}

/*
 * DoCLEPT - Clear Page Tables
 *  Affected: Pagetables, A, T, X, B registers ????
 *  T,X used as an adress reg  with 24 bits in the xxxTX instructions
 *
 * This instruction apparently is a replacement for this sequence:
 * CLEPT:	JXZ * 10	(if X=0 goto END)
 *		LDBTX 10	(B:=177000|(2*(EL)), EL=T,X+1)
 *		LDA ,B		(A:=(B))
 *		JAZ * 3		(if A=0 goto LOOP)
 *		STATX 20	((EL):=A, EL=T,X+2)
 *		STZ ,B		( (B):=0 )
 *		LDXTX 00	(X:=(EL), EL=T,X)
 * LOOP:	JMP *-7		(goto CLEPT)
 * END:		...
 */
void DoCLEPT() {
	ulong fulladdress;
	ushort eff_addr;
	ushort temp;
	bool UseAPT;
	/* Initial code only.. TODO: Privilege check, and check it actually works... */
	while (gX) {
		/* LDBTX 10 */
		fulladdress = (((unsigned int)gT) <<16) | (ushort)(gX + 1);
		temp = 2 * PhysMemRead(fulladdress);
		gB = 0177000 | PhysMemRead((ulong)temp);
		/* LDA, B */
		eff_addr = New_GetEffectiveAddr(044400,&UseAPT);
		gA = MemoryRead(eff_addr,UseAPT);
		if (gA) {
			/* STATX 20 */
			fulladdress = (((unsigned int)gT) <<16) | (ushort)(gX + 2);
			PhysMemWrite(gA,fulladdress);
			/* STZ, B */
			UseAPT = (0 == (0400 & (0x03<<8))) ? true : false;
			eff_addr = New_GetEffectiveAddr(0400,&UseAPT);
			MemoryWrite(0,eff_addr,UseAPT,2);
			/* LDXTX 00*/
			fulladdress = (((unsigned int)gT) <<16) | gX;
			gX = PhysMemRead(fulladdress);
		}
	}
}

/*
 * DoTRA - Transfer to register
 *  Affected: Accumulator
 *  A = <IR>;
 */
void DoTRA(ushort instr) {
	int s;
	ushort temp,level;
	ushort i;
	switch(instr & 0x0F) {
	case 00: /* TRA PANS */
		gA = gPANS;
		if (trace) trace_step(1,"A<=PANS",0);
		if (debug) fprintf(debugfile,"TRA PANS: A <= %06o\n",gA);
		break;
	case 01: /* TRA STS */
		gA= gReg->reg[gPIL][_STS]; /* If everything is done correctly elsewhere this should work fine */
		if (trace) trace_step(1,"A<=STS",0);
		break;
	case 02: /* TRA OPR */
		gA = gOPR;
		if (trace) trace_step(1,"A<=OPR",0);
		if (debug) fprintf(debugfile,"TRA OPR:\n");
		break;
	case 03: /* TRA PGS */
		/* TODO:: Check that this also is supposed to clear the PGS as it "unlocks" it */
		while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
			continue; /* Restart if interrupted by handler */
		gA = gPGS;
		gPGS=0;
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		if (trace) trace_step(1,"A<=PGS",0);
		break;
	case 04: /* TRA PVL */
		/* This one has a strange format. Described in ND-100 Functional Description section 2.9.2.5.4 */
		gA = 0; /* Clean it */
		gA = (gPVL & 0x07)<<3 | 0xd782; /* = IRR (PVL) DP */
		if (trace) trace_step(1,"A<=PVL",0);
		break;
	case 05: /* TRA IIC */
		/* Manuals says(2.2.4.3) that this should be a number equal to the highest bit set in (IID & IIE) - Roger */
		/* Only bit 1-10 is used, so we only return a value between 1 and 10  or else  zero */
		while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
			continue; /* Restart if interrupted by handler */
		gIIC = 0;
		temp=gIID & gIIE;
		for (i=1;i<16;i++) {
			gIIC = ((1<<i) & temp) ? i : gIIC;
		}
		gA = gIIC;
		gIID = 0;
		gReg->mylock_IIC = false;
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		if (trace) trace_step(1,"A<=IIC",0);
		break;
	case 06:
		gA = gPID;
		if (trace) trace_step(1,"A<=PID",0);
		break;
	case 07:
		gA = gPIE;
		if (trace) trace_step(1,"A<=PIE",0);
		break;
	case 010:
		gA = gCSR;
		if (trace) trace_step(1,"A<=CSR",0);
		break;
	case 011:
		gA = CurrLEVEL;
		break;
	case 012:
		gA = gALD;
		if (trace) trace_step(1,"A<=ALD",0);
		break;
	case 013:
		gA = gPES;
		break;
	case 014: /* PCR */
		temp = gA;
		level = (temp >> 3) & 0x0f;
		gA = gReg->reg_PCR[level] & 0x0783; /* Mask out so we only get PT, APT, RING as per Manual*/
		break;
	case 015:
		gA = gPEA;
		break;
	default: /* These registers dont exist, so just return 0 for now FIXME: Check correct behaviour.*/
		gA = 0;
	}
	if (trace) trace_post(1,"A",(int)gA);
	gPC++;
}

/*
 * DoEXR - Run instruction in source register
 */
void DoEXR(ushort instr) {
	ushort sr, exr_instr;
	char disasm_str[256];
	sr = (instr >> 3) & 0x07;
	if(sr)
		exr_instr = gReg->reg[CurrLEVEL][sr];
	else
		exr_instr = 0;
	if (trace & 0x01) {
		OpToStr(disasm_str,exr_instr);
		fprintf(tracefile,
			"#o (i,d) #v# (\"%d\",\"EXR instr: %s\");\n",
			(int)instr_counter,disasm_str);
		fprintf(tracefile,
			"#e (i,d) #v# (\"%d\",\"%s\");\n",
			(int)instr_counter,disasm_str);
	}
	if (trace & 0x20) {
	}
	if (0140600 == extract_opcode(exr_instr)) { /* ILLEGAL:: EXR of EXR */
		setbit(_STS,_Z,1);		//:TODO: activate CPU trap on level 14!!!
		return;
	}
	if (DISASM)
		disasm_exr(gPC,exr_instr);
	/* TODO:: Do we have to totally rethink gPC++ handling??? */
	do_op(exr_instr);
}

/*
 * DoWAIT - Give up prio instruction
 * NOTE:: Only basic parts fixed yet, this is a fairly complex one
 * Also no privilege checks are done as of yet.
 */
void DoWAIT(ushort instr) {
	int s;
	ushort temp;
	if(!STS_IONI) { /* Interrupt is off, HALT cpu */
		gPC++;
		CurrentCPURunMode = STOP;
		return;
	}
	if(CurrLEVEL ==0 ) { /* This essentially becomes NOOP */
		gPC++;
	} else {
		gPC++;
		temp= ~(1<<CurrLEVEL); /* Now we have a 0 in the position we want */
		while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
			continue; /* Restart if interrupted by handler */
		gPID &= temp; /* Give up this level */
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		checkPK();
	}
}

/*
 * DoTRR - Transfer to register
 *  Affected: Internal register specified
 *  <IR> = A;
 *
 * NOTE: STS and PCR NOT fixed yet!!!
 */
void DoTRR(ushort instr) {
	int s;
	ushort temp,level;
	if (trace) trace_pre(1,"A",(int)gA);
	switch(instr & 0x0F) {
	case 00:
		gPANC = gA;
		if (trace) trace_step(1,"PANC<=A",0);
		if (PANEL_PROCESSOR) {
			gPAP->trr_panc = true;
			if (sem_post(&sem_pap) == -1) { /* kick panel processor */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure TRR PANC\n");
				CurrentCPURunMode = SHUTDOWN;
			}
		}
		break;
	case 01:
		/* ND-06.029.1 ND-110 Instruction Set, lists only lower 8 bits as changeable... */
		gReg->reg[CurrLEVEL][_STS] = (gReg->reg[CurrLEVEL][_STS] & 0xff00) | (gA & 0x00ff); /* Only change LSB  */
		if (trace) trace_step(1,"STS(LSB)<=A",0);
		break;
	case 02:
		gLMP = gA;
		if (trace) trace_step(1,"LMP<=A",0);
		if (debug) fprintf(debugfile,"TRR LMP: %06o => LMP\n",gA);
		break;
	case 03:
		temp = gA;
		level = (temp >> 3) & 0x0f;
		gReg->reg_PCR[level]= temp & 0x0783; /* Mask out so we only get PT, APT, RING as per Manual*/
		if (trace) trace_step(1,"PCR(%d)<=A",level);
		break;
	case 05:
		while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
			continue; /* Restart if interrupted by handler */
		gIIE = gA;
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		checkPK();
		if (trace) trace_step(1,"IIE<=A",0);
		break;
	case 06:
		/* This affects interrupt, so do locking and checking. */
		while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
			continue; /* Restart if interrupted by handler */
		gPID = gA;
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		checkPK();
		if (trace) trace_step(1,"PID<=A",0);
		break;
	case 07:
		/* This affects interrupt, so do locking and checking. */
		while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
			continue; /* Restart if interrupted by handler */
		gPIE = gA;
		if (sem_post(&sem_int) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		checkPK();
		if (trace) trace_step(1,"PIE<=A",0);
		break;
	case 010:
		gCCL = gA;
		if (trace) trace_step(1,"CCL<=A",0);
		break;
	case 011:
		gLCIL = gA;
		if (trace) trace_step(1,"LCIL<=A",0);
		break;
	case 012:
		gUCIL = gA;
		if (trace) trace_step(1,"UCIL<=A",0);
		break;
	case 013: /* TRR CILP (ND110 only??) */
	case 015: /* TRR ECCR (ND110 only??) */
	case 017: /* TRR CS (ND110 only) */
		break;
	}
	gPC++;
}

/*
 * DoSRB - Store register block.
 *  Affected:(EL),+ 1 +2 + 3 + 4 + 5 + 6 + 7
 *            P    X  T   A   D   L  STS  B
 */
void DoSRB(ushort operand) {
	ushort lvl, addr;
	ushort temp;

	lvl = ((operand & 0x0078) >> 3);
	addr=gX;

	if (trace) trace_pre(1,"X",(int)gX);

	temp = gReg->reg[lvl][_STS] & 0x00ff;

	MemoryWrite(gReg->reg[lvl][_P],addr,false,2);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"(%06o)<=P[%01o]:(%06o)",addr,lvl,gReg->reg[lvl][_P]);
		trace_step(1,(char *)trace_temp_str,0);
	}
	MemoryWrite(gReg->reg[lvl][_X],addr+1,false,2);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"(%06o)<=X[%01o]:(%06o)",addr+1,lvl,gReg->reg[lvl][_X]);
		trace_step(1,(char *)trace_temp_str,0);
	}
	MemoryWrite(gReg->reg[lvl][_T],addr+2,false,2);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"(%06o)<=T[%01o]:(%06o)",addr+2,lvl,gReg->reg[lvl][_T]);
		trace_step(1,(char *)trace_temp_str,0);
	}
	MemoryWrite(gReg->reg[lvl][_A],addr+3,false,2);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"(%06o)<=A[%01o]:(%06o)",addr+3,lvl,gReg->reg[lvl][_A]);
		trace_step(1,(char *)trace_temp_str,0);
	}
	MemoryWrite(gReg->reg[lvl][_D],addr+4,false,2);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"(%06o)<=D[%01o]:(%06o)",addr+4,lvl,gReg->reg[lvl][_D]);
		trace_step(1,(char *)trace_temp_str,0);
	}
	MemoryWrite(gReg->reg[lvl][_L],addr+5,false,2);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"(%06o)<=L[%01o]:(%06o)",addr+5,lvl,gReg->reg[lvl][_L]);
		trace_step(1,(char *)trace_temp_str,0);
	}
	MemoryWrite(temp,addr+6,false,2); /* Only write LSB of STS */
	if (trace) {
		(void)snprintf(trace_temp_str,255,"(%06o)<=STS[%01o]:(%06o)",addr+6,lvl,gReg->reg[lvl][_STS]);
		trace_step(1,(char *)trace_temp_str,0);
	}
	MemoryWrite(gReg->reg[lvl][_B],addr+7,false,2);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"(%06o)<=B[%01o]:(%06o)",addr+7,lvl,gReg->reg[lvl][_B]);
		trace_step(1,(char *)trace_temp_str,0);
	}
}

/*
 * DoLRB - Load register block.
 *           (EL),+ 1 +2 + 3 + 4 + 5 + 6 + 7
 *  Affected:  P    X  T   A   D   L  STS  B
 */
void DoLRB(ushort operand) {
	ushort lvl, addr;

	lvl = ((operand & 0x0078) >> 3);
	addr=gX;

	if (trace) trace_pre(1,"X",(int)gX);

	if (lvl != CurrLEVEL) {	/* Dont change P on current level if this happens to be specified */
		gReg->reg[lvl][_P]   = MemoryRead(addr,false);
		if (trace) {
			(void)snprintf(trace_temp_str,255,"P[%01o]<=(%06o):%06o",lvl,addr,MemoryRead(addr,false));
			trace_step(1,(char *)trace_temp_str,0);
		}
	}
	gReg->reg[lvl][_X]   = MemoryRead(addr+1,false);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"X[%01o]<=(%06o):%06o",lvl,addr+1,MemoryRead(addr+1,false));
		trace_step(1,(char *)trace_temp_str,0);
	}
	gReg->reg[lvl][_T]   = MemoryRead(addr+2,false);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"T[%01o]<=(%06o):%06o",lvl,addr+2,MemoryRead(addr+2,false));
		trace_step(1,(char *)trace_temp_str,0);
	}
	gReg->reg[lvl][_A]   = MemoryRead(addr+3,false);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"A[%01o]<=(%06o):%06o",lvl,addr+3,MemoryRead(addr+3,false));
		trace_step(1,(char *)trace_temp_str,0);
	}
	gReg->reg[lvl][_D]   = MemoryRead(addr+4,false);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"D[%01o]<=(%06o):%06o",lvl,addr+4,MemoryRead(addr+3,false));
		trace_step(1,(char *)trace_temp_str,0);
	}
	gReg->reg[lvl][_L]   = MemoryRead(addr+5,false);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"L[%01o]<=(%06o):%06o",lvl,addr+5,MemoryRead(addr+3,false));
		trace_step(1,(char *)trace_temp_str,0);
	}
	gReg->reg[lvl][_STS] = 
		(gReg->reg[lvl][_STS] & 0xff00 ) | (MemoryRead(addr+6,false) & 0x00ff); /* Only load LSB STS */
	if (trace) {
		(void)snprintf(trace_temp_str,255,"STS[%01o]<=(%06o):%06o",lvl,addr+6,MemoryRead(addr+3,false));
		trace_step(1,(char *)trace_temp_str,0);
	}
	gReg->reg[lvl][_B]   = MemoryRead(addr+7,false);
	if (trace) {
		(void)snprintf(trace_temp_str,255,"B[%01o]<=(%06o):%06o",lvl,addr+7,MemoryRead(addr+3,false));
		trace_step(1,(char *)trace_temp_str,0);
	}
}

bool IsSkip(ushort instr) {
	ushort sr, dr, source, desti;
	signed short ss, sd, sgr, ovf;
	char z, o, c, s;
	sr = (instr >> 3) & 0x07;
	dr = (instr >> 0) & 0x07;
	source = (0 == sr) ? 0 : gReg->reg[CurrLEVEL][sr]; /* Never use STS reg but zero value instead */
	desti  = (0 == dr) ? 0 : gReg->reg[CurrLEVEL][dr]; /* Never use STS reg but zero value instead */
	ss = (signed short)source;
	sd = (signed short)desti;

	if (trace) {
		if (sr) (void)snprintf(trace_temp_str,255,"S%s:%06o",regn[sr],source);
		else (void)snprintf(trace_temp_str,255,"0");
		trace_step(1,(char *)trace_temp_str,0);
		if (dr) (void)snprintf(trace_temp_str,255,"D%s:%06o",regn[dr],desti);
		else (void)snprintf(trace_temp_str,255,"0");
		trace_step(1,(char *)trace_temp_str,0);
	}

	/* Ok, lets set flags */
	z = (0 == (desti - source)) ? 1 : 0;
	sgr = sd - ss;
	ovf= (sd & ~ss & ~sgr) | (~sd & ss & sgr); 
	o = (ovf < 0) ? 1 : 0;
	c = ((desti - source) < 0) ? 0 : 1;
	s = ((ushort)(sd - ss)>>15) & 0x01;

	/* And use these to do the skipping, so we try and follow ND behaviour */
	switch ((instr >> 8) & 0x07)
	{
	case 0 :  /* EQL */
		if(z)	return true;
		break;
	case 1 :  /* GEQ */
		if(!s)	return true;
		break;
	case 2 :  /* GRE */
		if(!(s ^ o))	return true;
		break;
	case 3 :  /* MGRE */
		if(c)	return true;
		break;
	case 4 :  /* UEQ */
		if(!z)	return true;
		break;
	case 5 :  /* LSS */
		if(s)	return true;
		break;
	case 6 :  /* LST */
		if(s ^ o)	return true;
		break;
	case 7 :  /* MLST */
		if(!c)	return true;
		break;
	}
	return false;
}

void do_bops(ushort operand) {
	ushort bn, dr, desti;
	gPC++;
	bn = ((operand & 0x0078) >> 3);
	dr = (operand & 0x0007);
	if (trace) trace_pre(1,regn[dr],gReg->reg[CurrLEVEL][dr]);
	switch (( operand & 0x0780) >> 7) {
	case 0 : /* BSET ZRO */
		setbit(dr,bn,0);
		break;
	case 1 : /* BSET ONE */
		setbit(dr,bn,1);
		break;
	case 2 : /* BSET BCM */
		desti=getbit(dr,bn);
		desti^=1; /* XOR with one to invert bit */
		setbit(dr,bn,desti);
		break;
	case 3 : /* BSET BAC */
		setbit(dr,bn,getbit(_STS,_K));
		break;
	case 4 : /* BSKP ZRO */
		if (!getbit(dr,bn)) gPC++; /* Skip next instruction if zero */
		break;
	case 5 : /* BSKP ONE */
		if (getbit(dr,bn)) gPC++; /* Skip next instruction if one */
		break;
	case 6 : /* BSKP BCM */
		if ((getbit(dr,bn)^1)==getbit(_STS,_K)) gPC++; /* Skip next instruction if bit complement */
		break;
	case 7 : /* BSKP BAC */
		if (getbit(dr,bn)==getbit(_STS,_K)) gPC++; /* Skip next instruction if equal */
		break;
	case 8 : /* BSTC */
		setbit(dr,bn,(getbit(_STS,_K)^1));
		setbit(_STS,_K,1);
		break;
	case 9 : /* BSTA */
		setbit(dr,bn,getbit(_STS,_K));
		setbit(_STS,_K,0);
		break;
	case 10 : /* BLDC */
		setbit(_STS,_K,getbit(dr,bn)^1);
		break;
	case 11 : /* BLDA */
		setbit(_STS,_K,getbit(dr,bn));
		break;
	case 12 : /* BANC */
		setbit(_STS,_K,((getbit(dr,bn)^1) & getbit(_STS,_K)));
		break;
	case 13 : /* BAND */
		setbit(_STS,_K,(getbit(dr,bn) & getbit(_STS,_K)));
		break;
	case 14 : /* BORC */
		setbit(_STS,_K,((getbit(dr,bn)^1) | getbit(_STS,_K)));
		break;
	case 15 : /* BORA */
		setbit(_STS,_K,(getbit(dr,bn) | getbit(_STS,_K)));
		break;
	}
	if (trace) {
		(void)snprintf(trace_temp_str,255,"%s=%06o",regn[dr],gReg->reg[CurrLEVEL][dr]);
		trace_step(1,(char *)trace_temp_str,0);
	}
}

ushort ShiftReg(ushort reg, ushort instr) {
	bool isneg = ((instr & 0x0020)>>5) ? 1:0;
	ushort offset = (isneg) ? (~((instr & 0x003F) | 0xFFC0)+1) : (instr & 0x003F);
	ushort shifttype = ((instr >> 9) & 0x03);
	int i,tmp,msb;
	int m = getbit(_STS,_M);
	tmp = m; /* just in case.. */
	for (i=1; i <= offset; i++) {
		tmp = (isneg) ? (reg & 0x01) : ((reg>>15) & 0x01); /* tmp = bit shifted out */
		msb = reg>>15 & 1; /* msb before shift */
		reg = (isneg) ? reg >> 1 : reg << 1;
		switch (shifttype) {
			case 0:	/* Plain */
				reg = (isneg) ? ((reg & 0x7fff) | (msb<<15)) : (reg & 0xfffe); /* SHR : SHL */
				break;
			case 1: /* ROT */
				reg = (isneg) ? ((reg & 0x7fff) | (tmp<<15)) : ((reg & 0xfffe) | tmp);
				break;
			case 2: /* ZIN */
				reg = (isneg) ? (reg & 0x7fff) : (reg & 0xfffe);
				break;
			case 3: /* LIN */
				reg = (isneg) ? ((reg & 0x7fff) | (m<<15)) : ((reg & 0xfffe) | m);
				break;
		}
	}
	setbit(_STS,_M,tmp);
	return reg;
}

ulong ShiftDoubleReg(ulong reg, ushort instr) {
	bool isneg = ((instr & 0x0020)>>5) ? 1:0;
	ushort offset = (isneg) ? (~((instr & 0x003F) | 0xFFC0)+1) : (instr & 0x003F);
	ushort shifttype = ((instr >> 9) & 0x03);
	int i,tmp,msb;
	int m = getbit(_STS,_M);
	tmp = m; /* just in case.. */
	for (i=1; i <= offset; i++) {
		tmp = (isneg) ? (reg & 0x01) : ((reg>>31) & 0x01); /* tmp = bit shifted out */
		msb = reg>>31 & 1; /* msb before shift */
		reg = (isneg) ? reg >> 1 : reg << 1;
		switch (shifttype) {
			case 0:	/* Plain */
				reg = (isneg) ? ((reg & 0x7fffffff) | (msb<<31)) : (reg & 0xfffffffe); /* SHR : SHL */
				break;
			case 1: /* ROT */
				reg = (isneg) ? ((reg & 0x7fffffff) | (tmp<<31)) : ((reg & 0xfffffffe) | tmp);
				break;
			case 2: /* ZIN */
				reg = (isneg) ? (reg & 0x7fffffff) : (reg & 0xfffffffe);
				break;
			case 3: /* LIN */
				reg = (isneg) ? ((reg & 0x7fffffff) | (m<<31)) : ((reg & 0xfffffffe) | m);
				break;
		}
	}
	setbit(_STS,_M,tmp);
	return reg;
}

/*
 * DoIDENT
 * Handles IDENT PLxx instructions
 */
void DoIDENT(char priolevel) {
	int s;
	ushort id = 0;
	struct IdentChain *curr = gIdentChain;
	while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
		continue; /* Restart if interrupted by handler */
	while(curr){
		if (curr->level == priolevel) {
			id = curr->identcode; /* IDENT code found. Store it */
			RemIdentChain(curr); /* Remove item since we now have identified it */
			break;
		} else
			curr=curr->next;
	}
	if (sem_post(&sem_int) == -1) { /* release interrupt lock */
		if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DoIDENT\n");
		CurrentCPURunMode = SHUTDOWN;
	}
	if (id) {
		gA=id; /* Set A reg to ident code */
		if (trace) trace_step(1,"A<=%06o",id);
	} else {
		if (debug) fprintf(debugfile,"DoIDENT IOX Error\n");
		interrupt(14,1<<7); /* IOX Error if no IDENT code found */
		if (trace) trace_pre(2,"PID",gPID,"PIE",gPIE);
	}
	return;
}

/*
 * MOVB instruction. TODO:: Fix edge case and document params here...
 */
void DoMOVB(ushort instr) {
	ushort source,dest,lens,lend,len,s_lr,d_lr,s_apt,d_apt;
	int dir; /* direction, 0=low to high, 1 = high to low */
	int i;
	ushort thebyte;
	ushort addr_d, addr_s;

	addr_d = 0;
	addr_s = 0;
	dir = 0;
	source=gA;
	dest=gX;
	lens=gD & 0x0fff;
	lend=gT & 0x0fff;
	s_lr = ((gD >> 15) & 1);
	d_lr = ((gT >> 15) & 1);
	s_apt = ((gD >> 14) & 1);
	d_apt = ((gT >> 14) & 1);
	len = (((int) lens-lend)<0) ? lens : lend; /* get smallest length as number to copy */
	/* Check overlap if any and direction to copy */
	if (((int)source-dest)<0) {
		dir = 1;
	} else if (((int)source-dest)==0) { /* :TODO: check bytes to determine direction, or if no need to copy exist */

	} else {
		dir = 0;
	}
//	if (debug) fprintf(debugfile,"MOVB(ante): gA:%06o gD:%06o gX:%06o gT:%06o len:%d\n",gA,gD,gX,gT,len);
	/* COPY */
	if (dir) { /* high to low */
		for (i=len-1;i>=0;i--) {
			addr_s = source + ((i+s_lr)>>1); /* Word adress of byte to read */
			thebyte = MemoryRead(addr_s,s_apt);
			thebyte = ((i+d_lr)&1) ? thebyte : (thebyte >> 8) &0xff; /* right, LSB : left, MSB */
			addr_d = dest + ((i+d_lr)>>1); /* Word adress of byte to write */
			MemoryWrite(thebyte,addr_d,d_apt,((i+d_lr)&1));
		}
		i=0;
	} else { /* low to high */
		for (i=0;i<len;i++) {
			addr_s = source + ((i+s_lr)>>1); /* Word adress of byte to read */
			thebyte = MemoryRead(addr_s,s_apt);
			thebyte = ((i+d_lr)&1) ? thebyte : (thebyte >> 8) &0xff; /* right, LSB : left, MSB */
			addr_d = dest + ((i+d_lr)>>1); /* Word adress of byte to write */
			MemoryWrite(thebyte,addr_d,d_apt,((i+d_lr)&1));
		}
	}

	gD &= 0x7000; /* Null number of bytes, as per manual, also null bit 15 */
	gT &= 0x7000; /* Null number of bytes, also null bit 15 */
	gD |= ((i+d_lr) & 1)<<15; /* set bit 15 to point to next free byte */
	gT |= ((i+d_lr) & 1)<<15; /* set bit 15 to point to next free byte */
	gT |= len & 0x0fff; /* number of bytes done to lowest 12 bits*/

	gA = addr_s + ((len+s_lr)>>1);
	gX = addr_d + ((len+d_lr)>>1);
//	if (debug) fprintf(debugfile,"MOVB(post): gA:%06o gD:%06o gX:%06o gT:%06o len:%d\n",gA,gD,gX,gT,len);

	gPC++; /* This function has a SKIP return on no error, which is always? */

	gPC++;
}

/*
 * MOVBF instruction. TODO:: ALL
 */
void DoMOVBF(ushort instr) {
	ushort source,dest,lens,lend,len,s_lr,d_lr,s_apt,d_apt;
	int i;
	ushort thebyte;
	ushort addr_d, addr_s;
	source=gA;
	dest=gX;
	bool overlap;

	addr_d = 0;
	addr_s = 0;
	lens=gD & 0x0fff;
	lend=gT & 0x0fff;
	s_lr = ((gD >> 15) & 1);
	d_lr = ((gT >> 15) & 1);
	s_apt = ((gD >> 14) & 1);
	d_apt = ((gT >> 14) & 1);


	len = (((int) lens-lend)<0) ? lens : lend; /* get smallest length as number to copy */


	if (source>dest) overlap = false;
	else
		if ((ushort)((ushort)(ceil(len /2)) + source -1) > dest) overlap = true;
		else overlap = false;

	if (debug) fprintf(debugfile,"MOVBF(pre): gA:%06o gD:%06o gX:%06o gT:%06o gPC:%06o len:%d\n",gA,gD,gX,gT,gPC,len);
	if (debug) fprintf(debugfile,"MOVBF(dec): gA:%06d gD:%06d gX:%06d gT:%06d gPC:%06d len:%06d\n",gA,gD,gX,gT,gPC,len);
	if (debug) fprintf(debugfile,"MOVBF(pre): overlap=%d\n",overlap);

	for (i=0;i<len;i++) {
		addr_s = source + ((i+s_lr)>>1); /* Word adress of byte to read */
		thebyte = MemoryRead(addr_s,s_apt);
		thebyte = ((i+d_lr)&1) ? thebyte : (thebyte >> 8) &0xff; /* right, LSB : left, MSB */
		addr_d = dest + ((i+d_lr)>>1); /* Word adress of byte to write */
		MemoryWrite(thebyte,addr_d,d_apt,((i+d_lr)&1));
		lens--;
		lend--;
	}

	gA = source + ((len+s_lr)>>1);
	gX = dest + ((len+d_lr)>>1);

	gD &= 0xefff; /* Null bit 12 */
	gT &= 0xcfff; /* Null bit 12 & 13 */
	gD |= ((i+d_lr) & 1)<<15; /* set bit 15 to point to next free byte */
	gT |= ((i+d_lr) & 1)<<15; /* set bit 15 to point to next free byte */

	gD &= 0xf000; /* clean lowest bits before or */
	gT &= 0xf000; /* clean lowest bits before or */
	gD |= lens & 0x0fff; /* decremented byte counter to lowest 12 bits*/
	gT |= lend & 0x0fff; /* decremented byte counter to lowest 12 bits*/


	if (!overlap) gPC++;
	gPC++; /* This function has a SKIP return on no error */
	if (debug) fprintf(debugfile,"MOVBF(post): gA:%06o gD:%06o gX:%06o gT:%06o gPC:%06o len:%d\n",gA,gD,gX,gT,gPC,len);
	if (debug) fprintf(debugfile,"MOVBF(post): addr_s=%d addr_d=%d s_lr=%d d_lr=%d len:%d\n",addr_s,addr_d,s_lr,d_lr,len);
	if (debug) fprintf(debugfile,"*********************************************************************\n");
	return;
}

void add_A_mem(ushort eff_addr, bool UseAPT) {
	int temp, data, oldreg;
	oldreg = gA;
	data = MemoryRead(eff_addr,UseAPT);
	temp = gA + data;

// FIXME - ADD FLAG HANDLING CORRECTLY FOR C,O,Q FLAGS (CHECK AGAIN THINK WE MIGHT HAVE SUBTLE BUGS)

	if (( temp > 0xFFFF) || ( temp <0)) {
		setbit(_STS,_C,1);
		if ((oldreg & 0x8000) && ( data & 0x8000) && !(temp & 0x8000)) {
			setbit(_STS,_Q,1);
		} else {
			setbit(_STS,_Q,0);
		}
	} else {
		setbit(_STS,_C,0);
		if (!(oldreg & 0x8000) && !(data & 0x8000) && (temp & 0x8000)) {
			setbit(_STS,_Q,1);
		} else {
			setbit(_STS,_Q,0);
		}
	}

	gA=(temp  & 0xFFFF);
}

void sub_A_mem(ushort eff_addr, bool UseAPT) {
	int temp, data, oldreg;
	oldreg = gA;
	data = MemoryRead(eff_addr,UseAPT);
	temp = gA - data;
/*
 * FIXME - ADD FLAG HANDLING CORRECTLY FOR C,O,Q FLAGS (CHECK AGAIN THINK WE MIGHT HAVE SUBTLE BUGS)
 */
	if ((temp > 0xFFFF) || (temp < 0)) {
		setbit(_STS,_C,0);
		if ((oldreg & 0x8000) && (data & 0x8000) && !(temp & 0x8000)) {
			setbit(_STS,_Q,1);
		} else {
			setbit(_STS,_Q,0);
		}
	} else {
		setbit(_STS,_C,1);
		if (!(oldreg & 0x8000) && !(data & 0x8000) && (temp & 0x8000)) {
			setbit(_STS,_Q,1);
		} else {
			setbit(_STS,_Q,0);
		}
	}

	gA=(temp & 0xFFFF);
}

/*
 * RDIV
 */
void rdiv(ushort instr){
	sshort divider;
	int dividend;
	div_t result3;          /* stdlib.h */
	/* :TODO: Apparently Carry can be set too. CHECK that... Might be RAD=1??? */
	/* Overflow and division with zero also need to be fixed!! */
	/* :NOTE: The way it is described in the manual, we assume this is a fraction (numerator/denominator and return a quotient and remainder as per manual */
	divider = ((instr & 0x0038) >> 3) ? (sshort)gReg->reg[gPIL][((instr & 0x0038) >> 3)] : 0;
	dividend = ((int)gA << 16) | gD;
	result3 = div(dividend,divider);
	gA = result3.quot;
	gD = result3.rem;
	gPC++;
	return;
}

/*
 * RMPY
 */
void rmpy(ushort instr){
	/* :TODO: Apparently Carry can be set too. CHECK that... Might be RAD=1??? */
	int a,b,result;
	a = ((instr & 0x0038) >> 3) ? (int) gReg->reg[gPIL][((instr & 0x0038) >> 3)] : 0;
	b = (instr & 0x0007) ? (int) gReg->reg[gPIL][(instr & 0x0007)] : 0;
	result = a * b;
	if (abs(result) > INT_MAX) { /* Set O and Q */
		setbit(_STS,_Q,1);
		setbit(_STS,_O,1);
	} else {
		;//:TODO: Carry???;
		setbit(_STS,_Q,0);
		setbit(_STS,_O,0);
	}
	gA = (sshort)((result & 0xffff0000)>>16);
	gD = (sshort)(result & 0x0000ffff);
	gPC++;
	return;
}

/*
 * MPY
 */
void mpy(ushort instr){
	bool UseAPT;
	int a,b,result;
	ushort eff_addr;
	eff_addr = New_GetEffectiveAddr(instr,&UseAPT);
	a = (sshort)gA;
	b = (sshort)MemoryRead(eff_addr,UseAPT);
	result = a * b;
	if (debug) fprintf(debugfile,"MPY: %d = %d * %d\n",result,a,b);
	if (abs(result) > 32767) { /* Set O and Q */
		setbit(_STS,_Q,1);
		setbit(_STS,_O,1);
	} else {
		setbit(_STS,_Q,0);
		setbit(_STS,_O,0);
	}
	gA = (sshort)result;
	gPC++;
	return;
}

void setreg(int r, int val) { /* FIXME - kolla upp flaggor */
	gReg->reg[CurrLEVEL][r]=(ushort) (val & 0xFFFF);
}

ushort getbit(ushort regnum, ushort stsbit) {
	ushort result;
	result=((gReg->reg[CurrLEVEL][regnum] >> stsbit) & 1);
	return result;
}

void clrbit(ushort regnum, ushort stsbit) {
	ushort thebit;
	thebit=(1 << stsbit) ^ 0xFFFF;
	gReg->reg[CurrLEVEL][regnum] = (thebit & gReg->reg[CurrLEVEL][regnum]);
}

/*
 * setbit_STS_MSB:
 * This function handles all setting of MSB STS bits, due to the importance
 * to sync all levels of STS MSB
 * NOTE:: PIL handling is done by setPIL function!!
 * TODO:: Privilege checks
 */
void setbit_STS_MSB(ushort stsbit, char val) {
	int i;
	ushort thebit = 0;
	if (val) {
		thebit=(1 << stsbit);
		for(i=0;i<=15;i++)
			gReg->reg[i][_STS] = gReg->reg[i][_STS] | thebit;
	} else {
		thebit=(1 << stsbit) ^ 0xFFFF;
		for(i=0;i<=15;i++)
			gReg->reg[i][_STS] = gReg->reg[i][_STS] & thebit;
	}


}

void setPIL(char val) {
	int i;
	for(i=0;i<=15;i++){
		gReg->reg[i][_STS] &= 0xf0ff; /* clear PIL bits first */
		gReg->reg[i][_STS] = gReg->reg[i][_STS] | ((val & 0x0f)<<8);
	}
}

void setbit(ushort regnum, ushort stsbit, char val) {
	ushort thebit = 0;
	if (val) {
		thebit=(1 << stsbit);
		gReg->reg[CurrLEVEL][regnum] = (thebit | gReg->reg[CurrLEVEL][regnum]);
	} else {
		thebit=(1 << stsbit) ^ 0xFFFF;
		gReg->reg[CurrLEVEL][regnum] = (thebit & gReg->reg[CurrLEVEL][regnum]);
	}
}

ushort do_add(ushort a, ushort b, ushort k) {
	int tmp;
	bool is_diff;
	tmp = ((int)a) + ((int)b) + ((int)k);
	/* C (carry) */
	if (tmp & 0xffff0000)
		setbit(_STS,_C,1);
	else
		setbit(_STS,_C,0);
	/* O(static overflow), Q (dynamic overflow) */
	is_diff = (((1<<15)&a)^((1<<15)&b)); /* is bit 15 of the two operands different? */
	if(!(is_diff) && (((1<<15)&a)^((1<<15)&tmp))) { /* if equal and result is different... */
		setbit(_STS,_O,1);
		setbit(_STS,_Q,1);
	} else
		setbit(_STS,_Q,0);
	return (ushort)tmp;
}

void AdjustSTS(ushort reg_a, ushort operand, int result) {
	/* C (carry) */
	if(result > 0xFFFF)
		setbit(_STS,_C,1);
	else
		setbit(_STS,_C,0);

	/* O(static overflow), Q (dynamic overflow) */
	if(!(((1<<15) & reg_a)^((1<<15) & operand)) && (((1<<15) & reg_a)^((1<<15) & result))) {
		setbit(_STS,_O,1);
		setbit(_STS,_Q,1);

	} else
		setbit(_STS,_Q,0);
}

/*
 * converts an ascii octal number to an integer.
 * only handles positive values, so if result is negative
 * we have an error. this also means we only handle numbers
 * up to max positive int on the platform.
 */
int aoct2int(char *str){
	double tmp=0;
	int num,count;

	num=strlen(str);
	for(count=0;num>0;num--) {
		if((str[num-1]>='0') && (str[num-1]<='7')) {
			tmp+=(double)((str[num-1]-'0')*pow((double)8,(double)count));
			count++;
		} else {
			return(-1);
		}
	}
	if (tmp > (double) INT_MAX)
		tmp=-1;
	return((int)tmp);
}

/*
 *
 */
void mopc_cmd(char *cmdstr, char cmdc){
	int len;
	int val;
	bool has_val = false;

	len=strlen((const char*)cmdstr);
	if (len > 255)
		return;		/* This is BAAD, so we just silently fail the command at the moment */

	if (len){ /* We probably have an octal argument here */
		val=aoct2int(cmdstr);
		has_val=true;
	}
	switch (cmdc) {
	case '.':
		/* Set breakpoint */
		if (!(has_val));	/*TODO: Check whats needed here */
		if ((val>=0) && (val < 65536)){ /* valid range for 16 bit addr */
			gReg->has_breakpoint = true;
			gReg->breakpoint = (ushort) (val &0xffff);
			CurrentCPURunMode = SEMIRUN;

		}
		break;
	default:
		break;
	}

	if (debug) fprintf(debugfile,"(##)mopc_cmd: ");
	if (debug) fprintf(debugfile,"%s%c\n",cmdstr,cmdc);
	if (debug) fflush(debugfile);
}


/* We run mopc as a thread here, but ticks it either from panel or rtc to get more correct nd behaviour */
/* TODO:: NO ERROR CHECKING CURRENTLY DONE!!!! Need to see how real ND mopc behaves first */
void mopc_thread(){
	int s;
	char ch;
	char str[256];
	unsigned char ptr = 0; /* points to next free char position in str */
	int i;

	if (debug) fprintf(debugfile,"(##)mopc_thread running...\n");
	if (debug) fflush(debugfile);

	memset(str, '\0', sizeof(str));

	while(CurrentCPURunMode != SHUTDOWN) {
		/* This should trigger once every rtc/panel interrupt hopefully */
		while ((s = sem_wait(&sem_mopc)) == -1 && errno == EINTR) /* wait for mopc lock to be free and grab it*/
			continue; /* Restart if interrupted by handler */

		if (debug) fprintf(debugfile,"(##)mopc tick...\n");
		if (debug) fflush(debugfile);

		if (mopc_in(&ch)) { /* char available */
			if (debug) fprintf(debugfile,"(##)mopc char available... char='%c'\n",ch);
			if (debug) fflush(debugfile);

			if ((ch >= '0' && ch <= '7') || (ch >= 'A'  && ch <= 'Y')) {
				str[ptr]=ch;
				ptr++;
				mopc_out(ch);
			} else if ((ch =='@') || (ch == ' ')) {
				ptr=0;
				mopc_out(ch);
			} else if (ch == 10) {
//				mopc_cmd(str,ch);
				mopc_out(ch);
				ptr=0;
			} else if ((ch =='@') || (ch == ' ') || (ch == '<') || (ch =='/') || (ch == '*')) {
				mopc_out(ch);
			} else if ((ch == '&') || (ch == '$')) {
				mopc_out(ch);
			} else if (ch == '.') {
				mopc_out(ch);
				mopc_cmd(str,ch);
				ptr=0;
				memset(str, '\0', sizeof(str));
			} else if (ch == 'Z') {
				mopc_out(ch);
			} else if (ch == '!') {
				if (CurrentCPURunMode == STOP) { 
					mopc_out(ch);
					if(ptr) {	/* ok we have some chars available */
						i=aoct2int(str); /* FIXME :: THIS IS WRONG, we should use octal input, not decimal!!! (just added this quickly to test)*/
						gPC=i;
					}
					CurrentCPURunMode = RUN;
					if (sem_post(&sem_run) == -1) { /* release run lock */
						if (debug) fprintf(debugfile,"ERROR!!! sem_post failure mopc_thread\n");
						CurrentCPURunMode = SHUTDOWN;
					}
				} else {
					mopc_out('?');
				}
			} else if (ch == '#') {
				mopc_out(ch);
			} else if (ch == 27) {
				if (CurrentCPURunMode != STOP)
					MODE_OPCOM=0;
			} else
				mopc_out('?');
		}
	}
}

/*
 * Change PK if conditions for it's setting are right.
 *
 */
void checkPK() {
	int s;
	ushort lvl;
	ushort i;
	gPK=0;
	while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
		continue; /* Restart if interrupted by handler */
	i = gPIE & gPID;
//	if (debug) fprintf(debugfile,"PT DEB CheckPK: gPIE=%06o gPID=%06o i=%06o gPK=%d gPIL=%d\n",gPIE,gPID,i,gPK,gPIL);
	if (i) { /* Do we have a pending interupt condition */
		for (lvl=1;lvl<16;lvl++) {
			gPK = (i & 1<<lvl) ? lvl : gPK; /* Will retain highest bit set as level*/
//			if (debug) fprintf(debugfile,"PT DEB CheckPK: lvl=%d gPK=%d gPIL=%d\n",lvl,gPK,gPIL);
		}
	}
	if (sem_post(&sem_int) == -1) { /* release interrupt lock */
		if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
		CurrentCPURunMode = SHUTDOWN;
	}
}

/*
 * Main interruptsetting routine.
 * IN: interrupt level and possible subbitfield
 * for those levels that has that. (LVL 14).
 */
void interrupt(ushort lvl, ushort sub){
	int s;
	while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
		continue; /* Restart if interrupted by handler */
	if (lvl == 14) {
		gIID |= sub;
		if (gIID & gIIE) gPID|= (1<<14);
	} else {
		gPID |= (1 << lvl) ;
	}
	if (sem_post(&sem_int) == -1) { /* release interrupt lock */
		if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
		CurrentCPURunMode = SHUTDOWN;
	}
	checkPK();
}

/*
 * Check if access is to the PageTables in shadow memory
 * 
 */
bool IsShadowMemAccess(ulong addr) {
	ushort pcr = gReg->reg_PCR[CurrLEVEL];
	unsigned char ring_num = pcr & 0x03;
	if((3 == ring_num) || !(STS_PONI)) {
		if(((STS_SEXI) && (addr >= 0177000) && (addr <01000000)) || (!(STS_SEXI) && (addr >= 0177400) && (addr <01000000))) {
			return true;
		}
	}
	return false;
}

/*
 * Write to shadow mem/pagetables.
 */
void PT_Write(ushort value, ushort addr, ushort byte_select){
	ushort ptadd;
	ulong temp;
	ptadd = (STS_SEXI) ? (addr & 0x01ff) >>1 : (addr & 0x00ff);
	temp = gPT->pt_arr[ptadd];
//	if (debug) fprintf(debugfile,"PT_Write: addr=%06o(%d) ptadd=%d temp=%08x byte_select=%d value=%04x SEXI=%d\n",
//		addr,addr,ptadd,temp,byte_select,value,STS_SEXI);
	switch(byte_select) {
	case 0:	/* MSB in ND */
		if(STS_SEXI)
			temp = (!(addr & 0x01)) ? (temp & 0x00ffffff) | (value << 24) : /* Even addr, MSB = bits 31-24 in PT */
			 (temp & 0xffff00ff) | (value << 8); /* Odd addr, MSB = bits 15-8 in PT */
		else
			temp = (temp & 0x01fffeff) | (ulong)(value & 0xfe)<< 25 | ((value & 0x01) << 8);
		break;
	case 1:	/* LSB in ND */
		if(STS_SEXI)
			temp = (!(addr & 0x01)) ? (temp & 0xff00ffff) | (value & 0xff) << 16 : /* Even addr, LSB = bits 23-16 in PT */
				 (temp & 0xffffff00) | (value & 0xff); /* Odd addr, LSB = bits 7-0 in PT */
		else
			temp = (temp & 0xffffff00) | (value & 0xff);
		break;
	default: /* whole word write */
		if(STS_SEXI)
			temp = (!(addr & 0x01)) ? (temp & 0x0000ffff) | (value << 16) :
				 (temp & 0xffff0000) | value;
		else
			temp = (temp & 0x01fffe00) | (ulong)(value & 0xfe00)<<16 | (value & 0x01ff);
		break;
	}
//	if (debug) fprintf(debugfile,"PT_Write: ==> temp=%08x\n",temp);
	gPT->pt_arr[ptadd]=temp;
	if (trace & 0x08) fprintf(tracefile,
		"#m (i,t,a) #v# (\"%d\",\"Write PageTables\",\"%08o\");\n",
		(int)instr_counter,addr);
	return;
}

/*
 * Read from shadow mem/pagetables.
 */
ushort PT_Read(ushort addr){
	ushort ptadd;
//	ulong temp;
	unsigned int temp;	/* This should be 32 bit always */
	ushort res;
	ptadd = (STS_SEXI) ? (addr & 0x01ff) >>1 : (addr & 0x00ff);
	temp = gPT->pt_arr[ptadd];
	if (debug) fprintf(debugfile,"PT_Read: addr=%06o(%d) ptadd=%d temp=%08x SEXI=%d\n",
		addr,addr,ptadd,temp,STS_SEXI);
	if(STS_SEXI)
		res = (!(addr & 0x01)) ? ((temp & 0xffff0000) >> 16) : /* Even addr */
			(temp & 0x0000ffff); /* Odd addr */
	else
		res = ((temp & 0xfe000000)>>16) | (temp & 0x000001ff);
	if (debug) fprintf(debugfile,"PT_Read: <== res=%04x\n",res);
	return(res); /* PT data */
}

/*
 * Routine that handles phys mem writes and shadow memory.
 */
void PhysMemWrite(ushort value, ulong addr){
	ushort* p_phy_addr;
	if(IsShadowMemAccess(addr)) { /* Write to PageTables!!! */
		PT_Write(value,(ushort)addr,2); /* 2 = word write */
		return;
	}
	addr &= (ND_Memsize); /* Mask it to the memory size we have to prevent coredumps :) */
	p_phy_addr = &VolatileMemory.n_Array[addr];
	*p_phy_addr = value;
}

/*
 * Routine that handles phys mem reads and shadow memory.
 */
ushort PhysMemRead(ulong addr){
	ushort res;
	if(IsShadowMemAccess(addr)) { /* Read from PageTables!!! */
		res = PT_Read((ushort)addr);
		return(res); /* PT data */
	}
	addr &= (ND_Memsize); /* Mask it to the memory size we have to prevent coredumps :) */
	return VolatileMemory.n_Array[addr];
}

/*
 * Write a word to memory.
 * Here we implement all Memory Management System functions.
 */
void MemoryWrite(ushort value, ushort addr, bool UseAPT, unsigned char byte_select) {
	ushort pcr = gReg->reg_PCR[CurrLEVEL];
	unsigned char ring_num = pcr & 0x03;
	unsigned char vpn = addr>>10;
	ushort ppn;
	unsigned char pt_num;
	ulong PTe;
	ushort* p_phy_addr;
//	bool error = false;

	/* just debug the virtual address for now. later on we got to get the real address I think */
	/* this is for now so we can get output of all memory accesses in a program and debug instructions at full speed */
//	if (trace) AddMemTrace((unsigned int)addr,'W');

	/* First we check if Shadow Memory is accessible. */
	if(IsShadowMemAccess((ulong)addr)) { /* Write to PageTables!!! */
		PT_Write(value,addr,byte_select);
		return;
	}
	if (STS_PONI) {
		if((STS_PTM) && UseAPT)
			pt_num = (pcr>>7) & 0x03;	/* APT */
		else
			pt_num = (pcr>>9) & 0x03;	/* PT */

		PTe = gPT->pt[pt_num][vpn];

		/* Check if not WPM(Write Permit bit is not set) */
		if (!(PTe & (1<<31))) {
//			debug=1; /* PT DEBUGGING: remove once finished */
			if (!(PTe & (0x07<<29))) {
				interrupt(14,1<<3);  /* Page Fault */
				gPGS= pt_num<<6 | (vpn & 0x3f) | 1<<14; /* Page Fault */
//				if (debug) fprintf(debugfile,
//					"WriteMemory: Page Fault, instr#=%d PTe=%08x pt_num=%d vpn=%d\n",
//					(int)instr_counter,PTe,pt_num,vpn);
			} else {
				interrupt(14,1<<2); /* Memory Protection Violation */
				gPGS= pt_num<<6 | (vpn & 0x3f) | 1<<14; /* Permit Violation */
//				if (debug) fprintf(debugfile,
//					"WriteMemory: Memory Protection Violation, instr#=%d PTe=%08x pt_num=%d vpn=%d\n",
//						(int)instr_counter,PTe,pt_num,vpn);
			}
			if (trace & 0x08) fprintf(tracefile,
				"#m (i,t,a) #v# (\"%d\",\"Write Fail(WPM)\",\"%08o\");\n",
				(int)instr_counter,addr);
			return;
//			error=true;
		}

		/* Check if ring number is too low */
		if(((PTe>>24) & 0x03) > ring_num) {
//			debug=1; /* PT DEBUGGING: remove once finished */
//			if (debug) fprintf(debugfile,"WriteMemory: Ring Violation, PTe=%08x pt_num=%d vpn=%d\n",PTe,pt_num,vpn);
			gPGS = ((ushort)pt_num<<6) | vpn; /* Ring Violation */
			interrupt(14,1<<2); /* Ring Protection Violation */
			if (trace & 0x08) fprintf(tracefile,
				"#m (i,t,a) #v# (\"%d\",\"Write Fail(Ring)\",\"%08o\");\n",
				(int)instr_counter,addr);
			return;
//			error=true;
		}
//		if(error) return;

		/* Mark that the page was written and used */
		gPT->pt[pt_num][vpn] |= ((ulong)0x03<<27); /* Set WIP and PGU */

		/* Get physical page number */
		ppn = (STS_SEXI) ? PTe & 0x3fff : PTe & 0x01ff;

//		if (debug) fprintf(debugfile,"WriteMemory: OK, PTe=%08x pt_num=%d vpn=%d ppn=%04x\n",PTe,pt_num,vpn,ppn);
//		if (debug) fprintf(debugfile,"WriteMemory: OK, gPT->pt[pt_num][vpn]=%08x\n",gPT->pt[pt_num][vpn]);

		p_phy_addr = &VolatileMemory.n_Pages[ppn][addr & (((ushort)1<<10) - 1)];
		if (trace& 0x08) fprintf(tracefile,
			"#m (i,t,a) #v# (\"%d\",\"Write (PT)\",\"%08o\");\n",
			(int)instr_counter,addr);
	} else {
		p_phy_addr = &VolatileMemory.n_Array[addr];	/* Only 16 address bits in POF mode */
		if (trace & 0x08) fprintf(tracefile,
			"#m (i,t,a) #v# (\"%d\",\"Write ()\",\"%08o\");\n",
			(int)instr_counter,addr);
	}

	// :NOTE: ND memory is big endian but NDemulator is little endian!
	switch(byte_select) {
	case 0:		/* Even, which means MSB byte, or bits 15-8 */
		*p_phy_addr = (*p_phy_addr & 0xFF) | (value<<8);
		break;
	case 1:		/*Odd, which means LSB byte, or bits 7-0 */
		*p_phy_addr = (*p_phy_addr & 0xFF00) | value;
		break;
	default:
		*p_phy_addr = value;
		break;
	}
}

/*
 * Read a word from memory.
 * Here we implement all Memory Management System functions.
 */
ushort MemoryRead(ushort addr, bool UseAPT) {
	ushort pcr = gReg->reg_PCR[CurrLEVEL];
	unsigned char ring_num = pcr & 0x03;
	ulong PTe;
	ushort res;
	unsigned char vpn = addr>>10;
	ushort ppn;
	unsigned char pt_num;
//	bool error = false;


	/* just debug the virtual address for now. later on we got to get the real address I think */
	/* this is for now so we can get output of all memory accesses in a program and debug instructions at full speed */
//	if (trace) AddMemTrace((unsigned int)addr,'R');

	/* First we check if Shadow Memory is accessible. */
	if(IsShadowMemAccess((ulong)addr)) { /* Read from PageTables!!! */
		res = PT_Read(addr);
		return(res); /* PT data */
	}

	if(STS_PONI) {
		if((STS_PTM) && UseAPT)
			pt_num = (pcr>>7) & 0x03;	// APT
		else
			pt_num = (pcr>>9) & 0x03;	// PT

		PTe = gPT->pt[pt_num][vpn];

		/* Check if not RPM(Read Permit bit is not set) */
		if (!(PTe & ((ulong)1<<30))) {
//			debug=1; /* PT DEBUGGING: remove once finished */
			gPGS = ((ushort)1<<14) | ((ushort)pt_num<<6) | vpn;
			if (!(PTe & ((ulong)0x07<<29))) {
//				if (debug) fprintf(debugfile,"ReadMemory: Page Fault, PTe=%08x\n",PTe);
				interrupt(14,1<<3); /* Page Fault */
			} else {
//				if (debug) fprintf(debugfile,"ReadMemory: Memory protection Violation, PTe=%08x\n",PTe);
				interrupt(14,1<<2); /* Memory Protection Violation */
			}
			if (trace & 0x08) fprintf(tracefile,
				"#m (i,t,a) #v# (\"%d\",\"Read Fail(RPM)\",\"%08o\");\n",
				(int)instr_counter,addr);
			return(0); /* TODO:: We should rethink MemoryRead to handle errors more gracefully. */
//			error=true;
		}

		/* Check if ring number is too low */
		if(((PTe>>24) & 0x03) > ring_num) {
// 			debug=1; /* PT DEBUGGING: remove once finished */
			gPGS = ((ushort)pt_num<<6) | vpn;
			interrupt(14,1<<2); /* Ring Protection Violation */
//			if (debug) fprintf(debugfile,"ReadMemory: Ring Violation, PTe=%08x\n",PTe);
			if (trace & 0x08) fprintf(tracefile,
				"#m (i,t,a) #v# (\"%d\",\"Read Fail(Ring)\",\"%08o\");\n",
				(int)instr_counter,addr);
			return(0); /* TODO:: We should rethink MemoryRead to handle errors more gracefully. */
//			error=true;
		}

//		if (error) return;

		/* Mark that the page was used */
		gPT->pt[pt_num][vpn] |= ((ulong)0x01<<27); /* Set PGU */

		/* Get physical page number */
		ppn = (STS_SEXI) ? PTe & 0x3fff : PTe & 0x01ff;

//		if (debug) fprintf(debugfile,"ReadMemory: OK, PTe=%08x pt_num=%d vpn=%d ppn=%04x\n",PTe,pt_num,vpn,ppn);
//		if (debug) fprintf(debugfile,"ReadMemory: OK, gPT->pt[pt_num][vpn]=%08x\n",gPT->pt[pt_num][vpn]);

		if (trace & 0x08) fprintf(tracefile,
			"#m (i,t,a) #v# (\"%d\",\"Read (PT)\",\"%08o\");\n",
			(int)instr_counter,addr);
		return VolatileMemory.n_Pages[ppn][addr & (((ushort)1<<10) - 1)];
	} else {
		if (trace & 0x08) fprintf(tracefile,
			"#m (i,t,a) #v# (\"%d\",\"Read ()\",\"%08o\");\n",
			(int)instr_counter,addr);
		return VolatileMemory.n_Array[addr];	/* Only 16 address bits in POF mode */
	}
}

ushort MemoryFetch(ushort addr, bool UseAPT) {
	ushort pcr = gReg->reg_PCR[CurrLEVEL];
	unsigned char ring_num = pcr & 0x03;
	ulong PTe;
	ushort res;
	unsigned char vpn = addr>>10;
	ushort ppn;
	unsigned char pt_num;
//	bool error = false;

	/* just debug the virtual address for now. later on we got to get the real address I think */
	/* this is for now so we can get output of all memory accesses in a program and debug instructions at full speed */
//	if (trace) AddMemTrace((unsigned int)addr,'F');

	/* First we check if Shadow Memory is accessible. */
	if(IsShadowMemAccess((ulong)addr)) { /* Read from PageTables!!! */
		res = PT_Read(addr);
		return(res); /* PT data */
	}

	if(STS_PONI) {
		if((STS_PTM) && UseAPT)
			pt_num = (pcr>>7) & 0x03;	// APT
		else
			pt_num = (pcr>>9) & 0x03;	// PT

		PTe = gPT->pt[pt_num][vpn];

		/* Check if not FPM(Fetch Permit bit is not set) */
		if (!(PTe & ((ulong)1<<29))) {
// 			debug=1; /* PT DEBUGGING: remove once finished */
			gPGS = ((ushort)3<<14) | ((ushort)pt_num<<6) | vpn;
			if (!(PTe & ((ulong)0x07<<29)))
				interrupt(14,1<<3); /* Page Fault */
			else
				interrupt(14,1<<2); /* Memory Protection Violation */
			if (trace & 0x08) fprintf(tracefile,
				"#m (i,t,a) #v# (\"%d\",\"Fetch Fail(FPM)\",\"%08o\");\n",
				(int)instr_counter,addr);
			return(0); /* TODO:: We should rethink MemoryFetch to handle errors more gracefully. */
//			error = true;
		}

		/* Check if ring number is too low */
		if(((PTe>>24) & 0x03) > ring_num) {
// 			debug=1; /* PT DEBUGGING: remove once finished */
			gPGS = ((ushort)1<<15) | ((ushort)pt_num<<6) | vpn;
			interrupt(14,1<<2); /* Ring Protection Violation */
			if (trace & 0x08) fprintf(tracefile,
				"#m (i,t,a) #v# (\"%d\",\"Fetch Fail(Ring)\",\"%08o\");\n",
				(int)instr_counter,addr);
			return(0); /* TODO:: We should rethink MemoryFetch to handle errors more gracefully. */
//			error = true;
		}

//		if (error) return;

		/* Mark that the page was used */
		gPT->pt[pt_num][vpn] |= ((ulong)0x01<<27); /* Set PGU */

		/* Get physical page number */
		ppn = (STS_SEXI) ? PTe & 0x3fff : PTe & 0x01ff;

//		if (debug) fprintf(debugfile,"FetchMemory: OK, PTe=%08x pt_num=%d vpn=%d ppn=%04x\n",PTe,pt_num,vpn,ppn);
//		if (debug) fprintf(debugfile,"FetchMemory: OK, gPT->pt[pt_num][vpn]=%08x\n",gPT->pt[pt_num][vpn]);

		if (trace & 0x08) fprintf(tracefile,
			"#m (i,t,a) #v# (\"%d\",\"Fetch (PT)\",\"%08o\");\n",
			(int)instr_counter,addr);
		return VolatileMemory.n_Pages[ppn][addr & (((ushort)1<<10) - 1)];
	} else {
		if (trace & 0x08) fprintf(tracefile,
			"#m (i,t,a) #v# (\"%d\",\"Fetch ()\",\"%08o\");\n",
			(int)instr_counter,addr);
		return VolatileMemory.n_Array[addr];	/* Only 16 address bits in POF mode */
	}
}

void cpurun(){
	int s;
	ushort operand, p_now;
	char disasm_str[256];
//	debug=0; /* PT DEBUGGING: remove once finished */
	prefetch(); /* works because gPC should already be setup when cpurun is called */
	gReg->myreg_IR = gReg->myreg_PFB;
	while ((CurrentCPURunMode != STOP) && (CurrentCPURunMode != SHUTDOWN)) {
		if (CurrentCPURunMode == SEMIRUN) { /* Here we should handle single step, breakpoints etc */
			if(gReg->has_breakpoint)
				if (gReg->breakpoint == gPC) {		/* TODO:: Check if we should execute the instruction at breakpoint address or not */
					CurrentCPURunMode = STOP;
					return;
				}
			if (gReg->has_instr_cntr)
				if(gReg->instructioncounter > 0)
					gReg->instructioncounter--;
				else {
					CurrentCPURunMode = STOP;
					return;
				}
		}
		instr_counter++;
		if (trace) trace_pre(1,"S",gReg->reg[CurrLEVEL][0]);
		operand=gReg->myreg_IR;
//		operand=MemoryFetch(gPC,true);
		p_now=gPC;
		if (trace) trace_instr(operand);
		if (DISASM) disasm_instr(gPC,operand);
		do_op(operand);
		if (trace & 0x16) trace_regs();
		if(STS_IONI && (gPK != gPIL)) { /* Time to change runlevel */
			while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
				continue; /* Restart if interrupted by handler */
			gPVL = gPIL; /* Save current runlevel */
			setPIL(gPK); /* Change to new runlevel */
			if (sem_post(&sem_int) == -1) { /* release interrupt lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
				CurrentCPURunMode = SHUTDOWN;
			}
			prefetch(); /* Ok, since we are changing runlevel, we chuck old prefetched instruction and fetch a new one. */
		}
		if (trace) trace_post(1,"S",gReg->reg[CurrLEVEL][0]);
		if (trace) trace_flush();
		gReg->myreg_IR = gReg->myreg_PFB; /* prefetch of next instruction should have been done while executing current one. */
	}
}

void cpu_thread(){
	int s;
	if (debug) fprintf(debugfile,"(##)cpu_thread running...\n");
	if (debug) fflush(debugfile);

	Setup_Instructions();	/* OK lets set up the parsing for our current cpu before we start it. */
	if (DISASM)
		disasm_setlbl(gPC);

	while (CurrentCPURunMode != SHUTDOWN) {
		if(CurrentCPURunMode != STOP) cpurun();

		/* signal that we are now stopped and the routine waiting on us can continue */
		if(CurrentCPURunMode != SHUTDOWN) {
			if (sem_post(&sem_stop) == -1) { /* release stop lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure cpu_thread\n");
				CurrentCPURunMode = SHUTDOWN;
			}
		}

		/* Wait for run signal */
		while ((s = sem_wait(&sem_run)) == -1 && errno == EINTR) /* wait for run lock to be free */
			continue; /* Restart if interrupted by handler */
	}
}

 void AddIdentChain(char lvl, ushort identnum, int callerid){
	struct IdentChain *curr, *new;
	new=calloc(1,sizeof(struct IdentChain));
	new->identcode = identnum;
	new->level = lvl;
	new->callerid = callerid;

	curr=gIdentChain;

	if(curr != NULL ) {
		while (curr->next != NULL) {
			if  (curr->callerid == callerid){ /* We have already registered once, so exit */
				free(new);
				return;
			}
			curr=curr->next;
		}
	} else {
		gIdentChain=new;
		return;
	}
	curr->next=new;
	new->prev=curr;
}

void RemIdentChain(struct IdentChain * elem){
	struct IdentChain *p, *n;
	n=elem->next;
	p=elem->prev;
	/* Unlink this element */
	if(n && p) {
		p->next=n;
		n->prev=p;
	} else if (n){
		n->prev=NULL;
		gIdentChain=n;
	} else if (p) {
		p->next=NULL;
	} else {
		gIdentChain=NULL;
	}
	free(elem);
}

void AddMemTrace(unsigned int addr, char whom){
	struct MemTraceList * curr, * new;
	new=malloc(sizeof(struct MemTraceList));
	new->next = 0;
	new->addr=addr;
	new->funct=whom;
	curr=gMemTrace;
	if (curr != 0) {
		while (curr->next != 0)
			curr=curr->next;
	} else {
		gMemTrace=new;
		return;
	}
	curr->next=new;
}

void DelMemTrace(){
	struct MemTraceList * curr, * head;
	head=gMemTrace;
	while(head){
		curr=head->next;
		free(head);
		head=curr;
	}
	gMemTrace=head;
}

void PrintMemTrace(){
	struct MemTraceList * curr;
	char temp;
	curr=gMemTrace;
	while(curr){
		temp=curr->funct;
		switch (temp) {
		case 'F':
			fprintf(tracefile,"INSERT INTO tracemem (instrnum,type,addr) values (\"%d\",\"Fetch\",\"%08o\");\n",(int)instr_counter,curr->addr);
			break;
		case 'R':
			fprintf(tracefile,"INSERT INTO tracemem (instrnum,type,addr) values (\"%d\",\"Read\",\"%08o\");\n",(int)instr_counter,curr->addr);
			break;
		case 'W':
			fprintf(tracefile,"INSERT INTO tracemem (instrnum,type,addr) values (\"%d\",\"Write\",\"%08o\");\n",(int)instr_counter,curr->addr);
			break;
		default:
			fprintf(tracefile,"INSERT INTO tracemem (instrnum,type,addr) values (\"%d\",\"Impossible\",\"%08o\");\n",(int)instr_counter,curr->addr);
			break;
		}
		curr=curr->next;
	}
}

void Instruction_Add(int start, int stop, void *funcpointer) {
        int i;
        for(i=start;i<=stop;i++)
                instr_funcs[i] = funcpointer;
        return;
}

/*
 * Add IO handler addresses in this function
 * This also thus actually acts as the new instruction parser also.
 */
void Setup_Instructions () {
	Instruction_Add(0000000,0177777,&ndfunc_stz);			/* First make all instructions by default point to illegal_instr  */

	Instruction_Add(0000000,0003777,&ndfunc_stz);			/* STZ  */
	Instruction_Add(0004000,0007777,&ndfunc_sta);			/* STA  */
	Instruction_Add(0010000,0013777,&ndfunc_stt);			/* STT  */
	Instruction_Add(0014000,0017777,&ndfunc_stx);			/* STX  */
	Instruction_Add(0020000,0023777,&ndfunc_std);			/* STD  */
	Instruction_Add(0024000,0027777,&ndfunc_ldd);			/* LDD  */
	Instruction_Add(0030000,0033777,&ndfunc_stf);			/* STF  */
	Instruction_Add(0034000,0037777,&ndfunc_ldf);			/* LDF  */
	Instruction_Add(0040000,0043777,&ndfunc_min);			/* MIN  */
	Instruction_Add(0044000,0047777,&ndfunc_lda);			/* LDA  */
	Instruction_Add(0050000,0053777,&ndfunc_ldt);			/* LDT  */
	Instruction_Add(0054000,0057777,&ndfunc_ldx);			/* LDX  */
	Instruction_Add(0060000,0063777,&ndfunc_add);			/* ADD  */
	Instruction_Add(0064000,0067777,&ndfunc_sub);			/* SUB  */
	Instruction_Add(0070000,0073777,&ndfunc_and);			/* AND  */
	Instruction_Add(0074000,0077777,&ndfunc_ora);			/* ORA  */
	Instruction_Add(0100000,0103777,&ndfunc_fad);			/* FAD  */
	Instruction_Add(0104000,0107777,&ndfunc_fsb);			/* FSB  */
	Instruction_Add(0110000,0113777,&ndfunc_fmu);			/* FMU  */
	Instruction_Add(0114000,0117777,&ndfunc_fdv);			/* FDV  */
	Instruction_Add(0120000,0123777,&mpy);				/* MPY  */
	Instruction_Add(0124000,0127777,&ndfunc_jmp);			/* JMP  */
/* CJPs - Conditional jumps */
	Instruction_Add(0130000,0130377,&ndfunc_jap);			/* JAP */
	Instruction_Add(0130400,0130777,&ndfunc_jan);			/* JAN */
	Instruction_Add(0131000,0131377,&ndfunc_jaz);			/* JAZ */
	Instruction_Add(0131400,0131777,&ndfunc_jaf);			/* JAF */
	Instruction_Add(0132000,0132377,&ndfunc_jpc);			/* JPC */
	Instruction_Add(0132400,0132777,&ndfunc_jnc);			/* JNC */
	Instruction_Add(0133000,0133377,&ndfunc_jxz);			/* JXZ */
	Instruction_Add(0133400,0133777,&ndfunc_jxn);			/* JXN */
	Instruction_Add(0134000,0137777,&ndfunc_jpl);			/* JPL  */
// TODO: Check RANGE!!!!
	Instruction_Add(0140000,0143777,&ndfunc_skp);			/* SKP (this one is special,
									as if bit 7-6 = 00 its a SKIP instruction,
									otherwise it is an extended instruction,
									so any instruction 0140x00 where x=1,2,3,5,6,7
									has to be added below this one*/

	Instruction_Add(0140120,0140120,&unimplemented_instr);		/* ADDD  */
	Instruction_Add(0140121,0140121,&unimplemented_instr);		/* SUBD  */
	Instruction_Add(0140122,0140122,&unimplemented_instr);		/* COMD  */
	Instruction_Add(0140123,0140123,&unimplemented_instr);		/* TSET  */
	Instruction_Add(0140124,0140124,&unimplemented_instr);		/* PACK  */
	Instruction_Add(0140125,0140125,&unimplemented_instr);		/* UPACK */
	Instruction_Add(0140126,0140126,&unimplemented_instr);		/* SHDE  */
	Instruction_Add(0140127,0140127,&unimplemented_instr);		/* RDUS  */
	Instruction_Add(0140130,0140130,&ndfunc_bfill);			/* BFILL */
	Instruction_Add(0140131,0140131,&DoMOVB);			/* MOVB  */
	Instruction_Add(0140132,0140132,&DoMOVBF);			/* MOVBF */

	switch(CurrentCPUType){
	case ND110:
	case ND110CE:
	case ND110CX:
	case ND110PCX:
		Instruction_Add(0140133,0140133,&ndfunc_versn);		/* VERSN - ND110+ */
		break;
	default:
		break;
	}

	Instruction_Add(0140134,0140134,&ndfunc_init);			/* INIT  */
	Instruction_Add(0140135,0140135,&ndfunc_entr);			/* ENTR  */
	Instruction_Add(0140136,0140136,&ndfunc_leave);			/* LEAVE */
	Instruction_Add(0140137,0140137,&ndfunc_eleav);			/* ELEAV */

	Instruction_Add(0140200,0140277,&illegal_instr);		/* USER1 (microcode defined by user or illegal instruction otherwise) */


	switch(CurrentCPUType){
	case ND110:
	case ND110CE:
	case ND110CX:
	case ND110PCX:
		Instruction_Add(0140500,0140500,&unimplemented_instr);	/* WGLOB - ND110 Specific */
		Instruction_Add(0140501,0140501,&unimplemented_instr);	/* RGLOB - ND110 Specific */
		Instruction_Add(0140502,0140502,&unimplemented_instr);	/* INSPL - ND110 Specific */
		Instruction_Add(0140503,0140503,&unimplemented_instr);	/* REMPL - ND110 Specific */
		Instruction_Add(0140504,0140504,&unimplemented_instr);	/* CNREK - ND110 Specific */
		Instruction_Add(0140505,0140505,&unimplemented_instr);	/* CLPT  - ND110 Specific */
		Instruction_Add(0140506,0140506,&unimplemented_instr);	/* ENPT  - ND110 Specific */
		Instruction_Add(0140507,0140507,&unimplemented_instr);	/* REPT  - ND110 Specific */
		Instruction_Add(0140510,0140510,&unimplemented_instr);	/* LBIT  - ND110 Specific */

		Instruction_Add(0140513,0140513,&unimplemented_instr);	/* SBITP - ND110 Specific */
		Instruction_Add(0140514,0140514,&unimplemented_instr);	/* LBYTP - ND110 Specific */
		Instruction_Add(0140515,0140515,&unimplemented_instr);	/* SBYTP - ND110 Specific */
		Instruction_Add(0140516,0140516,&unimplemented_instr);	/* TSETP - ND110 Specific */
		Instruction_Add(0140517,0140517,&unimplemented_instr);	/* RDUSP - ND110 Specific */
		break;
	default:
		Instruction_Add(0140500,0140577,&illegal_instr);	/* USER2 (microcode defined by user or illegal instruction otherwise) */
		break;
	}

	Instruction_Add(0140600,0140677,&DoEXR);			/* EXR */
	switch(CurrentCPUType){
	case ND110:
	case ND110CE:
	case ND110CX:
	case ND110PCX:
		Instruction_Add(0140700,0140700,&unimplemented_instr);	/* LASB - ND110 Specific */
		Instruction_Add(0140701,0140701,&unimplemented_instr);	/* SASB - ND110 Specific */
		Instruction_Add(0140702,0140702,&unimplemented_instr);	/* LACB - ND110 Specific */
		Instruction_Add(0140703,0140703,&unimplemented_instr);	/* SASB - ND110 Specific */
		Instruction_Add(0140704,0140704,&unimplemented_instr);	/* LXSB - ND110 Specific */
		Instruction_Add(0140705,0140705,&unimplemented_instr);	/* LXCB - ND110 Specific */
		Instruction_Add(0140706,0140706,&unimplemented_instr);	/* SZSB - ND110 Specific */
		Instruction_Add(0140707,0140707,&unimplemented_instr);	/* SZCB - ND110 Specific */
		break;
	default:
		Instruction_Add(0140700,0140777,&illegal_instr);	/* USER3 (microcode defined by user or illegal instruction otherwise) */
		break;
	}

	Instruction_Add(0140300,0140300,&ndfunc_setpt);			/* SETPT */
	Instruction_Add(0140301,0140301,&ndfunc_clept);			/* CLEPT */

	Instruction_Add(0140302,0140302,&unimplemented_instr);		/* CLNREENT */
	Instruction_Add(0140303,0140303,&unimplemented_instr);		/* CHREENT-PAGES */
	Instruction_Add(0140304,0140304,&unimplemented_instr);		/* CLEPU */

	Instruction_Add(0141100,0141177,&illegal_instr);		/* USER4 (microcode defined by user or illegal instruction otherwise) */
	Instruction_Add(0141200,0141277,&rmpy);				/* RMPY */
	Instruction_Add(0141300,0141377,&illegal_instr);		/* USER5 (microcode defined by user or illegal instruction otherwise) */
	Instruction_Add(0141500,0141577,&illegal_instr);		/* USER6 (microcode defined by user or illegal instruction otherwise) */
	Instruction_Add(0141600,0141677,&rdiv);				/* RDIV */
	Instruction_Add(0141700,0141777,&illegal_instr);		/* USER7 (microcode defined by user or illegal instruction otherwise) */
	Instruction_Add(0142100,0142177,&illegal_instr);		/* USER8 (microcode defined by user or illegal instruction otherwise) */
	Instruction_Add(0142200,0142277,&ndfunc_lbyt);			/* LBYT */
	Instruction_Add(0142300,0142377,&illegal_instr);		/* USER9 (microcode defined by user or illegal instruction otherwise) */
	Instruction_Add(0142500,0142577,&illegal_instr);		/* USER10 (microcode defined by user or illegal instruction otherwise) */
	Instruction_Add(0142600,0142677,&ndfunc_sbyt);			/* SBYT */
	Instruction_Add(0142700,0142777,&ndfunc_geco);			/* GECO - Undocumented instruction */
	Instruction_Add(0143100,0143177,&unimplemented_instr);		/* MOVEW */
	Instruction_Add(0143200,0143277,&ndfunc_mix3);			/* MIX3 */
	Instruction_Add(0143300,0143300,&ndfunc_ldatx);			/* LDATX */
	Instruction_Add(0143301,0143301,&ndfunc_ldxtx);			/* LDXTX */
	Instruction_Add(0143302,0143302,&ndfunc_lddtx);			/* LDDTX */
	Instruction_Add(0143303,0143303,&ndfunc_ldbtx);			/* LDBTX */
	Instruction_Add(0143304,0143304,&ndfunc_statx);			/* STATX */
	Instruction_Add(0143305,0143305,&ndfunc_stztx);			/* STZTX */
	Instruction_Add(0143306,0143306,&ndfunc_stdtx);			/* STDTX */

	switch(CurrentCPUType){
	case ND110:		/* Not implemented on ND110 due to microcode format changes */
	case ND110CE:		/* Should become a NOOOP on these */
	case ND110CX:
	case ND110PCX:
		Instruction_Add(0143500,0143500,&unimplemented_instr);	/* LWCS */
		break;
	default:
		Instruction_Add(0143500,0143500,&unimplemented_instr);	/* LWCS */
		break;
	}

	Instruction_Add(0143604,0143604,&ndfunc_ident);			/* IDENT PL10 */

	Instruction_Add(0143611,0143611,&ndfunc_ident);			/* IDENT PL11 */

	Instruction_Add(0143622,0143622,&ndfunc_ident);			/* IDENT PL12 */

	Instruction_Add(0143643,0143643,&ndfunc_ident);			/* IDENT PL13 */

	Instruction_Add(0144000,0147777,&regop);			/* --ROPS-- */
	Instruction_Add(0150000,0150077,&DoTRA);			/* TRA */
	Instruction_Add(0150100,0150177,&DoTRR);			/* TRR */
	Instruction_Add(0150200,0150277,&DoMCL);			/* MCL */
	Instruction_Add(0150300,0150377,&DoMST);			/* MST */
	Instruction_Add(0150400,0150400,&ndfunc_opcom);			/* OPCOM */
	Instruction_Add(0150401,0150401,&ndfunc_iof);			/* IOF */
	Instruction_Add(0150402,0150402,&ndfunc_ion);			/* ION */
	switch(CurrentCPUType){
	case ND110PCX:
		/* ND110 Butterfly only instruction */
		Instruction_Add(0150403,0150403,&unimplemented_instr);	/* RTNSIM (SECRE) */
		break;
	default:
		break;
	}
	Instruction_Add(0150404,0150404,&ndfunc_pof);			/* POF */
	Instruction_Add(0150405,0150405,&ndfunc_piof);			/* PIOF */
	Instruction_Add(0150406,0150406,&ndfunc_sex);			/* SEX */
	Instruction_Add(0150407,0150407,&ndfunc_rex);			/* REX */
	Instruction_Add(0150410,0150410,&ndfunc_pon);			/* PON */

	Instruction_Add(0150412,0150412,&ndfunc_pion);			/* PION */

	Instruction_Add(0150415,0150415,&ndfunc_ioxt);			/* IOXT */
	Instruction_Add(0150416,0150416,&ndfunc_exam);			/* EXAM */
	Instruction_Add(0150417,0150417,&ndfunc_depo);			/* DEPO */

	Instruction_Add(0151000,0151377,&DoWAIT);			/* WAIT */
	Instruction_Add(0151400,0151777,&ndfunc_nlz);			/* NLZ */
	Instruction_Add(0152000,0152377,&ndfunc_dnz);			/* DNZ */
 /* NOTE: These two seems to have bit req on 0-2 as well */
	Instruction_Add(0152400,0152577,&ndfunc_srb);			/* SRB */
	Instruction_Add(0152600,0152777,&ndfunc_lrb);			/* LRB */
	Instruction_Add(0153000,0153377,&ndfunc_mon);			/* MON */
	Instruction_Add(0153400,0153577,&ndfunc_irw);			/* IRW */
	Instruction_Add(0153600,0153777,&ndfunc_irr);			/* IRR */

	Instruction_Add(0154000,0157777,&ndfunc_shifts);		/* SHT, SHD, SHA, SAD */
/* NOTE: this is actually a ND1 instruction, so need to check which NDs implement it later */
	switch(CurrentCPUType){
	case ND1:
		/* ND1 only instruction */
		Instruction_Add(0160000,01163777,&ndfunc_iot);		/* IOT */
		break;
	default:
		break;
	}
	Instruction_Add(0164000,0167777,&ndfunc_iox);			/* IOX */
	Instruction_Add(0170000,0170377,&ndfunc_sab);			/* SAB */
	Instruction_Add(0170400,0170777,&ndfunc_saa);			/* SAA */
	Instruction_Add(0171000,0171377,&ndfunc_sat);			/* SAT */
	Instruction_Add(0171400,0171777,&ndfunc_sax);			/* SAX */
	Instruction_Add(0172000,0172377,&ndfunc_aab);			/* AAB */
	Instruction_Add(0172400,0172777,&ndfunc_aaa);			/* AAA */
	Instruction_Add(0173000,0173377,&ndfunc_aat);			/* AAT */
	Instruction_Add(0173400,0173777,&ndfunc_aax);			/* AAX */
	Instruction_Add(0174000,0177777,&do_bops);			/* Bit Operation Instructions */
									/* Bit operations, 16 of them, 4 BSET,4 BSKP and 8 others */
}

