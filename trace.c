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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <termios.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include "nd100.h"
#include "trace.h"

/*
 * Routine for tracing steps in instructions, this one for before doing instruction
 * here we create strings with register=value for the compressed SQL output.
 * IN: number of value pairs, value pair, value pair, .... (regstring,value)
 * NOTE: Only does this when trace bit 5 is set, as thats when we do that kind of tracing.
 */
void trace_pre(int num,...){
	int i;
	char * tmps;
	int tmpi;
	va_list ap;
	va_start(ap, num);
	if (trace & 0x20) {
		for (i=1;i<=num;i++){
			tmps=va_arg(ap, char *);
			tmpi=va_arg(ap, int);
			(void)snprintf(ts_block[ts_counter],MAXTSSTR,
				"#s (i,s,w) #v# (\"%d\",\"0\",\"%s=%06o\");\n",(int)instr_counter,tmps,tmpi);
			ts_counter++;
		}
	}
	va_end(ap);
}

/*
 * Routine for tracing steps in instructions, this one during instruction execution.
 * here we create strings for the compressed SQL output.
 * IN: number of value pairs, value pair, value pair, .... (formatstring,value)
 * NOTE: Only does this when trace bit 5 is set, as thats when we do that kind of tracing.
 */
void trace_step(int num,...){
	int i;
	char * tmps;
	int tmpi;
	char tmps2[256];
	va_list ap;
	va_start(ap, num);
	if (trace & 0x20) {
		for (i=1;i<=num;i++){
			tmps=va_arg(ap, char *);
			tmpi=va_arg(ap, int);
			(void)snprintf(tmps2,256,tmps,tmpi);
			ts_step++;
			(void)snprintf(ts_block[ts_counter],MAXTSSTR,
				"#s (i,s,w) #v# (\"%d\",\"%d\",\"%s\");\n",(int)instr_counter,ts_step,tmps2);
			ts_counter++;
		}
	}
	va_end(ap);
}

/*
 * Routine for tracing steps in instructions, this one for after doing instruction
 * here we create strings with register=value for the compressed SQL output.
 * IN: number of value pairs, value pair, value pair, .... (regstring,value)
 * NOTE: Only does this when trace bit 5 is set, as thats when we do that kind of tracing.
 */
void trace_post(int num,...){
	int i;
	char * tmps;
	int tmpi;
	va_list ap;
	va_start(ap, num);
	if (trace & 0x20) {
		for (i=1;i<=num;i++){
			tmps=va_arg(ap, char *);
			tmpi=va_arg(ap, int);
			(void)snprintf(ts_block[ts_counter],MAXTSSTR,
				"#s (i,s,w) #v# (\"%d\",\"100\",\"%s=%06o\");\n",(int)instr_counter,tmps,tmpi);
			ts_counter++;
		}
	}
	va_end(ap);
}

void trace_exr(ushort instr){
	char disasm_str[256];
	if (trace & 0x01) {
		OpToStr(disasm_str,instr);
		fprintf(tracefile,"#e (i,d) #v# (\"%d\",\"%s\");\n",
			(int)instr_counter,disasm_str);
	}
}

void trace_instr(ushort instr){
	ushort opcode;
	char disasm_str[256];
	char disasm_str2[256];

	extract_opcode(instr);

	if(trace & 0x01) {
		OpToStr(disasm_str,instr);
		fprintf(tracefile,"#c (i,l,a,d,c) #v# (\"%d\",\"%d\",\"%06o\",\"%06o\",\"%s\");\n",
			(int)instr_counter,CurrLEVEL,gPC,instr,disasm_str);

		if (opcode == 0140600 ){ /* EXR */
			OpToStr(disasm_str,instr);
			fprintf(trace2file,"| %08d | %02d | %06o | %06o | %s |\n",
				(int)instr_counter,CurrLEVEL,gPC,instr,disasm_str);
		} else
			fprintf(trace2file,"| %08d | %02d | %06o | %06o | %s |\n",
				(int)instr_counter,CurrLEVEL,gPC,instr,disasm_str);
	}
}

