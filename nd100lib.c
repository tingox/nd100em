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
#include <signal.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <libconfig.h>
#include "nd100.h"
#include "nd100lib.h"

/*
 * IN: pointer to string of octal chars.
 * OUT: integer
 * NOTE: No error handling...Should not matter since it should only be called with 16 bit numbers normally
 */
int octalstr_to_integer(char *str){
	int i,len,res;
	char ch;
	len=strlen(str);
	res=0;
	for(i=0;i<len;i++){
		ch=str[len-i-1];
		ch=ch-48;  /* Ok, "0" now equals 0 */
		res += (int)ch*pow(8,i);
	}
	return(res);
}

int mysleep(int sec, int usec) {
#if !(defined __FreeBSD__ || defined BSD )
	int res;
	struct timeval timeout;
	/* Initialize the timeout data structure. */
	timeout.tv_sec = sec;
	timeout.tv_usec = usec;
	res = select (FD_SETSIZE,NULL, NULL, NULL,&timeout);
	return res;
#else
	if (sec) sleep(sec);
	if (usec) usleep(usec);
	return 0;
#endif
}

/*
 * Load routine for BPUN files with MSB,LSB ordering (Big-endian)
 * :TODO: Fix parsing of numbers in header, and action code. Also
 * fix checks for overflows.
 */

int bpun_load() {
	FILE *bpun_file;
	char bpun[]="test.bpun";
	char *str;
	ushort read_word, eff_word, temp, checksum,load_add;
	char read_byte;
	bool isnum,isheader;
	ushort counter,bpun_words;
	int count, b_num, c_num;
	char loadtype[]="r";
	ushort *addr;
	addr = (ushort *)&VolatileMemory;

	if (debug) fprintf(debugfile,"BPUN file load:\n");
	counter=0;
	str = calloc(200,1);
	bpun_file=fopen(bpun,loadtype);

	isheader = true;
	isnum = false;
	bpun_words = 0;
	load_add = 0;
	/* Get BPUN header */
	do {
                count = fread(&read_byte,1,1,bpun_file);
		read_byte &= 0x7f; /* Remove parity bit */
//		if (debug) fprintf(debugfile,"BPUN load: Byte READ %c @ counter: %d\n",read_byte,counter);
		if ((read_byte >='0') && (read_byte <='9')) {
			isnum=true;
			strncat(str,&read_byte,1);
			/* :TODO: Fix checks for str overflow. */
		} else if (read_byte == 13){
			if (isnum) {
				b_num = octalstr_to_integer(str);
				if (debug) fprintf(debugfile,"B number: %s, lenght %d, b_num=%d\n",str,(int)strlen(str),b_num);
				free(str);
				str = calloc(200,1);
			}
			isnum=false;
		} else if (read_byte == '!'){
			if (debug) fprintf(debugfile,"Found !\n");
			if (isnum) {
				c_num = octalstr_to_integer(str);
				if (debug) fprintf(debugfile,"C number: %s, lenght %d, c_num=%d\n",str,(int)strlen(str),c_num);
				free(str);
			}
			isnum=false;
			isheader=false;
		} else {
			isnum=false;
		}
		counter++;
	} while ((count > 0) && isheader);
	/* Get block load address */
	if (count) {
                count = fread(&read_word,1,2,bpun_file);
		temp = (read_word & 0xff00) >> 8;
		eff_word = temp |((read_word & 0x00ff) << 8);
		load_add=eff_word;
		if (debug) fprintf(debugfile,"Block load address: %d\n",eff_word);
	}
	/* Get block word count */
	if (count) {
                count = fread(&read_word,1,2,bpun_file);
		temp = (read_word & 0xff00) >> 8;
		eff_word = temp |((read_word & 0x00ff) << 8);
		bpun_words = eff_word;
		if (debug) fprintf(debugfile,"Word count of block F: %d\n",eff_word);
	}
	/* Get BPUN data */
	counter=0; checksum=0;
	do {
                count = fread(&read_word,1,2,bpun_file);
		temp = (read_word & 0xff00) >> 8;
		eff_word = temp |((read_word & 0x00ff) << 8);
		if (count){
			MemoryWrite(eff_word,(counter+load_add),0,2);
			if (DISASM){
				disasm_addword(counter+load_add,eff_word);
			}
		}
		checksum = (eff_word + checksum) & 0xffff;
		counter++;
	} while ((count > 0) && counter < bpun_words);
	/* Get BPUN checksum */
	if (count) {
                count = fread(&read_word,1,2,bpun_file);
		temp = (read_word & 0xff00) >> 8;
		eff_word = temp |((read_word & 0x00ff) << 8);
		if (debug) fprintf(debugfile,"Checksum word: %d, expected: %d\n",eff_word,checksum);
	}
	/* Get BPUN action code.. Assuming word size */
	if (count) {
                count = fread(&read_word,1,2,bpun_file);
		temp = (read_word & 0xff00) >> 8;
		eff_word = temp |((read_word & 0x00ff) << 8);
		if (debug) fprintf(debugfile,"Action code: %d\n",eff_word);
	}
	fclose(bpun_file);
	return 0;
}

