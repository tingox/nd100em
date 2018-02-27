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

/* GLOBAL VARS */

extern int trace;
extern FILE *tracefile;
extern int debug;
extern FILE *debugfile;

extern char *regn[];
extern unsigned short bank;
extern unsigned short MON_RUN;
extern struct CpuRegs *gReg;

extern double instr_counter;


extern int getch (void);
extern char mygetc (void);


/* define existant mon call functions here */
void mon_0();
void mon_1();
void mon_2();
void mon_64();
void mon_notyet();



/* Array of pointers to mon call functions */

void (*monarr[])() = {
	&mon_0,&mon_1,&mon_2,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,			/* MON 0-7     */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 10-17   */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 20-27   */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 30-37   */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 40-47   */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 50-57   */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_64,&mon_notyet,&mon_notyet,&mon_notyet,		/* MON 60-67   */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 70-77   */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 100-107 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 110-117 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 120-127 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 130-137 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 140-147 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 150-157 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 160-167 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 170-177 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 200-207 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 210-217 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 220-227 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 230-237 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 240-247 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 250-257 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 260-267 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 270-277 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 300-307 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 310-317 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 320-327 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 330-337 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 340-347 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 350-357 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,	/* MON 360-367 */
	&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet,&mon_notyet 	/* MON 370-377 */
};

/* then call one */
/* monarr[5](); */