void trace_regs() {
	int i,j;
	j=CurrLEVEL;
	if (trace & 0x02){
		for(i=0; i<=7; i++) {
			fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",\"%d\",\"%s\",\"%06o\");\n",(int)instr_counter,j,regn[i],gReg->reg[j][i]);
		}
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",\"%d\",\"PCR\",\"%06o\");\n",(int)instr_counter,j,gReg->reg_PCR[j]);
	}
	if(trace & 0x10) {
		for(j=0;j<=15;j++) {
			if  (!((trace & 0x02) && (j == CurrLEVEL))) {
				for(i=0; i<=7; i++) {
					fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",\"%d\",\"%s\",\"%06o\");\n",(int)instr_counter,j,regn[i],gReg->reg[j][i]);
				}
				fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",\"%d\",\"PCR\",\"%06o\");\n",(int)instr_counter,j,gReg->reg_PCR[j]);
			}
		}
	}
	if(trace & 0x04) {
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"CurrLEVEL\",\"%06o\");\n",(int)instr_counter,CurrLEVEL);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"PANC\",\"%06o\");\n",(int)instr_counter,gPANC);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"PANS\",\"%06o\");\n",(int)instr_counter,gPANS);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"OPR\",\"%06o\");\n",(int)instr_counter,gOPR);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"LMP\",\"%06o\");\n",(int)instr_counter,gLMP);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"PGS\",\"%06o\");\n",(int)instr_counter,gPGS);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"PVL\",\"%06o\");\n",(int)instr_counter,gPVL);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"IIC\",\"%06o\");\n",(int)instr_counter,gIIC);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"IID\",\"%06o\");\n",(int)instr_counter,gIID);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"IIE\",\"%06o\");\n",(int)instr_counter,gIIE);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"PID\",\"%06o\");\n",(int)instr_counter,gPID);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"PIE\",\"%06o\");\n",(int)instr_counter,gPIE);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"CSR\",\"%06o\");\n",(int)instr_counter,gCSR);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"CCL\",\"%06o\");\n",(int)instr_counter,gCCL);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"ACTL\",\"%06o\");\n",(int)instr_counter,gACTL);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"LCIL\",\"%06o\");\n",(int)instr_counter,gLCIL);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"ALD\",\"%06o\");\n",(int)instr_counter,gALD);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"UCIL\",\"%06o\");\n",(int)instr_counter,gUCIL);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"PES\",\"%06o\");\n",(int)instr_counter,gPES);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"PGC\",\"%06o\");\n",(int)instr_counter,gPGC);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"PEA\",\"%06o\");\n",(int)instr_counter,gPEA);
		fprintf(tracefile,"#r (i,l,r,v) #v# (\"%d\",NULL,\"ECCR\",\"%06o\");\n",(int)instr_counter,gECCR);
	}
}

void trace_flush(){
	if (trace & 0x20) {
		while (ts_counter > 0) {
			ts_counter--;
			fprintf(tracefile,"%s",ts_block[ts_counter]);
		}
		ts_step = 0;
		ts_counter = 0;
	}
}

int trace_open(){
	tracefile=fopen(tracename,tracetype);
	fprintf(tracefile,"USE nd100em;\n");

	trace2file=fopen(trace2name,trace2type);

	return(1);
}


int trace2_open(){
	trace2file=fopen(trace2name,trace2type);
	return(1);
}

void disasm_instr(ushort addr, ushort instr){
	char disasm_str[32];
	OpToStr(disasm_str,instr);
	if ((*p_DIS)[addr] != NULL) {
		(*p_DIS)[addr]->iscode = true;
		snprintf((*p_DIS)[addr]->asm_str,32,"%s",disasm_str);
	}
}

void disasm_exr(ushort addr, ushort instr){
	char disasm_str[32];
	OpToStr(disasm_str,instr);
	if ((*p_DIS)[addr] != NULL) {
		(*p_DIS)[addr]->isexr = true;
		snprintf((*p_DIS)[addr]->exr,32,"%s",disasm_str);
	}
}

