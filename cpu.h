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

/* NOTE!!! ND100 seems to be big endian + word based */
/* take care with that since intel among others is little endian byte based */


/* GLOBAL VARS */


char *regn[] = {"S","D","P","B","L","A","T","X","U0","U1"};
char *regn_w[] = {"DS","DD","DP","DB","DL","DA","DT","DX"};

char *intregn_r[] = {"PANS","STS","OPR","PGS","PVL","IIC","PID","PIE","CSR","ACTL", "ALD" ,"PES","PGC","PEA","16","17"};
char *intregn_w[] = {"PANC","STS","LMP","PCR", "4", "IIE","PID","PIE","CCL","LCIL","UCILR","13", "14", "15" ,"16","17"};

char *relmode_str[] ={"",",B ","I ","I ,B ",",X ",",X ,B ","I ,X ","I ,B ,X "};
char *shtype_str[] ={"","ROT ","ZIN ","LIN "};

char *skiptype_str[] = {"EQL","GEQ","GRE","MGRE","UEQ","LSS","LST","MLST"};
char *skipregn_dst[] = {"0","DD","DP","DB","DL","DA","DT","DX"};
char *skipregn_src[] = {"0","SD","SP","SB","SL","SA","ST","SX"};

char *bopstsbit_str[] = {"SSPTM","SSTG","SSK","SSZ","SSQ","SSO","SSC","SSM","","","","","","","",""};

char *bop_str[] = {"BSET ZRO","BSET ONE","BSET BCM","BSET BAC","BSKP ZRO","BSKP ONE",
		   "BSKP BCM","BSKP BAC","BSTC","BSTA","BLDC","BLDA","BANC","BAND","BORC","BORA"};


extern FILE *tracefile;
extern int trace;
extern int DISASM;

extern FILE *debugfile;
extern int debug;

extern int emulatemon;


/*************************************************/
/* NEW ORGANIZATION OF MEMORY AND REGISTERS!!    */
/*************************************************/

/* NOTE: Memory part not implemented yet!! */

_NDRAM_		VolatileMemory;
_NDPT_		PageTable;
_RUNMODE_	CurrentCPURunMode;
_CPUTYPE_	CurrentCPUType;

struct CpuRegs *gReg;
union NewPT *gPT;
struct MemTraceList *gMemTrace;
struct IdentChain *gIdentChain;

#define ND_Memsize	(sizeof(VolatileMemory)/sizeof(ushort))

/*
 * NEW INSTRUCTION HANDLING!!
 *
 * OK here we have it, a 64K array of function pointers for instruction execution
 * We still need to initialize it before using, that should just be needed once though.
 * Even with optimized branch table handling, this should be way faster
 * It also makes possible modifications to instruction handling at runtime, for possible
 * implementation of USER instructions etc.
 */
void (*instr_funcs[65536])(ushort);

/*************************************************/

/* We have a maximum of 64Kword device register addresses*/
unsigned short devices[65536];

unsigned short MON_RUN=1;

unsigned short MODE_RUN=1;
unsigned short MODE_OPCOM=0;

/* This variable tells us if we have the display panel option */
unsigned short PANEL_PROCESSOR=0;

void ndfunc_stz(ushort operand);
void ndfunc_sta(ushort operand);
void ndfunc_stt(ushort operand);
void ndfunc_stx(ushort operand);
void ndfunc_std(ushort operand);
void ndfunc_stf(ushort operand);
void ndfunc_stztx(ushort operand);
void ndfunc_statx(ushort operand);
void ndfunc_stdtx(ushort operand);
void ndfunc_lda(ushort operand);
void ndfunc_ldt(ushort operand);
void ndfunc_ldx(ushort operand);
void ndfunc_ldd(ushort operand);
void ndfunc_ldf(ushort operand);
void ndfunc_ldatx(ushort operand);
void ndfunc_ldxtx(ushort operand);
void ndfunc_lddtx(ushort operand);
void ndfunc_ldbtx(ushort operand);
void ndfunc_min(ushort operand);
void ndfunc_add(ushort operand);
void ndfunc_sub(ushort operand);
void ndfunc_and(ushort operand);
void ndfunc_ora(ushort operand);
void ndfunc_fad(ushort operand);
void ndfunc_fsb(ushort operand);
void ndfunc_fmu(ushort operand);
void ndfunc_fdv(ushort operand);
void ndfunc_jmp(ushort operand);
void ndfunc_geco(ushort operand);
void ndfunc_versn(ushort operand);
void ndfunc_iox(ushort operand);
void ndfunc_ioxt(ushort operand);
void ndfunc_setpt(ushort operand);
void ndfunc_clept(ushort operand);
void ndfunc_ident(ushort operand);
void ndfunc_opcom(ushort operand);
void ndfunc_iof(ushort operand);
void ndfunc_ion(ushort operand);
void ndfunc_irw(ushort operand);
void ndfunc_irr(ushort operand);
void ndfunc_exam(ushort operand);
void ndfunc_depo(ushort operand);
void ndfunc_pof(ushort operand);
void ndfunc_piof(ushort operand);
void ndfunc_pon(ushort operand);
void ndfunc_pion(ushort operand);
void ndfunc_rex(ushort operand);
void ndfunc_sex(ushort operand);
void ndfunc_aaa(ushort operand);
void ndfunc_aab(ushort operand);
void ndfunc_aat(ushort operand);
void ndfunc_aax(ushort operand);
void ndfunc_mon(ushort operand);
void ndfunc_saa(ushort operand);
void ndfunc_sab(ushort operand);
void ndfunc_sat(ushort operand);
void ndfunc_sax(ushort operand);
void ndfunc_shifts(ushort operand);
void ndfunc_nlz(ushort operand);
void ndfunc_dnz(ushort operand);
void ndfunc_srb(ushort operand);
void ndfunc_lrb(ushort operand);
void ndfunc_jap(ushort operand);
void ndfunc_jan(ushort operand);
void ndfunc_jaz(ushort operand);
void ndfunc_jaf(ushort operand);
void ndfunc_jpc(ushort operand);
void ndfunc_jnc(ushort operand);
void ndfunc_jxz(ushort operand);
void ndfunc_jxn(ushort operand);
void ndfunc_jpl(ushort operand);
void ndfunc_skp(ushort operand);
void ndfunc_bfill(ushort operand);
void ndfunc_init(ushort operand);
void ndfunc_entr(ushort operand);
void ndfunc_leave(ushort operand);
void ndfunc_eleav(ushort operand);
void ndfunc_lbyt(ushort operand);
void ndfunc_sbyt(ushort operand);
void ndfunc_mix3(ushort operand);

