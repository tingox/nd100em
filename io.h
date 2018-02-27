/*
 * nd100em - ND100 Virtual Machine
 *
 * Copyright (c) 2008 Roger Abrahamsson
 * Copyright (c) 2008 Zdravko Dimitrov
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


extern int trace;
extern FILE *tracefile;
extern int debug;
extern FILE *debugfile;

extern struct CpuRegs *gReg;
extern _RUNMODE_ CurrentCPURunMode;
extern int CONSOLE_IS_SOCKET;
extern ushort MODE_OPCOM;

extern ushort STARTADDR;

extern double instr_counter;

extern struct ThreadChain *AddThreadChain();
extern void RemThreadChain(struct ThreadChain * elem);

extern sem_t sem_run;
extern sem_t sem_stop;

/* OK here we have it, a 64K array of function pointers for iox/ioxt instructions */
/* We still need to initialize it before using, thats the domain of Setup_IO_Handlers */

void (*ioarr[65536])(ushort);

/* And the array of data pointers for these functions. Unitialisez, it's the domain */
/* of Setup_IO_Handlers to create a structure and put in the references here */
/* This way we become very flexible and can reuse same routine for all devices of same type */
/* NOT USED YET */
void (*iodata[65536]);

struct tty_io_data {
	ushort snd_arr[256];	/* send ringbuffer */
	unsigned char snd_fp;	/* feeded pointer for snd ringbuffer */
	unsigned char snd_cp;	/* consumer pointer for snd ringbuffer */
	ushort rcv_arr[256];	/* rcv ringbuffer */
	unsigned char rcv_fp;	/* feeded pointer for rcv ringbuffer */
	unsigned char rcv_cp;	/* consumer pointer for rcv ringbuffer */
	unsigned char ttynum;	/* which ttynum is this?? (0=console) */
	ushort in_status;
	ushort in_control;
	ushort out_status;
	ushort out_control;
};

struct tty_io_data (*tty_arr[256]); /* array of pointers to con_io_data structures we allocate */

#define FDD_BUFSIZE 256
struct fdd_unit {
	char *filename;
	bool readonly;
	FILE *fp;		/* pointer to actual file. If non null points to an open file */
	int drive_format;	/* 0 = ibm3740, 1 = ibm3600, 2 = ibm system 32-II */
	int curr_track;		/* track "head" is on now */
	int diff_track;			/* difference between current and desired track */
	int dir_track;			/* direction 0=lower track no, 1= higher track no */
};

struct floppy_data {
	bool irq_en;			/* allow device interrupts */
	int unit_select;		/* actual fdd 0-2 */
	ushort buff[FDD_BUFSIZE];	/* buffer for 1 sectors data. FIXME:: Check that this is like the real floppy controller do.*/
	int bufptr;			/* buffer pointer */
	bool bufptr_msb;		/* If we work with bytes, access to lsb or msb in buf... */
	struct fdd_unit (*unit[3]);	/* fdd drive unit 0-2 pointers to private data */
	int selected_drive;		/* selected fdd unit 0-2 = drive, -1=no drive*/
	bool test_mode;
	unsigned char test_byte;
	bool timeout_en;
	bool sense;			/* error occured, check status reg 2 for details */
	bool drive_not_rdy;		/* Set if drive is selected and drive has open door/no diskette (no attached file)*/
	bool write_protect;		/* set if trying to write to write protected diskette (file) */
	bool missing;			/* sector missing / no am */
	bool busy;			/* processing a command */
	int command;			/* command to execute */
	ushort sector;
	bool sector_autoinc;
};

struct hdd_10mb_unit {
	char *filename; /* hdd image name */
	char access;	/* 'r' = readonly, 'w' = read/write */
};

struct hdd_10mb_data {
	bool irq_rdy_en;	/* device ready for transfer enable */
	bool irq_err_en;	/* error interrupt enable */
	bool irq_rdy;
	bool irq_err;
	int unit_select;	/* actual hdd 0-3 */
	struct hdd_10mb_unit (*unit[4]);	/* hdd drive unit 0-3 pointers to private data */
};

/* TEMP!!! Solution, until we have completely changed config parsing*/
char *FDD_IMAGE_NAME;
bool FDD_IMAGE_RO;

#define TERM_IO_NUM 46
ushort reg_TerminalIO[TERM_IO_NUM][6] = {{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0}};
// Terminal register map
// 0 - Input data
// 1 - Input status
// 2 - Input control
// 3 - Output data
// 4 - Output status
// 5 - Output control

ushort reg_Tesselator[4][8] = {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}};

void io_op (ushort ioadd);
void Default_IO(ushort ioadd);
void floppy_init();
void Floppy_IO(ushort ioadd);
void Parity_Mem_IO(ushort ioadd);
int mopc_in(char * chptr);
void mopc_out(char ch);
void Console_IO(ushort ioadd);
void Setup_IO_Handlers (void);
void do_listen(int port, int numconn, int * sock);
void console_stdio_in(void);
void console_stdio_thread(void);
void console_socket_in(int *connected);
void console_socket_thread(void);
void panel_thread(void);
void setup_pap();
void panel_event();
void panel_processor_thread();


extern void RTC_IO(ushort ioadd);
extern int mysleep(int sec, int usec);
extern void setbit_STS_MSB(ushort stsbit, char val);
extern void setbit(ushort regnum, ushort stsbit, char val);
extern void interrupt(ushort lvl, ushort sub);