int bp_load() {
	int i; ushort eff_word;
	FILE *bin_file;
	char bpun[]="test.bp";
	char loadtype[]="r+";

	if (debug) fprintf(debugfile,"BP file load:\n");
	bin_file=fopen(bpun,loadtype);
	fread(&VolatileMemory,2,65536,bin_file);
	for(i=0;i<65536;i++){
		if (DISASM){
			eff_word = MemoryRead((ushort)i,0);
			disasm_addword(i,eff_word);
		}
	}
	fclose(bin_file);
	return 0;
}

int debug_open(){
	debugfile=fopen(debugname,debugtype);
	if(debugfile) {
		fprintf(debugfile,"\n-----------------NEW DEBUG---------------------------------------\n");
	} else {
		debug = 0; /* Since we failed to open the debugfile, turn off debugging. But otherwise continue. */
	};
	return(0);
}

void unsetcbreak (void) {/* prepare to exit this program. */
	tcsetattr(0, TCSADRAIN, &savetty);
}

void setcbreak (void) {/* set console input to raw mode. */
	struct termios tty;
	tcgetattr(0, &savetty);
	tcgetattr(0, &tty);
	tty.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN);
	tty.c_cc[VTIME] = (cc_t)0;     /* inter-character timer unused */
	tty.c_cc[VMIN] = (cc_t)0;	/* Dont wait for chars */
	tcsetattr(0, TCSADRAIN, &tty);
}

struct ThreadChain *AddThreadChain() {
	if (debug) fprintf(debugfile,"AddThreadChain called...\n");
	if (debug) fflush(debugfile);

	struct ThreadChain * curr, * new;
	new=calloc(1,sizeof(struct ThreadChain));
	curr=gThreadChain;
	if(curr !=0 ) {
		while (curr->next != 0)
			curr=curr->next;
	} else {
		gThreadChain=new;
		return(new);
	}
	curr->next=new;
	new->prev=curr;
	return(new);
}

void RemThreadChain(struct ThreadChain * elem){
	if (debug) fprintf(debugfile,"RemThreadChain called...\n");
	if (debug) fflush(debugfile);

	struct ThreadChain * p, * n;
	n=elem->next;
	p=elem->prev;
	/* Unlink this element */
	if(n && p) {
		p->next=n;
		n->prev=p;
	} else if (n){
		n->prev=0;
		gThreadChain=n;
	} else if (p) {
		p->next=0;
	} else {
		gThreadChain=0;
	}
	free(elem);
}

