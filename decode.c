/*
 * nd100em - ND100 Virtual Machine
 *
 * Copyright (c) 2008 Roger Abrahamsson
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

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include "nd100.h"

ushort decode_140k(ushort instr);
ushort decode_150k(ushort instr);
ushort extract_opcode(ushort instr);
extern int CurrentCPUType;

/* 
 * Mask out instruction opcode from parameters
 * NOTE: Manual does not specify details about how ND100
 * maps all bits and some opcodes will miss this decode.
 * This means we will not completely emulate ND100 behaviour for
 * illegal opcodes yet.
 */
ushort extract_opcode(ushort instr) {
	switch(instr & (0xFFFF<<11)) {
	case 0130000:			/* JAP, JAN, JAZ, JAF, JPC, JNC, JXZ, JXN */
		return(instr & (0xFFFF<<8));
	case 0140000:	
		if(0 == (instr & (0x03<<6)))	/* SKIP Instruction */
			return 0140000;
		else 				/* Decode further */
			return(decode_140k(instr));
	case 0150000:				/* Decode further */
			return(decode_150k(instr));
	case 0144000:				/* ROP Register operations */
		return instr & (0xFFFF<<6);
	case 0154000:				/* Shift Instruction */
		return instr & ((0xFFFF<<11) | (0x03<<7));
	case	0170000:		/* Argument Instructions */
		return instr & (0xFFFF<<8);
	case	0174000:		/* Bit Operation Instructions */
		return instr & (0xFFFF<<7); /* Bit operations, 16 of them, 4 BSET,4 BSKP and 8 others */
	default:				/* Memory Reference Instructions & IOX */
		return instr & (0xFFFF<<11);
	}
	return instr;	//:NOTE: This should not be reached. Added only to satisfy complaining compiler.
}

