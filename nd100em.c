/*
 * nd100em - ND100 Virtual Machine
 *
 * Copyright (c) 2006-2011 Roger Abrahamsson
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
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include "nd100.h"
#include "nd100em.h"

int main(int argc, char *argv[]) {
	int res;

	srand ( time(NULL) ); /* Generate PRNG Seed */

	used=calloc(1,sizeof(struct rusage)); /* Perf counter stuff */
	res=nd100emconf(); /* Load Configuration */
	if (res) exit(1); /* Configuration error */
	if (DAEMON)
		daemonize();
	blocksignals();

	if (debug) debug_open();
	if (trace) trace_open();
	if (DISASM) disasm_init();

	if (sem_init(&sem_int, 0, 1) == -1)
		exit(1);
	if (sem_init(&sem_cons, 0, 0) == -1) /* console locked, so thread waits for output at start */
		exit(1);
	if (sem_init(&sem_sigthr, 0, 0) == -1) /* signal thread locked, so it doesn't finish prematurely */
		exit(1);
	if (sem_init(&sem_rtc_tick, 0, 1) == -1) /* start with no lock. */
		exit(1);
	if (sem_init(&sem_rtc, 0, 1) == -1) /* start with no lock. */
		exit(1);
	if (sem_init(&sem_io, 0, 1) == -1) /* start with no lock. */
		exit(1);
	if (sem_init(&sem_floppy, 0, 1) == -1) /* start with lock. */
		exit(1);
	if (sem_init(&sem_mopc, 0, 1) == -1) /* start with lock. */
		exit(1);
	if (sem_init(&sem_run, 0, 0) == -1) /* start with no lock. */
		exit(1);
	if (PANEL_PROCESSOR)
		if (sem_init(&sem_pap, 0, 1) == -1) /* start with no lock. */
			exit(1);

	setup_cpu();
	program_load();

	if (PANEL_PROCESSOR)
		setup_pap();

	/* Direct input/output enabled */
	setcbreak ();
	setvbuf(stdout, NULL, _IONBF, 0);

	/* start the real machine as multiple threads */
	start_threads();
	pthread_join(gThreadChain->thread,NULL); /* TODO:: Maybe do this otherwise, we exploit that we "know" cputhread is first and will be running here */
	stop_threads();

	getrusage(RUSAGE_SELF, used);	/* Read how much resources we used */

	usertime=used->ru_utime.tv_sec+((float)used->ru_utime.tv_usec/1000000);
	systemtime=used->ru_stime.tv_sec+((float)used->ru_stime.tv_usec/1000000);
	totaltime=(float)usertime + (float)systemtime;

	unsetcbreak ();
	printf("Number of instructions run: %f, time used: %f\n",instr_counter,totaltime);
	printf("usertime: %f  systemtime: %f\n",usertime,systemtime);
	printf("Current cpu cycle time is:%f microsecs\n",(totaltime/((float)instr_counter/1000000)));

	disasm_dump();

	return(0);
}