/*
 * New config model used libconfig to load configuration file.
*/
int nd100emconf(){
	char conf[]="nd100em.conf";
	char *tmpstr;
	config_setting_t *setting = NULL;

	pCFG=malloc(sizeof(struct config_t));
	if(pCFG == NULL) {
		printf("ERROR allocating memory for configuration data!!!\n");
		exit(1);
	}
	bzero(pCFG,sizeof(struct config_t));
	/* Load config file */
	if(!config_read_file(pCFG, conf)){
		printf("Failed reading config file...\n");
		return(1);
	}
	setting = config_lookup(pCFG, "cputype");
	if (setting) {
		tmpstr = (char *) config_setting_get_string(setting);
		if (tmpstr) {
			if(strcmp("nd110cx",tmpstr)==0){
				CurrentCPUType = ND110CX; /* ND110/CX */
			} else if(strcmp("nd110ce",tmpstr)==0){
				CurrentCPUType = ND110CE; /* ND110/CE */
			} else if(strcmp("nd110",tmpstr)==0){
				CurrentCPUType = ND110; /* Plain ND110 */
			} else if(strcmp("nd100cx",tmpstr)==0){
				CurrentCPUType = ND100CX; /* ND100/CX */
			} else if(strcmp("nd100ce",tmpstr)==0){
				CurrentCPUType = ND100CE; /* ND100/CE */
			} else {
				CurrentCPUType = ND100; /* Plain ND100 by default */
			}
		} else {
			CurrentCPUType = ND100; /* Plain ND100 by default */
		}
	} else {
		CurrentCPUType = ND100; /* Plain ND100 by default */
	}
	setting = config_lookup(pCFG, "boot");
	if (setting) {
		tmpstr = (char *)config_setting_get_string(setting);
		if (tmpstr) {
			if(strcmp("bp",tmpstr)==0){
				BootType=BP;
			} else if(strcmp("bpun",tmpstr)==0){
				BootType=BPUN;
			} else if(strcmp("floppy",tmpstr)==0){
				BootType=FLOPPY;
			} else {
				/* TODO:: Need a default value */
			}
		} else {
			/* TODO:: Need a default value */
		}
	} else {
		/* TODO:: Need a default value */
	}
	setting = config_lookup(pCFG, "image");
	if (setting) {
		tmpstr = (char *)config_setting_get_string(setting);
		if (tmpstr) {
			//#and we need the image file name here, can be any image type.
			//image=test
			// NOT USED yet.. ..TODO
		} else {
			/* TODO:: Need a default value */
		}
	} else {
		/* TODO:: Need a default value */
	}
	setting = config_lookup(pCFG, "start");
	if (setting) {
		STARTADDR = config_setting_get_int(setting);
	} else {
		STARTADDR = 0;
	}
	setting = config_lookup(pCFG, "debug");
	if (setting) {
		debug = config_setting_get_int(setting);
	} else {
		debug = 0;
	}
	setting = config_lookup(pCFG, "trace");
	if (setting) {
		trace = config_setting_get_int(setting);
	} else {
		trace = 0;
	}
	setting = config_lookup(pCFG, "disasm");
	if (setting) {
		DISASM = config_setting_get_int(setting);
	} else {
		DISASM = 0;
	}
	setting = config_lookup(pCFG, "panel");
	if (setting) {
		PANEL_PROCESSOR = config_setting_get_int(setting);
	} else {
		PANEL_PROCESSOR = 0;
	}

	setting = config_lookup(pCFG, "daemonize");
	if (setting) {
		DAEMON = config_setting_get_int(setting);
	} else {
		DAEMON = 0;
	}
	setting = config_lookup(pCFG, "emulatemon");
	if (setting) {
		emulatemon = config_setting_get_int(setting);
	} else {
		emulatemon = 0;
	}
	setting = config_lookup(pCFG, "floppy_image");
	if (setting) {
		tmpstr = (char *)config_setting_get_string(setting);
		if (tmpstr)
			FDD_IMAGE_NAME = strdup(tmpstr);
	}
	setting = config_lookup(pCFG, "floppy_image_access");
	if (setting) {
		tmpstr = (char *)config_setting_get_string(setting);
		if (tmpstr) {
			if(strcmp("rw",tmpstr)==0)
				FDD_IMAGE_RO = 0;
		} else {
			FDD_IMAGE_RO = 1;
		}
	} else {
		FDD_IMAGE_RO = 1;
	}

	config_destroy(pCFG);
	free(pCFG);
	CONFIG_OK = 1;      /* :TODO: No detailed checks of all dependant parameters yet */
	return(0);
}

