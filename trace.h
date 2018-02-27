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

char tracename[]="tracefile.log";
char tracetype[]="a";
FILE *tracefile;
int trace;


char trace2name[]="trace2file.log";
char trace2type[]="a";
FILE *trace2file;

char disasm_fname[]="nd100em.disasm.log";
char disasm_ftype[]="a";
FILE *disasm_file;


int DISASM;
int disasm_ctr;

struct disasm_entry {
	bool isdata;
	bool iscode;
	int labelno;
	bool use_rel;
	int rel_acc_lbl;
	char asm_str[32];
	bool isexr;
	char exr[32];
	ushort theword;
};
struct disasm_entry *disasm_arr[65536];
struct disasm_entry *(*p_DIS)[] = &disasm_arr;

volatile int ts_counter = 0;
volatile int ts_step = 0;
#define MAXTSARR 32
#define MAXTSSTR 256
char ts_block[MAXTSARR][MAXTSSTR];

extern char *regn[];
extern double instr_counter;
extern struct CpuRegs *gReg;

extern void OpToStr(char *opstr, ushort operand);
extern ushort extract_opcode(ushort instr);

void trace_instr(ushort instr);
void disasm_addword(ushort addr, ushort myword);
void disasm_init();
void disasm_dump();
void disasm_instr(ushort addr, ushort instr);
void disasm_exr(ushort addr, ushort instr);
void disasm_setlbl(ushort addr);
void disasm_userel(ushort addr, ushort where);
void disasm_set_isdata(ushort addr);