void OpToStr(char *opstr, ushort operand);
void do_op(unsigned short operand);
void new_regop (unsigned short operand);
void regop (unsigned short operand);
void do_skp(unsigned short operand);
void do_bops(unsigned short operand);
void compare_jumps(unsigned short operand);
void do_shift( unsigned short instr);
ushort ShiftReg(ushort reg, ushort instr);
unsigned long int ShiftDoubleReg(unsigned long int reg, ushort instr);
void meminc(char XIB, char displacement);
void add_A_mem(ushort eff_addr, bool UseAPT);
void sub_A_mem(ushort eff_addr, bool UseAPT);
void rdiv(ushort instr); 
void rmpy(ushort instr);
void mpy(ushort instr);
void do_irr(unsigned short operand);
void do_irw(unsigned short operand);
void DoSRB(ushort operand);
void DoLRB(ushort operand);
void DoMCL(ushort instr);
void DoMST(ushort instr);
void DoCLEPT();
void DoTRA(ushort instr);
void DoTRR(ushort instr);
void DoWAIT(ushort instr);
void DoEXR(ushort instr);
void DoIDENT(char priolevel);
void DoMOVB(ushort instr);
void DoMOVBF(ushort instr);
void or_A_mem (char XIB, char displacement);
void and_A_mem (char XIB, char displacement);
ushort do_add(ushort a, ushort b, ushort k);
void setreg(int r, int val);
void do_debug_txt(char *txt);
void debug_regs(void);
void setbit_STS_MSB(ushort stsbit, char val);
void AdjustSTS(ushort reg_a, ushort operand, int result);
void clrbit(unsigned short regnum, unsigned short stsbit);
void setbit(unsigned short regnum, unsigned short stsbit, char val);
unsigned short getbit(unsigned short regnum, unsigned short stsbit);
int getch (void);
char mygetc (void);
void unsetcbreak (void);
void setcbreak (void);
bool IsSkip(ushort instr);
ushort GetEffectiveAddr(ushort instr);
ushort New_GetEffectiveAddr(ushort instr, bool *use_apt);
void PT_Write(ushort value, ushort addr, ushort byte_select);
ushort PT_Read(ushort addr);
void PhysMemWrite(ushort value, ulong addr);
ushort PhysMemRead(ulong addr);
void MemoryWrite(ushort value, ushort addr, bool is_P_relative, unsigned char byte_select);
ushort MemoryRead(ushort addr, bool is_P_relative);
ushort MemoryFetch(ushort addr, bool is_P_relative);
void AddMemTrace(unsigned int addr, char whom);
void DelMemTrace();
void PrintMemTrace();
void AddIdentChain(char lvl, ushort identnum, int callerid);
void RemIdentChain(struct IdentChain * elem);
void checkPK (void);
void interrupt(ushort lvl,ushort sub);
void illegal_instr(ushort operand);
void unimplemented_instr(ushort operand);
void prefetch();
void cpu_thread();
void mopc_thread();

void Instruction_Add(int start, int stop, void *funcpointer);
void Setup_Instructions ();

extern void mon (unsigned char monnum);
extern void io_op (ushort ioadd);
extern void Setup_IO_Handlers ();
extern unsigned short extract_opcode(unsigned short instr);
extern int sectorread (char cyl, char side, char sector, unsigned short *addr);
extern void trace_step(int num,...);
extern void trace_pre(int num,...);
extern void trace_post(int num,...);
extern void trace_instr(ushort instr);
extern void trace_regs();
extern void trace_flush();
extern int mopc_in(char *chptr);
extern void mopc_out(char ch);
extern int NDFloat_Div(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r);
extern int NDFloat_Mul(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r);
extern int NDFloat_Add(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r);
extern int NDFloat_Sub(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r);
extern void DoNLZ (char scaling);
extern void DoDNZ (char scaling);
extern int mysleep(int sec, int usec);

extern void disasm_instr(ushort addr, ushort instr);
extern void disasm_exr(ushort addr, ushort instr);
extern void disasm_setlbl(ushort addr);
extern void disasm_userel(ushort addr, ushort where);
extern void disasm_set_isdata(ushort addr);

extern sem_t sem_pap;
extern struct display_panel *gPAP;