void shutdown(int signum){
	if (debug) fprintf(debugfile,"(####) shutdown routine running\n");
	if (debug) fflush(debugfile);

	/* This works for now. All threads should terminate if this variable is set */
	CurrentCPURunMode = SHUTDOWN;

	if (sem_post(&sem_run) == -1) { /* release rum lock */
		if (debug) fprintf(debugfile,"ERROR!!! sem_post failure shutdown\n");
	}

	if (sem_post(&sem_mopc) == -1) { /* release mopc lock */
		if (debug) fprintf(debugfile,"ERROR!!! sem_post failure shutdown\n");
	}

	if (sem_post(&sem_sigthr) == -1) { /* release signal thread lock */
		if (debug) fprintf(debugfile,"ERROR!!! sem_post failure shutdown\n");
	}

	if (debug) fprintf(debugfile,"(####) shutdown routine done\n");
	if (debug) fflush(debugfile);
}

void rtc_handler (int signum){
	if (sem_post(&sem_rtc_tick) == -1) { /* release rtc tick  lock */
		if (debug) fprintf(debugfile,"ERROR!!! sem_post failure rtc_handler\n");
	}
}

void blocksignals() {
	static sigset_t   new_set;
	static sigset_t   old_set;
	sigemptyset (&new_set);
	sigemptyset (&old_set);

	/* set signals now */
	if (DAEMON) {
		sigaddset (&new_set, SIGCHLD); /* ignore child */
		sigaddset (&new_set, SIGTSTP); /* ignore tty signals */
		sigaddset (&new_set, SIGTTOU);
		sigaddset (&new_set, SIGTTIN);
	}
	sigaddset (&new_set, SIGALRM); /* ignore timer for process.. this one we will install handler for later */
	sigaddset (&new_set, SIGINT); /* kill signal we will catch in handles */
	sigaddset (&new_set, SIGHUP); /* see above */
	sigaddset (&new_set, SIGTERM); /* see above */
	pthread_sigmask (SIG_BLOCK, &new_set, &old_set);
}

void setsignalhandlers() {
	static sigset_t   new_set;
	static sigset_t   old_set;
	static struct sigaction act;
	static struct sigaction act_alrm;

	/* set up handler for SIGINT, SIGHUP, SIGTERM */
	act.sa_handler = &shutdown;
	sigemptyset (&act.sa_mask);
	sigaction (SIGINT, &act, NULL);
	sigaction (SIGHUP, &act, NULL);
	sigaction (SIGTERM, &act, NULL);
	sigemptyset (&new_set);
	sigemptyset (&old_set);
	sigaddset (&new_set, SIGINT);
	sigaddset (&new_set, SIGHUP);
	sigaddset (&new_set, SIGTERM);
	pthread_sigmask (SIG_UNBLOCK, &new_set, &old_set);

	/* set up handler for SIGALARM */
	act_alrm.sa_handler = &rtc_handler;
	sigemptyset (&act_alrm.sa_mask);
	sigaction (SIGALRM, &act_alrm, NULL);
	sigemptyset (&new_set);
	sigemptyset (&old_set);
	sigaddset (&new_set, SIGALRM);
	pthread_sigmask (SIG_UNBLOCK, &new_set, &old_set);
	return;
}

void signal_thread(){
	int s;
	setsignalhandlers();
	while ((s = sem_wait(&sem_sigthr)) == -1 && errno == EINTR) /* wait for signal thread lock to be free */
		continue; /* Restart if interrupted by handler */
}

void daemonize() {
	pid_t pid, sid;
	int i;

	if(getppid()==1) return; /* already a daemon */

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		/* Log any failure */
		exit(EXIT_FAILURE);
	}
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* child (daemon) continues */
	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		/* Log any failure */
		exit(EXIT_FAILURE);
	}

	/* Handle standard file descriptors */
	for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
	i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standard I/O */

	CONSOLE_IS_SOCKET = 1;
}

pthread_t add_thread(void *funcpointer, bool is_jointype){
	struct ThreadChain *tc_elem;

	tc_elem=AddThreadChain();
	pthread_attr_init(&tc_elem->tattr);
	if (is_jointype){
		pthread_attr_setdetachstate(&tc_elem->tattr,PTHREAD_CREATE_JOINABLE);
		tc_elem->tk = JOIN;
	} else {
		pthread_attr_setdetachstate(&tc_elem->tattr,PTHREAD_CREATE_DETACHED);
		tc_elem->tk = CANCEL;
	}
	pthread_create(&tc_elem->thread, &tc_elem->tattr, funcpointer, NULL );
	return(tc_elem->thread);
}