void disasm_addword(ushort addr, ushort myword){
	(*p_DIS)[addr] = calloc(1,sizeof(struct disasm_entry));
	if ((*p_DIS)[addr] != NULL) {
		(*p_DIS)[addr]->theword = myword;
	}
}

void disasm_init(){
	int i;
	for(i=0;i<65536;i++){
		(*p_DIS)[i]= NULL;
	}
	disasm_ctr=0;
}

void disasm_setlbl(ushort addr){
	disasm_ctr++;
	if ((*p_DIS)[addr] != NULL) {
		(*p_DIS)[addr]->labelno = disasm_ctr;
	}
}

void disasm_set_isdata(ushort addr){
	if ((*p_DIS)[addr] != NULL) {
		(*p_DIS)[addr]->isdata = true;
	}
}

void disasm_userel(ushort addr, ushort where){
	if ((*p_DIS)[addr] != NULL) {
		if ((*p_DIS)[addr]->use_rel) { /* we have already used relative from here */
		} else {
			(*p_DIS)[addr]->use_rel = true;
			if ((*p_DIS)[where]->labelno) { /* Where already has a label  */
				(*p_DIS)[addr]->rel_acc_lbl = (*p_DIS)[where]->labelno;
			} else {
				disasm_setlbl(where);
				(*p_DIS)[addr]->rel_acc_lbl = (*p_DIS)[where]->labelno;
			}
		}
	}
}

void disasm_dump(){
	int i;
	int tmp;
	char u,l;
	ushort w;
	char disasm_str[32];

	disasm_file=fopen(disasm_fname,disasm_ftype);

	for(i=0;i<65536;i++){
		if ((*p_DIS)[i] != NULL) {
			w = (*p_DIS)[i]->theword;
			u = (w >> 8) & 0xff;
			l = w & 0xff;

			fprintf(disasm_file,"%06o    %06o   ",i,(*p_DIS)[i]->theword);
			if ((*p_DIS)[i]->labelno)
				fprintf(disasm_file," L%05d ",(*p_DIS)[i]->labelno);
			else
				fprintf(disasm_file,"       ");
			if ((*p_DIS)[i]->iscode) {
				fprintf(disasm_file,"%s",(*p_DIS)[i]->asm_str);
				tmp=strlen((const char*)(*p_DIS)[i]->asm_str);
				fprintf(disasm_file,"%.*s", (32-tmp), "                                 "); /* align */
				if ((*p_DIS)[i]->use_rel)
					fprintf(disasm_file,"%% L%05d ",(*p_DIS)[i]->rel_acc_lbl);
				if ((*p_DIS)[i]->isexr)
					fprintf(disasm_file,"%% %s",(*p_DIS)[i]->exr);
			} else if ((*p_DIS)[i]->isdata) {
				fprintf(disasm_file,"DATA: ");
				if (u>=32 & u<=127)
					fprintf(disasm_file,"\'%c\'",u);
				if (l>=32 & l<=127)
					fprintf(disasm_file,"\'%c\'",l);
			} else {
				fprintf(disasm_file,"UNKN: ");
				if (u>=32 & u<=127)
					fprintf(disasm_file,"\'%c\'",u);
				if (l>=32 & l<=127)
					fprintf(disasm_file,"\'%c\'",l);

				fprintf(disasm_file,"          ");
				OpToStr(disasm_str,(*p_DIS)[i]->theword);
				fprintf(disasm_file,"%% %s",disasm_str);
			}
			fprintf(disasm_file,"\n");
		}
	}

	fclose(disasm_file);
}

//void main (int argc, char *argv[]){
//	instr_counter = 1000; // TEMP, REMOVE
//	if (trace) trace_open;
//	trace_pre(2,"UCIL",200,"PC",201);
/* Example strings we need to be able to process:	(IO:%06o)<=A	A<=(IO:%06o)	PANC<=A		S:(%06o)-	-E:(%06o)<=A	*/
//	trace_step(4,"(IO:%06o)<=A",200,"PANC<=A",0,"S:(%06o)-",10,"-E:(%06o)<=A",20);
//}

