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

extern int trace;
extern int DISASM;
extern ushort PANEL_PROCESSOR;

char debugname[]="debug.log";
char debugtype[]="a";
FILE *debugfile;
int debug = 0;

#define RUNNING_DIR     "/tmp"

extern _NDRAM_		VolatileMemory;
extern _RUNMODE_	CurrentCPURunMode;
extern _CPUTYPE_	CurrentCPUType;

extern struct CpuRegs *gReg;
extern union NewPT *gPT;
extern struct MemTraceList *gMemTrace;
extern struct IdentChain *gIdentChain;

extern double instr_counter;

extern pthread_mutex_t mutrun;
extern pthread_cond_t condrun;
extern pthread_mutex_t mutmopc;
extern pthread_cond_t condmopc;

struct ThreadChain *gThreadChain;

int emulatemon = 1;

int CONFIG_OK = 0;	/* This should be set to 1 when config file has been loaded OK */
typedef enum {BP, BPUN, FLOPPY} _BOOT_TYPE_;
_BOOT_TYPE_	BootType; /* Variable holding the way we should boot up the emulator */
ushort	STARTADDR;
/* should we try and disassemble as we run? */
int DISASM = 0;
/* Should we detatch and become a daemon or not? */
int DAEMON = 0;
/* is console on a socket, or just the local one? */
int CONSOLE_IS_SOCKET=0;

struct config_t *pCFG;

extern char *FDD_IMAGE_NAME;
extern bool FDD_IMAGE_RO;


/* semaphore to release signal thread when terminating */
sem_t sem_sigthr;
extern sem_t sem_rtc_tick;
extern sem_t sem_mopc;
extern sem_t sem_run;

struct termios savetty;

extern void rtc_20(void);
extern void cpu_thread();
extern void mopc_thread(void);
extern void panel_thread(void);
extern void console_socket_thread(void);
extern void console_stdio_thread(void);
extern void floppy_thread(void);
extern void floppy_init(void);
extern void MemoryWrite(ushort value, ushort addr, bool UseAPT, unsigned char byte_select);
extern ushort MemoryRead(ushort addr, bool UseAPT);

extern void Setup_IO_Handlers ();
extern void setbit(ushort regnum, ushort stsbit, char val);
extern void setbit_STS_MSB(ushort stsbit, char val);
extern int sectorread (char cyl, char side, char sector, unsigned short *addr);
extern void disasm_addword(ushort addr, ushort myword);
extern void panel_processor_thread();


int octalstr_to_integer(char *str);
int mysleep(int sec, int usec);
int bpun_load(void);
int bp_load(void);
int debug_open(void);
void unsetcbreak (void);
void setcbreak (void);
struct ThreadChain *AddThreadChain(void);
void RemThreadChain(struct ThreadChain * elem);
int nd100emconf(void);
void shutdown(int signum);
void setsignals(void);
void daemonize(void);
pthread_t add_thread(void *funcpointer, bool is_jointype);
void start_threads(void);
void stop_threads(void);
void setup_cpu(void);
void program_load(void);

