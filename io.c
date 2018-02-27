/*
 * nd100em - ND100 Virtual Machine
 *
 * Copyright (c) 2008 Roger Abrahamsson
 * Copyright (c) 2008 Zdravko Dimitrov
 * Copyright (c) 2008 Goran Axelsson
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
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "nd100.h"
#include "io.h"

/* console synchronization*/
sem_t sem_cons;

/* io synchronization*/
sem_t sem_io;

/* floppy synchronization*/
sem_t sem_floppy;

/* panel processor synchronization*/
sem_t sem_pap;

struct display_panel *gPAP;

/*
 * IOX and IOXT operation, dummy for now.
 * The Norm is: even address is read, odd address is write
 * source: ND-100 Input/Output Manual
 */
void io_op (ushort ioadd) {
        ioarr[ioadd](ioadd);	/* call using a function pointer from the array
				this way we are as flexible as possible as we
				implement io calls. */
}

/*
 * Access to unpopulated IO area
 */
void Default_IO(ushort ioadd) {
	mysleep(0,10); /* Sleep 10us for IOX timeout time simulation */
	if(gReg->reg_IIE & 0x80) {
		if (trace & 0x01) fprintf(tracefile,
			"#o (i,d) #v# (\"%d\",\"No IO device, IOX error interrupt after 10 us.\");\n",
			(int)instr_counter);
		interrupt(14,1<<7); /* IOX error lvl14 */
	}
}

/*
 * Read and write from/to floppy
 */