ushort decode_140k(ushort instr) {
	switch (instr & (0xFFFF)) {
	case 0140120: /* ADDD */
	case 0140121: /* SUBD */
	case 0140122: /* COMD */
	case 0140123: /* TSET */
	case 0140124: /* PACK */
	case 0140125: /* UPACK */
	case 0140126: /* SHDE */
	case 0140127: /* RDUS */
	case 0140130: /* BFILL */
	case 0140131: /* MOVB */
	case 0140132: /* MOVBF */
		return(instr);
	case 0140133: /* VERSN - ND110 specific */
		if ((CurrentCPUType == ND110) || (CurrentCPUType == ND110CE) || (CurrentCPUType == ND110CX))
			return(instr);
		else
			break;
	case 0140134: /* INIT */
	case 0140135: /* ENTR */
	case 0140136: /* LEAVE */
	case 0140137: /* ELEAV */
	case 0140300: /* SETPT */
	case 0140301: /* CLEPT */
	case 0140302: /* CLNREENT */
	case 0140303: /* CHREENT-PAGES */
	case 0140304: /* CLEPU */
	case 0143500: /* LWCS */
	case 0143604: /* IDENT PL10 */
	case 0143611: /* IDENT PL11 */
	case 0143622: /* IDENT PL12 */
	case 0143643: /* IDENT PL13 */
		return(instr);
	default:
		break;
        }
	switch (instr & (0xFFFF<<6)) {
	case 0140200: /* USER1 (microcode defined by user or illegal instruction otherwise) */
		return instr & (0xFFFF<<6);	
	case 0140500: /* USER2 (microcode defined by user or illegal instruction otherwise) */
		if ((CurrentCPUType == ND100) || (CurrentCPUType == ND100CE) || (CurrentCPUType == ND100CX)) /* We are ND100 */
			return instr & (0xFFFF<<6);	
		else switch (instr & (0xFFFF)) {							/* We are not */
			case 0140500: /* WGLOB - ND110 Specific */
			case 0140501: /* RGLOB - ND110 Specific */
			case 0140502: /* INSPL - ND110 Specific */
			case 0140503: /* REMPL - ND110 Specific */
			case 0140504: /* CNREK - ND110 Specific */
			case 0140505: /* CLPT  - ND110 Specific */
			case 0140506: /* ENPT  - ND110 Specific */
			case 0140507: /* REPT  - ND110 Specific */
			case 0140510: /* LBIT  - ND110 Specific */
			case 0140513: /* SBITP - ND110 Specific */
			case 0140514: /* LBYTP - ND110 Specific */
			case 0140515: /* SBYTP - ND110 Specific */
			case 0140516: /* TSETP - ND110 Specific */
			case 0140517: /* RDUSP - ND110 Specific */
				return(instr);
			default:
				break;
		}
	case 0140600: /* EXR */
		return instr & (0xFFFF<<6);	
	case 0140700: /* USER3 (microcode defined by user or illegal instruction otherwise) */
		if ((CurrentCPUType == ND100) || (CurrentCPUType == ND100CE) || (CurrentCPUType == ND100CX)) /* We are ND100 */
			return instr & (0xFFFF<<6);	
		else switch (instr & (0xFFC7)) {							/* We are not */
			case 0140700: /* LASB - ND110 Specific */
			case 0140701: /* SASB - ND110 Specific */
			case 0140702: /* LACB - ND110 Specific */
			case 0140703: /* SASB - ND110 Specific */
			case 0140704: /* LXSB - ND110 Specific */
			case 0140705: /* LXCB - ND110 Specific */
			case 0140706: /* SZSB - ND110 Specific */
			case 0140707: /* SZCB - ND110 Specific */
				return(instr & (0xFFC7));
			default:
				break;
		}
	case 0141100: /* USER4 (microcode defined by user or illegal instruction otherwise) */
	case 0141200: /* RMPY */
	case 0141300: /* USER5 (microcode defined by user or illegal instruction otherwise) */
	case 0141500: /* USER6 (microcode defined by user or illegal instruction otherwise) */
	case 0141600: /* RDIV */
	case 0141700: /* USER7 (microcode defined by user or illegal instruction otherwise) */
	case 0142100: /* USER8 (microcode defined by user or illegal instruction otherwise) */
	case 0142200: /* LBYT */
	case 0142300: /* USER9 (microcode defined by user or illegal instruction otherwise) */
	case 0142500: /* USER10 (microcode defined by user or illegal instruction otherwise) */
	case 0142600: /* SBYT */
	case 0142700: /* GECO - Undocumented instruction */
	case 0143100: /* MOVEW */
	case 0143200: /* MIX3 */
		return instr & (0xFFFF<<6);	
	default:
		break;
	}
	switch (instr & (0xFFC7)) {
	case 0143300: /* LDATX */
	case 0143301: /* LDXTX */
	case 0143302: /* LDDTX */
	case 0143303: /* LDBTX */
	case 0143304: /* STATX */
	case 0143305: /* STZTX */
	case 0143306: /* STDTX */
		return (instr & (0xFFC7));
	default:
		break;
	}
	return instr;	/* NOTE: This should not be reached, but decoding might not be totally exhaustive.*/
	/* TODO: Check if we should return NOOP, or create our own internal illegal instruction code and trap that later. */
}

ushort decode_150k(ushort instr) {
	switch (instr & (0xFFFF)) {
	case 0150400 : /* OPCOM */
	case 0150401 : /* IOF */
	case 0150402 : /* ION */
	case 0150404 : /* POF */
	case 0150405 : /* PIOF */
	case 0150406 : /* SEX */
	case 0150407 : /* REX */
	case 0150410 : /* PON */
	case 0150412 : /* PION */
	case 0150415 : /* IOXT */
	case 0150416 : /* EXAM */
	case 0150417 : /* DEPO */
		return (instr);	
	default:
		break;
	}
	switch (instr & (0xFFFF<<8)) {
	case 0151000 : /* WAIT */
	case 0151400 : /* NLZ*/
	case 0152000 : /* DNZ*/
	case 0153000 : /* MON */
		return (ushort)(instr & (0xFFFF<<8));	
	default:
		break;
	}
	switch (instr & (0xFFFF<<7)) {
	case 0152400 : /* SRB */ /* NOTE: These two seems to have bit req on 0-2 as well */
	case 0152600 : /* LRB */
	case 0153400 : /* IRW */
	case 0153600 : /* IRR */
		return instr & (0xFFFF<<7);	
	default:
		break;
	}

	switch (instr & (0xFFFF<<6)) {
	case 0150000 : /* TRA */
	case 0150100 : /* TRR */
	case 0150200 : /* MCL */
	case 0150300 : /* MST */
		return instr & (0xFFFF<<6);	
	default:
		break;
	}
	return instr;	/* NOTE: This should not be reached, but decoding might not be totally exhaustive.*/
	/* TODO: Check if we should return NOOP, or create our own internal illegal instruction code and trap that later. */
}
