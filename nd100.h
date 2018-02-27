/*
 * nd100em - ND100 Virtual Machine
 *
 * Copyright (c) 2006 Per-Olof Astrom
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

/* A complete listing of registers in a program level regbank including the 8 scratch regs. */
#define _STS 0
#define _D 1
#define _P 2
#define _B 3
#define _L 4
#define _A 5
#define _T 6
#define _X 7
#define _U0 8
#define _U1 9
#define _U2 10
#define _U3 11
#define _U4 12
#define _U5 13
#define _U6 14
#define _U7 15

/* A complete listing of privileged system registers */
/* Since some of them have the same "number" but are different */
/* Or even are part of some other register or take in bits from */
/* external sources, special care need to be taken when handling these */

#define PANS 0 /* Read */
#define PANC 0 /* Write */
#define STS 1 /* Read and write, but spread out over 16 levels too in the register file */
#define OPR 2 /* Read */
#define LMP 2 /* Write */
#define PGS 3 /* Read */
#define PCR 3 /* Write */
#define PVL 4 /* Read */
#define IIC 5 /* Read */
#define IIE 5 /* Write */
#define PID 6 /* Read and write */
#define PIE 7 /* Read and write */
#define CSR 8 /* Read */
#define CCL 8 /* Write */
#define ACTL 9 /* Read */
#define LCIL 9 /* Write */
#define ALD 10 /* Read */
#define UCIL 10 /* Write */
#define PES 11 /* Read */
#define PGC 12 /* Read */
#define PEA 13 /* Read */
#define ECCR 13 /* Write */

/*
 * PANS
 *
 * | 15 | 14 | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |
 * +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
 * |DISP|INP |RPAN|PAN |  0 |    PFUNC     |             RPAN                 |
 * |PRES|PDY |VAL |INT |    |              |                                  |
 * +----+----+----+----+----+--------------+----+----+----+----+----+----+----+
 *
 */

/*
 * PANC
 *
 * | 15 | 14 | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |
 * +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
 * |  0 |  0 |READ|N.A.|  0 |    PFUNC     |             WPAN                 |
 * |    |    | RQ |    |    |              |                                  |
 * +----+----+----+----+----+--------------+----+----+----+----+----+----+----+
 *
 */


/* Status register flags */

#define _PTM 0
#define _TG 1
#define _K 2
#define _Z 3
#define _Q 4
#define _O 5
#define _C 6
#define _M 7
#define _PL 8
#define _N100 12
#define _SEXI 13
#define _PONI 14
#define _IONI 15


typedef unsigned short int ushort;
typedef signed short int sshort;
typedef unsigned long int ulong;
typedef signed long int slong;

#define PAGINGSYSTEM 0
#define OPERATORSPANEL 0

/*************************************************/
/* NEW ORGANIZATION OF MEMORY AND REGISTERS!!    */
/*************************************************/

/* Lets use the full 16MWord space now (32MB ram in host)*/
#define MEMPTSIZE 16384

/* Volatile Memory
 * Fixed to MEMPTSIZE KWords for now.
 */
typedef union ndram {
	unsigned char	c_Array[MEMPTSIZE*1024*2];
	ushort		n_Array[MEMPTSIZE*1024];
	ushort		n_Pages[MEMPTSIZE][1024];
} _NDRAM_ ;

/* Paging tables(Shadow memory) */
typedef union ndpt {
	ushort	word_array[4*64*2];
	ushort	normal[4][64];
	ulong	extended[4][64];
} _NDPT_ ;

/* Paging tables(Shadow memory) */
union NewPT {
	ulong	pt_arr[4*64];
	ulong	pt[4][64];
};