void Floppy_IO(ushort ioadd) {
	int s;
	int a = ioadd & 0x07;
	ushort tmp;
	struct floppy_data *dev = iodata[ioadd];
	if (debug) fprintf(debugfile,"Floppy_IO: IOX %d - A=%d\n",ioadd,gA);
	fflush(debugfile);
	switch(a) {
	case 0: /* IOX RDAD - Read data buffer */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */

		if (dev->bufptr >= 0 && dev->bufptr < FDD_BUFSIZE) {
			gA = dev->buff[dev->bufptr];
			dev->bufptr++;
			if(dev->bufptr >= FDD_BUFSIZE)
				dev->bufptr = 0;
		}

		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Floppy_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 1: /* IOX WDAT - Write data buffer */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */

		if (dev->bufptr>=0 && dev->bufptr<FDD_BUFSIZE) {
			dev->buff[dev->bufptr] = gA;
			dev->bufptr++;
			if(dev->bufptr >= FDD_BUFSIZE)
				dev->bufptr = 0;
		}

		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Floppy_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 2: /* IOX RSR1 - Read status register No. 1 */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */

		gA = 0; /* Put A in consistent state */
		gA |= (dev->irq_en) ? (1<<1) : 0;	/* IRQ enabled (bit 1)*/
		gA |= (dev->busy) ? (1<<2) : 0;		/* Device is busy */
		gA |= (dev->sense) ? (1<<4) : 0;	/* interrupt set, check STS reg 2 */

		if (debug) fprintf(debugfile,"Floppy_IO: IOX %o RSR1 - A=%04x\n",ioadd,gA);

		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Floppy_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 3: /* IOX WCWD - Write control word */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */

		if ((gA >> 1) & 0x01) {
			dev->irq_en = 1;
		}
		if ((gA >> 3) & 0x01) {
			dev->test_mode = 1;
		}
		if ((gA >> 4) & 0x01) {		/* Device clear */
			dev->selected_drive = -1;
			dev->bufptr = 0;
		}
		if ((gA >> 5) & 0x01) {		/* Clear Interface buffer address */
			dev->bufptr = 0;
		}
		if (gA & 0xff00) {
			dev->busy = 1;
			dev->command = ((gA & 0xff00 ) >>8);
			/* TRIGGER FLOPPY THREAD */
			if (sem_post(&sem_floppy) == -1) { /* release floppy lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_floppy failure Floppy_IO\n");
				CurrentCPURunMode = SHUTDOWN;
			}

		}

		if (debug) fprintf(debugfile,"Floppy_IO: IOX %o WCWD - A=%04x\n",ioadd,gA);

		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Floppy_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 4: /* IOX RSR2 - Read status register No. 2 */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */

		gA = 0; /* Put A in consistent state */
		gA |= (dev->drive_not_rdy) ? (1<<8) : 0;	/* drive not ready (bit 8)*/
		gA |= (dev->write_protect) ? (1<<9) : 0;	/* set if trying to write to write protected diskette (file) */
		gA |= (dev->missing) ? (1<<11) : 0;		/* sector missing / no am */

		if (debug) fprintf(debugfile,"Floppy_IO: IOX %o RSR2 - A=%04x\n",ioadd,gA);

		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Floppy_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 5: /* IOX WDAD - Write Drive Address/ Write Difference */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */

		if (gA | 0x1) { /* Write drive address */
			if (debug) fprintf(debugfile,"IOX 1565 - Write Drive Address...\n");
			tmp = (gA >> 8) & 0x07;
			if (tmp <4)
				dev->selected_drive = tmp;
			if ((gA >> 11) & 0x01)
				dev->selected_drive = -1;
			tmp = (gA >> 14) & 0x03;
			switch (tmp) {
			case 0:
				if(dev->selected_drive != -1)
					dev->unit[dev->selected_drive]->drive_format = 0;
				break;
			case 1:
				if(dev->selected_drive != -1)
					dev->unit[dev->selected_drive]->drive_format = 0;
				break;
			case 2:
				if(dev->selected_drive != -1)
					dev->unit[dev->selected_drive]->drive_format = 1;
				break;
			case 3:
				if(dev->selected_drive != -1)
					dev->unit[dev->selected_drive]->drive_format = 2;
				break;
			}
		} else { /* Write difference */
			if (debug) fprintf(debugfile,"IOX 1565 - Write Difference...\n");
			tmp= (gA >> 8) & 0x7f;
			if(dev->selected_drive != -1) {
				dev->unit[dev->selected_drive]->diff_track = tmp;
				dev->unit[dev->selected_drive]->dir_track = (gA >> 15 ) & 0x01;
			}
		}

		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Floppy_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 6: /* Read Test */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */

		if (dev->bufptr>=0 && dev->bufptr<FDD_BUFSIZE) {
			if (dev->bufptr_msb) {
				dev->buff[dev->bufptr] = (dev->buff[dev->bufptr] & 0x00ff) | ((dev->test_byte << 8) & 0xff00);
				dev->bufptr_msb = 0;
				dev->bufptr++;
			} else {
				dev->buff[dev->bufptr] = (dev->buff[dev->bufptr] & 0xff00) | ((dev->test_byte) & 0x00ff);
				dev->bufptr_msb = 1;
			}
			if(dev->bufptr >= FDD_BUFSIZE)
				dev->bufptr = 0;
		}

		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Floppy_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 7: /*IOX WSCT - Write Sector/ Write Test Byte */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */

		if (dev->test_mode) {
			dev->test_byte = (gA >>8) & 0xff;
		} else {
			dev->sector = (gA >> 8) & 0x7f;
			dev->sector_autoinc = (gA>>15) &0x01;
			/*TODO:: check ranges on sector based on floppy type selected */
		}

		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Floppy_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	}
}

/*
 * TODO:: Not having figured out exactly what these are supposed to do.
 * MIGHT be described in Sintran III Reference Manual...
 * So just add functionality here for now to get further in testing programs
 * by guessing what they want :)
 */
void Parity_Mem_IO(ushort ioadd) {
	switch(ioadd) {
	case 04: /* Read */
		break;
	case 05: /* Write */
		break;
	case 06: /* Read */
		gA = 0x0008; /* test program seems to look for this */
		break;
	case 07: /* Write */
		break;
	}
}

/*
 * Read and write from/to floppy
 */
void HDD_10MB_IO(ushort ioadd) {
	int reladd = (int)(ioadd & 0x07); /* just get lowest three bits, to work with both disk system I and II */
	switch(reladd) {
	case 0: /* Read Memory Address */
		break;
	case 1: /* Load Memory Address */
		break;
	case 2: /* Read Sector Counter */
		break;
	case 3: /* Load Block Address */
		break;
	case 4: /* Read Status Register */
		break;
	case 5: /* Load Control Word */
		break;
	case 6: /* Read Block Address */
		break;
	case 7: /* Load Word Counter Register */
		break;
	}
	return;
}

/*
 * mopc function to scan for an available char
 * returns nonzero if char was available and the char
 * in the address pointed to by chptr.
 */
int mopc_in(char *chptr) {
	int s;
	unsigned char cp,pp,ptr; /* ringbuffer pointers */
	ushort status;

	if (debug) fprintf(debugfile,"(##) mopc_in...\n");
	if (debug) fflush(debugfile);

	if(!(tty_arr[0]))	/* array dont exists, so no chars available */
		return(0);

	cp = tty_arr[0]->rcv_cp;
	pp = tty_arr[0]->rcv_fp;
	status = tty_arr[0]->in_status;
	if (sem_post(&sem_io) == -1) { /* release io lock */
		if (debug) fprintf(debugfile,"ERROR!!! sem_post failure mopc_in\n");
		CurrentCPURunMode = SHUTDOWN;
	}

	if (!(status & 0x0008))	/* Bit 3=0 device not ready for transfer */
		return(0);

	if (debug) fprintf(debugfile,"(##) mopc_in looking for char...\n");
	if (debug) fflush(debugfile);

	if (pp != cp) {	/* ok we have some data here */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */
		ptr = tty_arr[0]->rcv_cp;
		*chptr =  (tty_arr[0]->rcv_arr[ptr] & 0x7f);
		tty_arr[0]->rcv_cp +=1;
		if (tty_arr[0]->rcv_fp == tty_arr[0]->rcv_cp) {
			tty_arr[0]->in_status &= ~0x0008;	/* Bit 3=0 device not ready for transfer */
		}
		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure mopc_in\n");
			CurrentCPURunMode = SHUTDOWN;
		}

		if (debug) fprintf(debugfile,"(##) mopc_in data found...\n");
		if (debug) fflush(debugfile);

		return(1);
	} else {
		if (debug) fprintf(debugfile,"(##) mopc_in data not found...\n");
		if (debug) fflush(debugfile);
		return(0);
	}
}

/*
 * mopc function to output a char ch.
 */
void mopc_out(char ch) {
	int s;
	unsigned char ptr; /* ringbuffer pointer */

	if (debug) fprintf(debugfile,"(##) mopc_out...\n");
	if (debug) fflush(debugfile);

	if(tty_arr[0]){ /* array exists so we can work with this now */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */
		ptr=tty_arr[0]->snd_fp;
		tty_arr[0]->snd_arr[ptr]= ch;
		tty_arr[0]->snd_fp++;
		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure mopc_out\n");
			CurrentCPURunMode = SHUTDOWN;
		}

		if (sem_post(&sem_cons) == -1) { /* release console lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure mopc_out\n");
			CurrentCPURunMode = SHUTDOWN;
		}
	}
}


/*
 * Read and write to system console
 */
void Console_IO(ushort ioadd) {
	int s;
	unsigned char ptr; /* ringbuffer pointer */
	switch(ioadd) {
	case 0300: /* Read input data */
		if (!tty_arr[0]){ /* tty structure not created yet... return zero(safety function) */
			gA=0;
			return;
		}
		if (MODE_OPCOM) { /* mopc has authority */
			return;
		}
		if(tty_arr[0]) { /* array exists so we can work with this now */
			while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
				continue; /* Restart if interrupted by handler */
			if (tty_arr[0]->rcv_fp != tty_arr[0]->rcv_cp) { /* ok we have some data here */
				ptr = tty_arr[0]->rcv_cp;
				gA =  (tty_arr[0]->rcv_arr[ptr] & 0x00ff);
				tty_arr[0]->rcv_cp++;
				if (tty_arr[0]->rcv_fp == tty_arr[0]->rcv_cp) {
					tty_arr[0]->in_status &= ~0x0008; /* Bit 3=0 device not ready for transfer */
				}
			} else {
				gA=0;
			}
			if (sem_post(&sem_io) == -1) { /* release io lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Console_IO\n");
				CurrentCPURunMode = SHUTDOWN;
			}
		}
		break;
	case 0301: /* NOOP*/
		break;
	case 0302: /* Read input status */
		if (!tty_arr[0]){ /* tty structure not created yet... return zero (safety function) */
			gA=0;
			return;
		}
		if (MODE_OPCOM) { /* mopc has authority */
			while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
				continue; /* Restart if interrupted by handler */
			gA =tty_arr[0]->in_status & ~0x00008;	/* Bit 3=0 device not ready for transfer */
			if (sem_post(&sem_io) == -1) { /* release io lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Console_IO\n");
				CurrentCPURunMode = SHUTDOWN;
			}
			return;
		}
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */
		gA =tty_arr[0]->in_status;
			if (sem_post(&sem_io) == -1) { /* release io lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Console_IO\n");
				CurrentCPURunMode = SHUTDOWN;
			}
		break;
	case 0303: /* Set input control */
		if (!tty_arr[0]) return; /* tty structure not created yet... return zero (safety function) */
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */
		tty_arr[0]->in_control = gA; /* sets control reg all flags */
		if (gA & 0x0004) {	/* activate device */
			tty_arr[0]->in_status |= 0x0004;
		} else {		/* deactivate device */
			tty_arr[0]->in_status &= ~0x0004;
		}
		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Console_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 0304: /* Returns 0 in A */
		gA = 0;
		break;
	case 0305: /* Write data */
		if(tty_arr[0]){ /* array exists so we can work with this now */
			while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
				continue; /* Restart if interrupted by handler */
			ptr=tty_arr[0]->snd_fp;
			tty_arr[0]->snd_arr[ptr]=gA & 0x007F;
			tty_arr[0]->snd_fp++;
			if (sem_post(&sem_io) == -1) { /* release io lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Console_IO\n");
				CurrentCPURunMode = SHUTDOWN;
			}

			if (sem_post(&sem_cons) == -1) { /* release console lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Console_IO\n");
				CurrentCPURunMode = SHUTDOWN;
			}
		}
		break;
	case 0306: /* Read output status */
		if (!tty_arr[0]){ /* tty structure not created yet... return zero (safety function) */
			gA=0;
			return;
		}
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */
		gA=tty_arr[0]->out_status;
		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Console_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		break;
	case 0307: /* Set output control */
		if (!tty_arr[0]) return; /* tty structure not created yet... return zero (safety function) */
		break;
	}
}

void IO_Handler_Add(int startdev, int stopdev, void *funcpointer, void *datapointer) {
	int i;
	for(i=startdev;i<=stopdev;i++)
		ioarr[i] = funcpointer;
	return;
}

void IO_Data_Add(int startdev, int stopdev,  void * datastructptr) {
	int i;
	for(i=startdev;i<=stopdev;i++)
		iodata[i] = datastructptr;
	return;
}


/* Add IO handler addresses in this function */
void Setup_IO_Handlers () {
	int i;

	/* to be removed, and changed to new structure */
	for(i=0; i<=255;i++){
		tty_arr[i] = 0;
	}

	IO_Handler_Add(0,65535,&Default_IO,NULL);		/* Default setup to dummy routine */
	IO_Data_Add(0,65535,NULL);				/* Make sure all data pointers are NULL */
	IO_Handler_Add(4,7,&Parity_Mem_IO,NULL);		/* Parity Memory something, 4-7 octal */
	IO_Handler_Add(8,11,&RTC_IO,NULL);			/* CPU RTC 10-13 octal */
	IO_Handler_Add(192,199,&Console_IO,NULL);		/* Console terminal 300-307 octal */
	IO_Handler_Add(880,887,&Floppy_IO,NULL);		/* Floppy Disk 1 at 1560-1567 octal */
	floppy_init();
//	IO_Handler_Add(320,327,&HDD_10MB_IO,NULL);		/* Disk System I at 500-507 octal */
}

/*
 * floppy_init
 * NOTE: Not sure if this is the best way, but needed to start somewhere. Might be a totally different solution in the end.
 *
 */
void floppy_init() {
	struct floppy_data * ptr;
	struct fdd_unit * ptr2;

	ptr =calloc(1,sizeof(struct floppy_data));
	if (ptr) {
		ptr2 = calloc(1,sizeof(struct fdd_unit));
		if (ptr2) {
			ptr->unit[0] = ptr2;
			if(FDD_IMAGE_NAME) {
				ptr->unit[0]->filename = strdup(FDD_IMAGE_NAME);
				ptr->unit[0]->readonly = FDD_IMAGE_RO;
				if (FDD_IMAGE_RO && ptr->unit[0]->filename) {
					ptr->unit[0]->fp = fopen(FDD_IMAGE_NAME, "r");
				} else if(ptr->unit[0]->filename){
					ptr->unit[0]->fp = fopen(FDD_IMAGE_NAME, "r+");
				}
			}
		}
		ptr2 = calloc(1,sizeof(struct fdd_unit));
		if (ptr2) {
			ptr->unit[1] = ptr2;
		}
		ptr2 = calloc(1,sizeof(struct fdd_unit));
		if (ptr2) {
			ptr->unit[2] = ptr2;
		}
	}
	ptr->selected_drive = -1;	/* no drive selected at start */
	IO_Data_Add(880,887,ptr);
}

/*
 * Here we do the stuff to setup a socket and listen to it.
 * The rest is supposed to be done in the function calling it,
 * like starting up threads for accepting connections etc.
 */
void do_listen(int port, int numconn, int * sock) {

	int istrue = 1;
	struct sockaddr_in server_addr;
	char errbuff[256];

	if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		if( strerror_r( errno, errbuff, 256 ) == 0 ) {
			if (debug) fprintf(debugfile,"(#)SOCKET error -- %s\n",errbuff);
		}
		CurrentCPURunMode = SHUTDOWN;
		return;
	}
	if (setsockopt(*sock,SOL_SOCKET,SO_REUSEADDR,&istrue,sizeof(int)) == -1) {
		if( strerror_r( errno, errbuff, 256 ) == 0 ) {
			if (debug) fprintf(debugfile,"(#)SOCKET error -- %s\n",errbuff);
		}
		CurrentCPURunMode = SHUTDOWN;
		return;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(server_addr.sin_zero),8);
	if (bind(*sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		if( strerror_r( errno, errbuff, 256 ) == 0 ) {
			if (debug) fprintf(debugfile,"(#)SOCKET bind error -- %s\n",errbuff);
		}
		CurrentCPURunMode = SHUTDOWN;
		return;
	}
	if (listen(*sock, numconn) == -1) {
		if( strerror_r( errno, errbuff, 256 ) == 0 ) {
			if (debug) fprintf(debugfile,"(#)SOCKET listen error -- %s\n",errbuff);
		}
		CurrentCPURunMode = SHUTDOWN;
		return;
	}
}

void console_stdio_in() {
	int s;
	char ch,parity;
	char recv_data[1024];
	int numbytes;
	unsigned char pp,cp,tmp,cnt,cnt2;
	int numread;
	ushort status,control;
	fd_set rfds;
	int retval;

	parity=0;

	if (debug) fprintf(debugfile,"(#)console_stdio_in running...\n");
	if (debug) fflush(debugfile);

	/* Watch stdin (fd 0) to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);

	while(CurrentCPURunMode != SHUTDOWN) {

		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */
		status = tty_arr[0]->in_status;
		control = tty_arr[0]->in_control;
		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure console_stdio_in\n");
			CurrentCPURunMode = SHUTDOWN;
		}

		if (status & 0x0004) { /* Bit 2=1 device is active */
			while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
				continue; /* Restart if interrupted by handler */
			pp=tty_arr[0]->rcv_fp;
			cp=tty_arr[0]->rcv_cp;
			if (sem_post(&sem_io) == -1) { /* release io lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure console_stdio_in\n");
				CurrentCPURunMode = SHUTDOWN;
			}

			/* this gets us max buffer size we can use */
			numbytes = (cp < pp) ? 256-pp+cp-2 : (pp < cp) ? cp-pp-2 : 254;
			numbytes = ( numbytes > 0) ? numbytes : 0;

			retval = select(1, &rfds, NULL, NULL, NULL);

			/* ok lets send what we got... */
			numread = read (0, recv_data, numbytes);
			tmp = numread;
			if (numread > 0) {
				tmp = numread;
				for (cnt=0;tmp>0; tmp--){
					ch=recv_data[cnt];

					if (debug) fprintf (debugfile,"(##) ch=%i (%c)\n",ch,ch);
					if (debug) fflush(debugfile);

					ch = (ch == 10) ? 13 : ch; /* change lf to cr */
					switch((control & 0x1800)>>11){
					case 0:/* 8 bits */
						break;
					case 1:/* 7 bits */
						ch &= 0x7f;
						break;
					case 2:/* 6 bits */
						ch &= 0x3f;
						break;
					case 3:/* 5 bits */
						ch &= 0x1f;
						break;
					}
					if(control & 0x4000) {	/* Bit 14=1 even parity is used */
						/* set parity to 0 for even parity or 1 for odd parity  */
						parity=0;
						for (cnt2 = 0; cnt2 < 8; cnt2++)
							parity ^= ((ch >> cnt2) & 1);
						ch = (parity) ? ch | 0x80 : ch;
					}

					if (debug) fprintf (debugfile,"(##) ch=%i (%c) parity=%i statusreg(bits)=%d\n",ch,ch,parity,(int)((control & 0x1800)>>11));
					if (debug) fflush(debugfile);

//					if (ch == 10) { /* lf?? */
//						cnt++;
//					} else {
						while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
							continue; /* Restart if interrupted by handler */
						pp=tty_arr[0]->rcv_fp;
						tty_arr[0]->rcv_arr[pp]=ch;
						tty_arr[0]->rcv_fp++;
						if (sem_post(&sem_io) == -1) { /* release io lock */
							if (debug) fprintf(debugfile,"ERROR!!! sem_post failure console_stdio_in\n");
							CurrentCPURunMode = SHUTDOWN;
						}

						cnt++;
						pp++;
//					}
				}
				while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
					continue; /* Restart if interrupted by handler */
				tty_arr[0]->in_status |= 0x0008;	/* Bit 3=1 device ready for transfer */
				if (sem_post(&sem_io) == -1) { /* release io lock */
					if (debug) fprintf(debugfile,"ERROR!!! sem_post failure console_stdio_in\n");
					CurrentCPURunMode = SHUTDOWN;
				}
			}
		}
	}
}

void console_stdio_thread() {
	int s;
//	char ch;
	int numbytes;
	unsigned char pp,cp,tmp;

	struct ThreadChain *tc_elem;

	if (debug) fprintf(debugfile,"(#)console_stdio_thread running...\n");
	if (debug) fflush(debugfile);
	tty_arr[0] = calloc(1,sizeof(struct tty_io_data));

	tc_elem=AddThreadChain();
	pthread_attr_init(&tc_elem->tattr);
	pthread_attr_setdetachstate(&tc_elem->tattr,PTHREAD_CREATE_DETACHED);
	tc_elem->tk = CANCEL;
	pthread_create(&tc_elem->thread, &tc_elem->tattr, (void *)&console_stdio_in, NULL);

	while(CurrentCPURunMode != SHUTDOWN) {
		tty_arr[0]->out_status |= 0x0008; /* Bit 3=1 ready for transfer */
		while(CurrentCPURunMode != SHUTDOWN) {
			while ((s = sem_wait(&sem_cons)) == -1 && errno == EINTR) /* wait for console lock to be free */
				continue; /* Restart if interrupted by handler */

			/* ok lets send what we got... */
			pp=tty_arr[0]->snd_fp;
			cp=tty_arr[0]->snd_cp;
			numbytes=0;
			if (cp != pp) {
				numbytes= (cp < pp) ? pp-cp : 256-cp+pp;
				for(tmp=0;tmp<numbytes;tmp++) {
					printf("%c",tty_arr[0]->snd_arr[cp]);
//					switch (ch) {
//					case 12:
//						break;
//					default:
//						printf("%c",tty_arr[0]->snd_arr[cp]);
//						break;
//					}
					cp++;
				}
				tty_arr[0]->snd_cp=cp;
			}
		}
	}
	return;
}

void console_socket_in(int *connected) {
	int s;
	char ch,parity;
	char recv_data[1024];
	int numbytes;
	ushort status,control;
	unsigned char pp,cp,tmp,cnt,cnt2;
	int numread;
	fd_set rfds;
	int fdmax;
	int retval;
	int throw =0;

	parity = 0;
	fdmax = *connected;

	if (debug) fprintf(debugfile,"(#)console_socket_in running...\n");
	if (debug) fflush(debugfile);

	/* Watch socket to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(*connected, &rfds);

	while(CurrentCPURunMode != SHUTDOWN) {
		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */
		status = tty_arr[0]->in_status;
		control = tty_arr[0]->in_control;
		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure console_stdio_in\n");
			CurrentCPURunMode = SHUTDOWN;
		}
		if (status & 0x0004) { /* Bit 2=1 device is active */
			while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
				continue; /* Restart if interrupted by handler */
			pp=tty_arr[0]->rcv_fp;
			cp=tty_arr[0]->rcv_cp;
			if (sem_post(&sem_io) == -1) { /* release io lock */
				if (debug) fprintf(debugfile,"ERROR!!! sem_post failure console_stdio_in\n");
				CurrentCPURunMode = SHUTDOWN;
			}

			/* this gets us max buffer size we can use */
			numbytes = (cp < pp) ? 256-pp+cp-2 : (pp < cp) ? cp-pp-2 : 254;
			numbytes = ( numbytes > 0) ? numbytes : 0;

			retval = select(fdmax+1, &rfds, NULL, NULL, NULL);

			/* ok lets send what we got... */

			numread = recv(*connected,recv_data,numbytes,0);
			if (numread > 0) {
				tmp = numread;
				for (cnt=0;tmp>0; tmp--){
					ch=recv_data[cnt];
					if((unsigned int)ch == 255) { /* Telnet IAC command */
						throw=2;
						cnt++;
					} else if (throw) { /* previous IAC command, throw out 2 chars after */
						throw--;
						cnt++;
					} else {
						if (debug) fprintf (debugfile,"(##) ch=%i (%c)\n",ch,ch);
						if (debug) fflush(debugfile);
//						ch = (ch == 10) ? 13 : ch; /* change lf to cr */
						ch &= 0x7f;	/* trim to 7 bit ASCII  FIXME:: This should depend on bits 11-12 in control reg*/
						if(tty_arr[0]->in_control && 0x4000){	/* Bit 14=1 even parity is used */
							/* set parity to 0 for even parity or 1 for odd parity  */
							parity=0;
							for (cnt2 = 0; cnt2 < 8; cnt2++)
								parity ^= ((ch >> cnt2) & 1);
							ch = (parity) ? ch | 0x80 : ch;
						}
						if (debug) fprintf (debugfile,"(##) ch=%i (%c) parity=%i\n",ch,ch,parity);
						if (debug) fflush(debugfile);
//						if (ch == 10) { /* lf?? */
//							cnt++;
//						} else {
							tty_arr[0]->rcv_arr[pp]=ch;
							cnt++;
							pp++;
//						}
					}
				}
				tty_arr[0]->rcv_fp=pp;
				tty_arr[0]->in_status |= 0x0008;	/* Bit 3=1 device ready for transfer */
			}
		}
	}
}

void console_socket_thread() {
	int s;
	int sock, connected;
	char send_data [1024];
	struct sockaddr_in client_addr;
	socklen_t sin_size;
	int numbytes;
	unsigned char pp,cp,tmp;

	/* IAC WILL ECHO IAC WILL SUPPRESS-GO-AHEAD IAC DO SUPPRESS-GO-AHEAD */
	char telnet_setup[9] = {0xff,0xfb,0x01,0xff,0xfb,0x03,0xff,0xfd,0x0f3};

	struct ThreadChain *tc_elem;

	if (debug) fprintf(debugfile,"(#)console_socket_thread running...\n");
	if (debug) fflush(debugfile);
	tty_arr[0] = calloc(1,sizeof(struct tty_io_data));

	do_listen(5001, 1, &sock);
	if (debug) fprintf(debugfile,"\n(#)TCPServer Waiting for client on port 5001\n");
	if (debug) fflush(debugfile);

	while(CurrentCPURunMode != SHUTDOWN) {
		sin_size = (socklen_t) sizeof(struct sockaddr_in);
		connected = accept(sock, (struct sockaddr *)&client_addr,&sin_size);
		if (debug) fprintf(debugfile,"(#)I got a console connection from (%s , %d)\n",
			inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
		if (debug) fflush(debugfile);

		/* setup the other side to "uncooked" data */
		send(connected,telnet_setup,9,0);

		tc_elem=AddThreadChain();
		pthread_attr_init(&tc_elem->tattr);
		pthread_attr_setdetachstate(&tc_elem->tattr,PTHREAD_CREATE_DETACHED);
		tc_elem->tk = CANCEL;
		pthread_create(&tc_elem->thread, &tc_elem->tattr, (void *)&console_socket_in, &connected);

		tty_arr[0]->out_status |= 0x0008; /* Bit 3=1 ready for transfer */
		while(CurrentCPURunMode != SHUTDOWN) {
			while ((s = sem_wait(&sem_cons)) == -1 && errno == EINTR) /* wait for console lock to be free */
				continue; /* Restart if interrupted by handler */

			/* ok lets send what we got... */
			pp=tty_arr[0]->snd_fp;
			cp=tty_arr[0]->snd_cp;
			numbytes=0;
			if (cp != pp) {

				numbytes= (cp < pp) ? pp-cp : 256-cp+pp;
				for(tmp=0;tmp<numbytes;tmp++) {
					send_data[tmp]=tty_arr[0]->snd_arr[cp];
					cp++;
				}
				tty_arr[0]->snd_cp=cp;
			}
			if (numbytes) send(connected,send_data,numbytes,0);
		}
	}
	close(sock);
	return;
}

/*
Floppy_IO: IOX 880 - A=0
Floppy_IO: IOX 880 - A=0
Floppy_IO: IOX 882 - A=880
Floppy_IO: IOX 883 - A=2
Floppy_IO: IOX 882 - A=10831
Floppy_IO: IOX 882 - A=2
*/
void floppy_thread(){
	int s;
	struct floppy_data *dev;

	while(CurrentCPURunMode != SHUTDOWN) {
		while ((s = sem_wait(&sem_floppy)) == -1 && errno == EINTR) /* wait for floppu lock to be free and take it */
			continue; /* Restart if interrupted by handler */
		/* Do our stuff now and then return and wait for next freeing of lock. */
		dev = iodata[880];	/*TODO:: This is just a temporary solution!!! */

		while ((s = sem_wait(&sem_io)) == -1 && errno == EINTR) /* wait for io lock to be free and take it */
			continue; /* Restart if interrupted by handler */

		if (dev->busy) {
			if(dev->command == 1) { 		/* CONTROL RESET */
			} else if (dev->command == 1) {		/* RECALIBRATE */
				/* track = 0, interrupt!, set status seek complete */
			} else if (dev->command == 1) {		/* SEEK */
				/* track = nn, interrupt!, set status seek complete */
			} else if (dev->command == 1) {		/* READ ID */
			} else if (dev->command == 1) {		/* READ DATA */
			} else if (dev->command == 1) {		/* WRITE DATA */
			} else if (dev->command == 1) {		/* WRITE DELETED DATA */
			} else if (dev->command == 1) {		/* FORMAT TRACK */
			}
		}

		if (sem_post(&sem_io) == -1) { /* release io lock */
			if (debug) fprintf(debugfile,"ERROR!!! sem_post failure Floppy_IO\n");
			CurrentCPURunMode = SHUTDOWN;
		}
	}
}

void panel_thread() {
	int s;
	int sock, connected, bytes_recieved;
	char recv_data[1024];
	struct sockaddr_in client_addr;
	socklen_t sin_size;

	if (debug) fprintf(debugfile,"(#)panel_thread running...\n");
	if (debug) fflush(debugfile);

	do_listen(5000, 1, &sock);
	if (debug) fprintf(debugfile,"\n(#)TCPServer Waiting for client on port 5000\n");
	if (debug) fflush(debugfile);

	while(CurrentCPURunMode != SHUTDOWN) {
		sin_size = (socklen_t) sizeof(struct sockaddr_in);
		connected = accept(sock, (struct sockaddr *)&client_addr,&sin_size);
		if (debug) fprintf(debugfile,"(#)I got a panel connection from (%s , %d)\n",
			inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
		if (debug) fflush(debugfile);
		while(CurrentCPURunMode != SHUTDOWN) {
			bytes_recieved = recv(connected,recv_data,1024,0);
			recv_data[bytes_recieved] = '\0';
			if (debug) fprintf(debugfile,"(#)PANEL DATA received\n");
			if(strncmp("OPCOM_PRESSED\n",recv_data,strlen("OPCOM_PRESSED"))==0){
				MODE_OPCOM=1;
				if (debug) fprintf(debugfile,"(#)OPCOM_PRESSED\n");

			} else if(strncmp("MCL_PRESSED\n",recv_data,strlen("MCL_PRESSED"))==0){
				if (debug) fprintf(debugfile,"(#)MCL_PRESSED\n");
				/* TODO:: this should be in a separate routine DoMCL later */
				CurrentCPURunMode = STOP;
				/* NOTE:: buggy in that we cannot do STOP and MCL without a running cpu between.. FIXME */
				while ((s = sem_wait(&sem_stop)) == -1 && errno == EINTR) /* wait for stop lock to be free and take it */
					continue; /* Restart if interrupted by handler */
				bzero(gReg,sizeof(struct CpuRegs));	/* clear cpu */
				setbit(_STS,_O,1);
				setbit_STS_MSB(_N100,1);
				gCSR = 1<<2;    /* this bit sets the cache as not available */

			} else if(strncmp("LOAD_PRESSED\n",recv_data,strlen("LOAD_PRESSED"))==0){
				if (debug) fprintf(debugfile,"(#)LOAD_PRESSED\n");
				gPC=STARTADDR;
				CurrentCPURunMode = RUN;
				if (sem_post(&sem_run) == -1) { /* release run lock */
					if (debug) fprintf(debugfile,"ERROR!!! sem_post failure panel_thread\n");
					CurrentCPURunMode = SHUTDOWN;
				}
			} else if(strncmp("STOP_PRESSED\n",recv_data,strlen("STOP_PRESSED"))==0){
				if (debug) fprintf(debugfile,"(#)STOP_PRESSED\n");
				CurrentCPURunMode = STOP;
				/* NOTE:: buggy in that we cannot do STOP and MCL without a running cpu between.. FIXME */
				while ((s = sem_wait(&sem_stop)) == -1 && errno == EINTR) /* wait for stop lock to be free and take it */
					continue; /* Restart if interrupted by handler */
			} else {
				if (debug) fprintf(debugfile,"(#)Panel received:%s\n",recv_data);
			}
			if (debug) fflush(debugfile);
		}
	}
	close(sock);
	return;
}

void setup_pap(){
	gPANS=0x8000;	/* Tell system we are here */
	gPANS=gPANS | 0x4000;	/* Set FULL which is active low, so not full */
	gPAP = calloc(1,sizeof(struct display_panel));
}

void panel_event(){
	char tmpbyte;

	if (gPAP->trr_panc) {	/* TRR has been issued, process command */
		if (debug) fprintf(debugfile,"panel_event: TRR\n");
		if (debug) fflush(debugfile);
		gPAP->trr_panc = false;
		switch ((gPANC & 0x0700)>>8) {
		case 0:		/* Illegal */
			break;
		case 1:		/* Future extension */
			break;
		case 2:		/* Message Append */	// TODO: Not Implemented yet except basic return info
			gPANS = 0xd200;
			break;
		case 3:		/* Message Control */	// TODO: Not Implemented yet except basic return info
			gPANS = 0xd300;
			break;
		case 4:		/* Update Low Seconds */
			if (gPANC & 0x2000){	/* Read */
				tmpbyte = (gPAP->seconds) & 0x00ff;
				gPANS = 0xf400 | tmpbyte;
			} else {		/*Write */
				tmpbyte = gPANC & 0x00ff;
				gPAP->seconds = (gPAP->seconds & 0xff00) | tmpbyte;
				gPANS = 0xd400;
			}
			break;
		case 5:		/* Update High Seconds */
			if (gPANC & 0x2000){	/* Read */
				tmpbyte = (gPAP->seconds >> 8);
				gPANS = 0xf500 | tmpbyte;
			} else {		/*Write */
				tmpbyte = gPANC & 0x00ff;
				gPAP->seconds = (gPAP->seconds & 0x00ff) | ((ushort)tmpbyte)<<8;
				gPANS = 0xd500;
			}
			break;
		case 6:		/* Update Low Days */
			if (gPANC & 0x2000){	/* Read */
				tmpbyte = (gPAP->days) & 0x00ff;
				gPANS = 0xf600 | tmpbyte;
			} else {		/*Write */
				tmpbyte = gPANC & 0x00ff;
				gPAP->days = (gPAP->days & 0xff00) | tmpbyte;
				gPANS = 0xd600;
			}
			break;
		case 7:		/* Update High Days */
			if (gPANC & 0x2000){	/* Read */
				tmpbyte = (gPAP->days >> 8);
				gPANS = 0xf700 | tmpbyte;
			} else {		/*Write */
				tmpbyte = gPANC & 0x00ff;
				gPAP->days = (gPAP->days & 0x00ff) | ((ushort)tmpbyte)<<8;
				gPANS = 0xd700;
			}
			break;
		default :	/* This should never happen */
			break;
		}
		if (debug) fprintf(debugfile,"panel_event: TRR - result: gPANS = %0x04\n",gPANS);
		if (debug) fflush(debugfile);
	}
	if (gPAP->sec_tick) {	/* Seconds tick from rtc, update counters */
		if (debug) fprintf(debugfile,"panel_event: 1 second tick\n");
		if (debug) fflush(debugfile);
		gPAP->sec_tick = false;
		gPAP->seconds++;
		if (gPAP->seconds >= 43200){	/* 12h wraparound */
			gPAP->seconds = 0;
			gPAP->days++;
		}
	}

}

void panel_processor_thread() {
	int s;
	while(CurrentCPURunMode != SHUTDOWN) {
		while ((s = sem_wait(&sem_pap)) == -1 && errno == EINTR) /* wait for pap 'kick' */
			continue; /* Restart if interrupted by handler */

		panel_event();
	}
	return;
}