void start_threads(){
	pthread_t thread_id;
	/* CPU Thread */
	thread_id = add_thread(&cpu_thread,1);
	if (debug) fprintf(debugfile,"Added thread id: %d as cpu_thread\n",(int)thread_id);
	if (debug) fflush(debugfile);

	thread_id = add_thread(&signal_thread,1);
	if (debug) fprintf(debugfile,"Added thread id: %d as signal_thread\n",(int)thread_id);
	if (debug) fflush(debugfile);

	thread_id = add_thread(&mopc_thread,1);
	if (debug) fprintf(debugfile,"Added thread id: %d as mopc_thread\n",(int)thread_id);
	if (debug) fflush(debugfile);

	thread_id = add_thread(&rtc_20,0);
	if (debug) fprintf(debugfile,"Added thread id: %d as rtc_20\n",(int)thread_id);
	if (debug) fflush(debugfile);

	thread_id = add_thread(&panel_thread,0);
	if (debug) fprintf(debugfile,"Added thread id: %d as panel_thread\n",(int)thread_id);
	if (debug) fflush(debugfile);

	thread_id = add_thread(&floppy_thread,0);
	if (debug) fprintf(debugfile,"Added thread id: %d as floppy_thread\n",(int)thread_id);
	if (debug) fflush(debugfile);

	if(CONSOLE_IS_SOCKET){
		thread_id = add_thread(&console_socket_thread,0);
	} else {
		thread_id = add_thread(&console_stdio_thread,0);
	}

	if(PANEL_PROCESSOR){
		thread_id = add_thread(&panel_processor_thread,0);
		if (debug) fprintf(debugfile,"Added thread id: %d as panel_processor_thread\n",(int)thread_id);
		if (debug) fflush(debugfile);
	}

	if (debug) fprintf(debugfile,"Added thread id: %d as console_socket/stdio_thread\n",(int)thread_id);
	if (debug) fflush(debugfile);
}

void stop_threads(){
	if (debug) fprintf(debugfile,"REMOVE thread id: %d\n",(int)gThreadChain->thread);
	if (debug) fflush(debugfile);
	RemThreadChain(gThreadChain);
	while (gThreadChain) {
		if (debug) fprintf(debugfile,"IN the kill while for threads with thread id: %d\n",(int)gThreadChain->thread);
		if (debug) fflush(debugfile);

		switch(gThreadChain->tk) {
		case (JOIN):
			pthread_join(gThreadChain->thread,NULL);
			RemThreadChain(gThreadChain);
			break;
		case (CANCEL):
			pthread_cancel(gThreadChain->thread);
			RemThreadChain(gThreadChain);
			break;
		default:	/* IGNORE */
			RemThreadChain(gThreadChain);
			break;
		}
	}
}

void setup_cpu(){
	/* initialize an empty register set */
	gReg=calloc(1,sizeof(struct CpuRegs));
	/* initialize an empty pagetable */
	gPT=calloc(1,sizeof(union NewPT));
	/* Initialize IO handler functions */
	Setup_IO_Handlers();
	/* initialize floppy drive data structures */
	floppy_init();

	setbit(_STS,_O,1);
	setbit_STS_MSB(_N100,1);
	gCSR = 1<<2;	/* this bit sets the cache as not available */

	/* Initialise Ident List  pointer to null */
	gIdentChain = 0;

	/* Set cpu as running for now. Probably should depend on settings */
	CurrentCPURunMode = RUN;
	instr_counter=0;

}

void program_load(){
	switch(BootType){
	case BP:
		bp_load();
		gPC = (CONFIG_OK) ? STARTADDR : 0;
		break;
	case BPUN:
		bpun_load();
		gPC = (CONFIG_OK) ? STARTADDR : 0;
		break;
	case FLOPPY:
		sectorread(0,0,1,(ushort *)&VolatileMemory);
		gPC = 0;
		break;
	}
}