struct CpuRegs {
	ushort	reg[16][16];	/* main CPU registers for all runlevels */
	ushort	reg_PANS;	/* */
	ushort	reg_PANC;	/* */
	ushort	reg_OPR;	/* */
	ushort	reg_LMP;	/* */
	ushort	reg_PGS;	/* */
	ushort	reg_PCR[16];	/* Paging Control Registers */
	ushort	reg_PVL;	/* */
	ushort	reg_IIC;	/* IIC is actually just a priority encoded (IID | IIE) */
	ushort	reg_IID;	/* Actual interrupt reg */
	ushort	reg_IIE;	/* */
	ushort	reg_PID;	/* */
	ushort	reg_PIE;	/* */
	ushort	reg_CSR;	/* */
	ushort	reg_CCL;	/* */
	ushort	reg_ACTL;	/* active runlevel for this cpu */
				/* NOTE:: Not sure if this is stored as a bitfield or not.. CHECK!!!! */
				/* For now we just use it as a normal value */
	ushort	reg_LCIL;	/* */
	ushort	reg_ALD;	/* */
	ushort	reg_UCIL;	/* */
	ushort	reg_PES;	/* */
	ushort	reg_PGC;	/* */
	ushort	reg_PEA;	/* */
	ushort	reg_ECCR;	/* */

	/* Personally Added to do Prefetch and Instruction more alike ND */
	ushort	myreg_IR;	/* InstructionRegister */
	ushort	myreg_PFB;	/* PrefetchBuffer */

	/* "locks" for registers that according to manual works that way (PES, PGS, IIC) */
	/* 1 = "locked" */
	/* :TODO: Check if PEA and PES should have a common lock */
	bool	mylock_PEA;
	bool	mylock_PES;
	bool	mylock_PGS;
	bool	mylock_IIC;

	/* taking a shortcut by creating a PK 4bit register */
	/* always modify this as well when touching PID or PIE */
	ushort	myreg_PK;

	/* For MOPC/OPCOM tracing and breakpoint functionality */
	/* counter for semirun mode*/
	bool	has_instr_cntr;
	ushort	instructioncounter;
	/* flag for breakpoint and breakpoint address */
	bool	has_breakpoint;
	ushort	breakpoint;
};

/*
 * A structure to trace all memoryaccesses for an instruction to be able to debug better.
 * Works as a chained list, and should be built up during an instruction, and destroyed after.
 * That way we can print all memoryaccesses to the instruction they belong even with EXRs.
 * We can add more functionality later if we want to debug more aspects of the memory accesses.
 */
struct MemTraceList {
	unsigned int addr;
	char funct; /* W=Write, R=Read, F=Fetch */
	struct MemTraceList *next;
};

typedef enum {IGNORE, CANCEL, JOIN} _THREAD_KILL_MODE_;

/*
 * Structure to keep track of all running threads in the emulator, and how they should be terminated.
 * A doubly linked one so we can unchain an element in the middle quite easily as well as traverse it back and forth.
 */
struct ThreadChain {
	_THREAD_KILL_MODE_ tk;	/* how this thread should be terminated by shutdown routine */
        pthread_t thread;	/* thread id (normally unsigned long int)*/
        pthread_attr_t tattr;	/* thread attributes */
	struct ThreadChain *prev, *next; /* the links */
};

/*
 * Structure for identifying interrupting device. To simplify the detection of which interrupting
 * device caused it, let us have a linked list of all pending interrupts in the system. A doubly linked
 * one so we can unchain an element in the middle quite easily as well as traverse it back and forth.
 * Use one list for now, but we can both split it into several as well as keep it. Also have a for now
 * unused variable called slotnum, which can be used later if we want to mimic ND100 ordering behaviour closer.
 */
struct IdentChain {
	char level; /* interrupt level this is. Might not be needed, if we use one list per level */
	ushort slotnum; /* which slot in the rack the device sits at. unused for now. */
	ushort identcode; /* which interrupting device it is. */
	int callerid;	/* We create this as a unique id, as there are identids that are same for several devices? */
	struct IdentChain *prev, *next; /* the links */
};

typedef enum {SHUTDOWN, STOP, SEMIRUN, RUN} _RUNMODE_;

typedef enum {ND1, ND4, ND10, ND100, ND100CE, ND100CX, ND110, ND110CE, ND110CX, ND110PCX} _CPUTYPE_;

#define gPC	gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_P]
#define gA	gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_A]
#define gT	gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_T]
#define gB	gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_B]
#define gD	gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_D]
#define gX	gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_X]
#define gL	gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_L]

