/*
 * nd100em - ND100 Virtual Machine
 *
 * Copyright (c) 2006-20011 Roger Abrahamsson
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

#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "nd100.h"
#include "rtc.h"

/*
 * rtc_20: a thread that sets the interrupt flag on lvl13 every 20 ms.
 * This function runs as a continuous program basically.
 * Uses posix timers and signals.
 */
void rtc_20(){
	int s;
	int rc;
	struct itimerval times;
	bool irq_en;
	int our_rnd_id;
	int cntr_20ms;

	if (debug) fprintf(debugfile,"(##)rtc_20 started...\n");

	our_rnd_id=rand(); /* our unique identifier for not generating multiple idents on the same device, TODO: Check if needed*/

	sys_rtc=calloc(1,sizeof(struct rtc_data));

	/*
	 * set up interval timer, starting in 20ms,
	 *	then every 20 ms.
	 */
	times.it_value.tv_sec = 0;
	times.it_value.tv_usec = 20000;
	times.it_interval.tv_sec = 0;
	times.it_interval.tv_usec = 20000;
	rc = setitimer (ITIMER_REAL, &times, NULL);
	if (rc==-1) {
		if (debug) fprintf(debugfile,"ERROR!!! setitimer failure rtc_20. errno=%d\n",errno);
	}

	while(CurrentCPURunMode != SHUTDOWN) {
		while ((s = sem_wait(&sem_rtc)) == -1 && errno == EINTR) /* wait for rtc synch lock to be free */
			continue;       /* Restart if interrupted by handler */
		irq_en = sys_rtc->irq_en;
		sys_rtc->rdy = 1;

		if (sys_rtc->cntr_20ms >= 49) {	/* Think this is right.. :) 20ms x 50 = 1 sec */
			sys_rtc->cntr_20ms = 0;

		} else
			sys_rtc->cntr_20ms++;
		cntr_20ms = sys_rtc->cntr_20ms;

		if (sem_post(&sem_rtc) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure rtc_20\n");
			CurrentCPURunMode = SHUTDOWN;
		}

		if(PANEL_PROCESSOR) {	/* OK here we should "tick" the panel processor?? */
// TODO: This should most likely be changed to use a separate posix timer and signal thread etc. SIGUSR1 maybe
/*TODO: tick panel second counter, also check if this is the right way, since we can "reset" the rtc 20ms timer */
			if (cntr_20ms == 0){
				gPAP->sec_tick = true;
				if (sem_post(&sem_pap) == -1) { /* 'kick' panel processor thread */
					if (debug) fprintf(debugfile,"ERROR!!! sem_post failure rtc_20\n");
					CurrentCPURunMode = SHUTDOWN;
				}
			}
		}

		if (irq_en) {
			if(CurrentCPURunMode != STOP) {
				while ((s = sem_wait(&sem_int)) == -1 && errno == EINTR) /* wait for interrupt lock to be free */
					continue;       /* Restart if interrupted by handler */
				gPID |= 0x2000; /* Bit 13 */
				AddIdentChain(13,1,our_rnd_id); /* Add interrupt to ident chain, lvl13, ident code 1, and identify us */
				if (sem_post(&sem_int) == -1) { /* release interrupt lock */
					if (debug) fprintf(debugfile,"ERROR!!! sem_post failure DOMCL\n");
					CurrentCPURunMode = SHUTDOWN;
				}
				checkPK();
			}
//			if(!PANEL_PROCESSOR) /* No panel processor available, trigger mopc here */
				if (MODE_OPCOM) {
					if (sem_post(&sem_mopc) == -1) { /* release mopc lock */
						if (debug) fprintf(debugfile,"ERROR!!! sem_post failure rtc_20\n");
						CurrentCPURunMode = SHUTDOWN;
					}
				}

		}
		/* now wait for the alarms */
		while ((s = sem_wait(&sem_rtc_tick)) == -1 && errno == EINTR) /* wait for rtc tick lock to be free */
			continue;       /* Restart if interrupted by handler */
	}
}

/*
 * Read and write to system rtc ( on cpu board )
 */
void RTC_IO(ushort ioadd) {
	int s;
	int rc;
	struct itimerval times;
	switch(ioadd) {
	case 010: /* Return 0 in A, no other effect */
		gA=0;
		break;
	case 011: /* Clear rtc counter. This resets rtc so next interrupt happens exactly 20ms later.*/
		/* If done repeatedly, no rtc clock pulses will occur. */

		/*
		 * do it by setting up the timer again from scratch...
		 * set up interval timer, starting in 20ms,
		 *	then every 20 ms.
		 */
		times.it_value.tv_sec = 0;
		times.it_value.tv_usec = 20000;
		times.it_interval.tv_sec = 0;
		times.it_interval.tv_usec = 20000;
		rc = setitimer (ITIMER_REAL, &times, NULL);
		if (rc==-1) {
			if (debug) fprintf(debugfile,"ERROR!!! setitimer failure RTC_IO. errno=%d\n",errno);
		}
		break;
	case 012: /* Read real time clock status. */
		/* Bit 0 = 1 => interrupt when next clock pulse arrives */
		/* Bit 3 = 1 => The clock is ready for transfer, i.e. a clock pulse has occured */
		/* All other bits are zero */
		gA = 0;
		while ((s = sem_wait(&sem_rtc)) == -1 && errno == EINTR) /* wait for rtc synch lock to be free */
			continue;       /* Restart if interrupted by handler */
		if (sys_rtc) { /* make sure clock structure exist */
			gA |= (sys_rtc->irq_en) ? 1 : 0;
			gA |= (sys_rtc->rdy) ?  1<<3 : 0;
		}
		if (sem_post(&sem_rtc) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure RTC_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 013: /* Set real-time clock status.*/
		/* Bit 0 = 1 => Enable interrupt if ready for transfer occurs */
		/* Bit 13 = 1 => Clear Ready for transfer */
		while ((s = sem_wait(&sem_rtc)) == -1 && errno == EINTR) /* wait for rtc synch lock to be free */
			continue;       /* Restart if interrupted by handler */
		if (sys_rtc) { /* make sure clock structure exist */
			sys_rtc->irq_en = (gA & 0x01) ? 1 : 0 ;
			if ((gA >> 13 )& 0x01) sys_rtc->rdy = 0;
		}
		if (sem_post(&sem_rtc) == -1) { /* release interrupt lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure RTC_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	}
}