#define gPANC	gReg->reg_PANC
#define gPANS	gReg->reg_PANS
#define gOPR	gReg->reg_OPR
#define gLMP	gReg->reg_LMP
#define gPGS	gReg->reg_PGS
#define gPVL	gReg->reg_PVL
#define gIIC	gReg->reg_IIC
#define gIID	gReg->reg_IID
#define gIIE	gReg->reg_IIE
#define gPID	gReg->reg_PID
#define gPIE	gReg->reg_PIE
#define gCSR	gReg->reg_CSR
#define gCCL	gReg->reg_CCL
#define gACTL	gReg->reg_ACTL
#define gLCIL	gReg->reg_LCIL
#define gALD	gReg->reg_ALD
#define gUCIL	gReg->reg_UCIL
#define gPES	gReg->reg_PES
#define gPGC	gReg->reg_PGC
#define gPEA	gReg->reg_PEA
#define gECCR	gReg->reg_ECCR

/* Use lvl0 as default to start with, and then just always(!!!) set all levels when setting STS MSB flags */
/* so by default we use reg[0][_STS] as MSB STS */
#define CurrLEVEL	((gReg->reg[0][_STS] & 0x0f00) >>8)
#define gPIL		((gReg->reg[0][_STS] & 0x0f00) >>8)

/* Highest runlevel with PIE AND PID bits both set */
#define gPK		gReg->myreg_PK

/* The complete Status register both MSB and LSB for current runlevel. Read only MACRO */
#define gSTSr		((gReg->reg[0][_STS] & 0xFF00) | (gReg->reg[(gReg->reg[][_STS] & 0x0f00) >>8][_STS] & 0x00FF))

#define InstructionRegister	gReg->myreg_IR
#define PrefetchBuffer		gReg->myreg_PFB

#define STS_PTM  ((gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_STS] & 0x0001)>>0)	/* */
#define STS_TG   ((gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_STS] & 0x0002)>>1)	/* */
#define STS_K    ((gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_STS] & 0x0004)>>2)	/* */
#define STS_Z    ((gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_STS] & 0x0008)>>3)	/* */
#define STS_Q    ((gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_STS] & 0x0010)>>4)	/* */
#define STS_O    ((gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_STS] & 0x0020)>>5)	/* */
#define STS_C    ((gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_STS] & 0x0040)>>6)	/* */
#define STS_M    ((gReg->reg[((gReg->reg[0][_STS] & 0x0f00) >>8)][_STS] & 0x0080)>>7)	/* */
#define STS_PL   ((gReg->reg[0][_STS] & 0x0f00) >>8)					/* Program runlevel */
#define STS_N100 ((gReg->reg[0][_STS] & 0x1000) >>12)					/* Nord 100 indicator */
#define STS_SEXI ((gReg->reg[0][_STS] & 0x2000) >>13)					/* Extended MMS adressing on/off indicator (24 bit instead of 19 bit*/
#define STS_PONI ((gReg->reg[0][_STS] & 0x4000) >>14)					/* Memory management on/off indicator */
#define STS_IONI ((gReg->reg[0][_STS] & 0x8000) >>15)					/* Interrupt system on/off indicator */

#define UNDEF_INSTR ((ushort)0142500)

/*
 */
struct control_panel {
	int lock_key;
//	bool stop_button;
//	bool load_button;
//	bool opcom_button;
//	bool mcl_button;

	bool power_lamp;
	bool run_lamp;
	bool opcom_lamp;
};

struct display_panel {
//	bool opcom_button;

	bool trr_panc;	/* TRR has been issued, process command */
	bool sec_tick;	/* Seconds tick from rtc, update counters */

	ushort pap_curr_command;

	char func_display[40];	/* Max 40 chars in display buffer */
	ushort fdisp_cntr;	/* pointer to where we are */

	ushort seconds;		/* 16 bit second counter for realtime clock, ticks day counter every 12h */
	ushort days;		/* 16 bit day counter for realtime clock */
				/* So seconds should wrap at 3600x12 = 43200, and day possibly lowest bit is am/pm */

	bool power_lamp;
	bool run_lamp;
	bool opcom_lamp;
	int function_util;
	int function_hit;
	int function_ring;
	int function_mode;
};

