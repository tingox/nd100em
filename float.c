/*
 * nd100em - ND100 Virtual Machine
 *
 * Copyright (c) 2008 Zdravko
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
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>
#include "nd100.h"

int MUL32 (unsigned long int* a, unsigned long int* b, unsigned long int* r);
int NDFloat_Div (unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r);
int NDFloat_Mul (unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r);
int NDFloat_Add (unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r);
int NDFloat_Sub (unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r);
long double pow2l(int i);

extern double instr_counter;
extern int debug;
extern FILE *debugfile;
extern struct CpuRegs *gReg;
void DoNLZ (char scaling);
void DoDNZ (char scaling);
extern void setbit(ushort regnum, ushort stsbit, char val);

/* routine to sort out a missing powl in freebsd */
long double pow2l(int i){
	long double r;
#if !(defined __FreeBSD__ || defined BSD )
	r=powl(2,(long double)i);
#else
#endif
	return r;
}

/*
 * int MUL32(unsigned long int* a, unsigned long int* b, unsigned long int* r)
 * Multiplies two 32 bit numbers. The result is accomodated in 64 bits(Big Endian arrangement).
 * Parameters a,b - input operands; r - pointer to MSword of the 64 bit result
 * The return value is dummy(reserved for future development).
 */
int MUL32(unsigned long int* a, unsigned long int* b, unsigned long int* r) {
	unsigned short int a_l,a_m,b_l,b_m;
	unsigned long int temp,r_l,r_m;

	r_m = 0;
	r_l = 0;

	if((0 != *a) && (0 != *b)) {
		a_m = *a>>16;
		a_l = (*a & (((unsigned long int)1<<16) - 1));
		b_m = *b>>16;
		b_l = (*b & (((unsigned long int)1<<16) - 1));

		if((0 != a_m) && (0 != b_m))
			r_m = a_m * b_m;
		if((0 != a_l) && (0 != b_l))
			r_l = a_l * b_l;
		if((0 != a_l) && (0 != b_m)) {
			temp = a_l * b_m;
			r_m = r_m + (temp>>16) + ((r_l > (~(unsigned long int)0 - (temp<<16))) ? 1 : 0);
			r_l += temp<<16;
		}
		if((0 != a_m) && (0 != b_l)) {
			temp = a_m * b_l;
			r_m = r_m + (temp>>16) + ((r_l > (~(unsigned long int)0 - (temp<<16))) ? 1 : 0);
			r_l += temp<<16;
		}
	} else {
		r_m = 0;
		r_l = 0;
	}

	r[0] = r_m;
	r[1] = r_l;

	return 0;
}

/*
 * int NDFloat_Div(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r)
 * Emulates ND 48 bit float divide.
 * Parameters p_a - quotient; p_a - divisor; p_r - dividend
 * All the parameters are organized as arrays of 3, 16 bit elements.
 * The first element contains sign and exponent(reg T), the second MSword of mantisa(reg A), the last LSword of mantisa(reg D).
 * Look in "ND-100 Reference Manual, ND-06.014.02, Revision A" Section 3.1.2.5 for details of 48 bit float format.
 * Return value is used to indicate some of exeptions(overflow, underflow etc.)
 */
int old_NDFloat_Div(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r) {
	bool	sign_r;
	signed long int exp_r;
	unsigned long int a, b, r[3], r_sig;

	/* At first we get the sign of the result */
	sign_r = (p_a[0] & (1<<15)) ^ (p_b[0] & (1<<15));

	/* Shortcut if we have zero divident */
	if((0 == p_a[0]) && (0 == p_a[1]) && (0 == p_a[2])) {
		p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0);
		p_r[1] = 0;
		p_r[2] = 0;
		return 0;
	}

	/* Guard against division by zero */
	if((0 == p_b[0]) && (0 == p_b[1]) && (0 == p_b[2])) {
		/* :TODO: Division by zero!!! Activate CPU trap(level 14). */
		return -1;
	}

	/* Now we get the result exponent */
	exp_r = (p_a[0] & ~(1<<15)) - (p_b[0] & ~(1<<15)) + 1;

	/*
	 * Here we divide the significants(mantisas)
	 * The result is get in 64 bit. This may be oversized but will be corrected in the future.
	 * Then the result gets normilized and truncated to 32 bits and the exponent corrected
	 */
	a = ((unsigned long int)p_a[1])<<16 | p_a[2];	/* the host machine is Little Endian */
	b = ((unsigned long int)p_b[1])<<16 | p_b[2];	/* the host machine is Little Endian */
	/* Here we divide the two 32 bit long significants and get the 64 bit long rezult */
	int msb = 0;
	int bit = 63;
	r[0] = 0;
	r[1] = 0;
	goto start;
	do {
		msb = a & (1<<31);
		a <<= 1;
	start:
		if(msb || (a >= b)) {
			a -= b;
			if(bit > 31)
				r[0] |= (1<<(bit - 32));
			else
				r[1] |= (1<<bit);
		}
	} while(--bit >= 0);

	/* Here we normalize the rezult and correct the exponent */
	if(0 != r[0]) {
		int i;
		for(i = 32; i > 0; i--) {
			if(r[0] >= ((unsigned long int)1<<(i - 1)))
				break;
		}
		exp_r -= 32 - i;
		r_sig = (r[0]<<(32 - i)) | (r[1]>>i) | (((r[1] & ((1<<i) - 1)) > 0) ? 1 : 0);
		p_r[1] = r_sig>>16;
		p_r[2] = r_sig & (((unsigned long int)1<<16) - 1);
	} else {
		int i;
		for(i = 32; i > 0; i--) {
			if(r[1] >= ((unsigned long int)1<<(i - 1)))
				break;
		}
		exp_r -= 32 + 32 - i;
		r_sig = (r[1]<<(32 - i));
		p_r[1] = r_sig>>16;
		p_r[2] = r_sig & (((unsigned long int)1<<16) - 1);
	}

	/* Finaly we check the exponent for over/underflow */
	if(exp_r > ((1<<14) - 1)) {
		/* Overflow */
		p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0) | 0x7FFF;
		p_r[1] = 0xFFFF;
		p_r[2] = 0xFFFF;
		return -1;
	}
	if(exp_r < -(1<<14)) {
		/* Underflow */
		p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0);
		p_r[1] = 0;
		p_r[2] = 0;
		return -1;
	}
	/* build the exponent in ND 48 bit float format */
	p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0) | ((unsigned)exp_r + ((unsigned short int)1<<14));
	return 0;
}


/*
 * int NDFloat_Mul(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r)
 * Emulates ND 48 bit float multiply.
 * Parameters p_a - input operand; p_a - input operand; p_r - result
 * All the parameters are organized as arrays of 3, 16 bit elements.
 * The first element contains sign and exponent(reg T), the second MSword of mantisa(reg A), the last LSword of mantisa(reg D).
 * Look in "ND-100 Reference Manual, ND-06.014.02, Revision A" Section 3.1.2.5 for details of 48 bit float format.
 * Return value is used to indicate some of exeptions(overflow, underflow etc.)
 */
int old_NDFloat_Mul(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r) {
	bool sign_r;
	signed int exp_a, exp_b, exp_r;
	unsigned long int a, b, r_sig[2];
	unsigned long int i,temp,r, shift;
	bool inexact = false;


	/* First we get the result */
	sign_r = (p_a[0] & (1<<15)) ^ (p_b[0] & (1<<15));

	/* Shortcut if anyone of the arguments is zero */
	if(((0 == p_a[0]) && (0 == p_a[1]) && (0 == p_a[2])) || ((0 == p_b[0]) && (0 == p_b[1]) && (0 == p_b[2]))) {
		p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0);
		p_r[1] = 0;
		p_r[2] = 0;
		return 0;
	}

	/* Get the result exponent */
	exp_a = (p_a[0] & ~(1<<15)) - (1<<14) - 1;
	exp_b = (p_b[0] & ~(1<<15)) - (1<<14) - 1;
	exp_r = exp_a + exp_b;

	/* Multiplie the significants. The result is 64 bits long. */
	a = ((unsigned long int)p_a[1])<<16 | p_a[2];	/* the host machine is Little Endian */
	b = ((unsigned long int)p_b[1])<<16 | p_b[2];	/* the host machine is Little Endian */
	MUL32(&a,&b,r_sig);

	/* Normalize the rezult and truncate to 32 bits */
	/* Correct the exponent accordingly */
	if(0 == r_sig[0]) {	/* :NOTE: If we do not have denormalized numbers so this case should never be met! */
		temp = r_sig[1];
		if(0 != temp) {
			for(i = 32; i > 0; i--) {
				if(temp >= ((unsigned long int)1<<(i - 1)))
					break;
			}
			r = temp << (32 - i);
			shift = 32 + 32 - i;
		} else {
			/* Zero */
			p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0);
			p_r[1] = 0;
			p_r[2] = 0;
			return -1; 
		}
		exp_r -= shift;
	} else {
		temp = r_sig[0];
		for(i = 32; i > 0; i--) {
			if(temp >= ((unsigned long int)1<<(i - 1)))
				break;
		}
		exp_r += 32 - i;
		if((r_sig[1] & (((unsigned long int)1<<i) - 1)) > 0)
			inexact = true;
		r = (r_sig[0]<<(32 - i)) | (r_sig[1]>>i);
	}

	/* Checks for over/underflow */
	if(exp_r > ((1<<14) - 1)) {
		/* Overflow */
		p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0) | 0x7FFF;
		p_r[1] = 0xFFFF;
		p_r[2] = 0xFFFF;
		return -1;
	}
	if(exp_r < -(1<<14)) {
		/* :TODO: Underflow */
		p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0);
		p_r[1] = 0;
		p_r[2] = 0;
		return -1;
	}

	/* build the result in ND 48 bit float format */
	p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0) | ((unsigned)exp_r + ((unsigned short int)1<<14));
	p_r[1] = r >> 16;
	p_r[2] = (r & 0xFFFF)  | ((true == inexact) ? 1 : 0);

	return 0;
}

/*
 * int NDFloat_Add(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r)
 * Emulates ND 48 bit float addition.
 * Parameters p_a - input operand; p_a - input operand; p_r - result
 * All the parameters are organized as arrays of 3, 16 bit elements.
 * The first element contains sign and exponent(reg T), the second MSword of mantisa(reg A), the last LSword of mantisa(reg D).
 * Look in "ND-100 Reference Manual, ND-06.014.02, Revision A" Section 3.1.2.5 for details of 48 bit float format.
 * Return value is used to indicate some of exeptions(overflow, underflow etc.)
 * Note the ND peculiarities, ND sets lowest bit if it cannot contain result exactly,
 * but does it before it handles a possible carry, thus possibly shifting that bit out later.
 */

int NDFloat_Add(ushort* p_a,ushort* p_b,ushort* p_r) {
	bool sign_a,sign_b,sign_r;
	bool is_exact, do_nlz;
	int exp_a, exp_b, exp_r;
	unsigned  int delta_e;
	unsigned int a, b, r;

	bool has_carry = false;

	/* Null sign bit, subtract offset */
	exp_a = (p_a[0] & ~(1<<15)) - (1<<14);
	exp_b = (p_b[0] & ~(1<<15)) - (1<<14);

	/* Pluck out sign bit */
	sign_a = (p_a[0] & (1<<15)) ? true : false;
	sign_b = (p_b[0] & (1<<15)) ? true : false;

	/* Convert to unsigned 32 bit numbers for easy handling */
	/* Host machine is little endian */
	a = ((p_a[1] & 0xffff)<<16) | (p_a[2] & 0xffff);
	b = ((p_b[1] & 0xffff)<<16) | (p_b[2] & 0xffff);

	is_exact = true;

	if(exp_a > exp_b) {
		delta_e = exp_a - exp_b;
		/* We will shift b right by delta_e, so check if we will shift out any ones first */
		if (b & (~(0xffffffff << delta_e)))
			is_exact = false;	/* Yes, take note of it */
		b = b >> delta_e;
		exp_r = exp_a;
	} else	if(exp_b > exp_a) {
		delta_e = exp_b - exp_a;
		/* We will shift a right by delta_e, so check if we will shift out any ones first */
		if (a & (~(0xffffffff << delta_e)))
			is_exact = false;	/* Yes, take note of it */
		a = a >> delta_e;
		exp_r= exp_b;
	} else {
		delta_e = 0;
		exp_r= exp_a;
	}

	if(sign_a == sign_b) { /* Same sign so addition no matter what */
		sign_r =  (sign_a) ? true : false;
		r = a + b;

		if ( a > UINT_MAX - b)	/* a + b would overflow */
			has_carry = true;
	} else {	/* Different signs, so we subtract the smaller number and flip sign depending on which is which */
		if (a >= b) {
			r = a - b;
			sign_r = (sign_a ) ?  true : false ;
		} else {
			r = b - a;
			sign_r = (sign_b ) ?  true : false ;
		}
	}
	r = (is_exact) ? r : r | 0x01;

	if (has_carry) {
		exp_r++;
		r = (r >> 1) | (0x01 << 31);	/* Adjust result */
	}

	/* Normalize result */
	if ( r != 0 ) {
		do {
			do_nlz = (!(r  & 0x80000000)) ? true : false; /* Highest bit is zero.. adjust */
			if (do_nlz) {
				r = r << 1;
				exp_r--;
			}
		} while (do_nlz == true);
	} else {
		exp_r = 0;
		sign_r = false;
	}

	if(exp_r > ((1<<14) - 1)) {
		/* Overflow */
		p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0) | 0x7FFF;
		p_r[1] = 0xFFFF;
		p_r[2] = 0xFFFF;
		return -2;
	}
	if(exp_r < -(1<<14)) {
		/* :TODO: Underflow */
		p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0);
		p_r[1] = 0;
		p_r[2] = 0;
		return -3;
	}

	p_r[0] = ((sign_r) ? ((unsigned short int)1<<15) : 0) | ((unsigned)exp_r + ((unsigned short int)1<<14));
	p_r[1] = r >> 16;
	p_r[2] = (r & 0xFFFF);

	/* Make ND standardized 0 */
	if ((p_r[0]== 040000) & (p_r[1]== 000000) & (p_r[2]== 000000))
		p_r[0]= 000000;

	if (is_exact)
		return 0;
	else
		return -1;
}

/*
 *	Normalize floating point number.
 *	Converts an integer in register A to a floating point number in {T,A,D} according to scaling factor.
 *	Input parameters: a scaling factor.
 */
void old_DoNLZ (char scaling) {
	ushort reg_a = gA;
	int i = 1;
	for(; i < 16; i++) {
		if(reg_a <= (unsigned)((1<<i) - 1))
			break;
	}
	gT = (scaling - i) + ((ushort)1<<14);
	gA = reg_a << (16 - i);
	gD = 0;
}

/*
 *	Denormalize floating point number.
 *	Converts a floating point number in {T,A,D} to an integer in register A according to scaling factor.
 *	Input parameters: a scaling factor.
 *	:NOTE: There are some remarks in the manual saying that there are some cases when
 *	this instruction gives erroneus results. Our implementation is much more precise than the original hardware.
 */
void old_DoDNZ(char scaling) {
	sshort exp = (gT & ~((ushort)1<<15)) - ((ushort)1<<14);
	ulong mantissa = ((unsigned long)gA<<16) | gD;
	ushort reg_a;
	int shift = scaling - 16 + exp;
	if(shift > 0) {
		if(shift <= 31)
			reg_a = (ushort)(mantissa >> (32 - shift));
		else {
			/* Overflow */
			setbit(_STS,_Z,1);
			gT = 0;
			gA = 0;
			gD = 0;
			return;
		}
	}
	else if(shift < 0)
		reg_a = (ushort)(mantissa >> (~((unsigned)shift) + 1 + 16));
	else
		reg_a = gA>>1;
	gA = (gT & ((ushort)1<<15)) | reg_a;
	gT = 0;
	gD = 0;
}

/*
 * ld_to_ndmant
 *
 * converts a long double normalised mantissa
 * to ND100 mantissa 32 bits
 */
unsigned int ld_to_ndmant(long double mant){
	int i,j;
	long double k,l;
	unsigned int res =0;
	j=31;
	i=-1;
	l=0; k=0;
	for(i=-1; i>=-32;i--) {
		l=k+pow2l(i);
		if (l>mant) { /* 0 in this bit pos */
			res &= ~(1<<j);
		} else { /* 1 in this bit pos */
			res |= 1<<j;
			k=l;
		}
		if (debug) fprintf(debugfile,"ld_to_ndmant: loop: i:%d j:%d l:%.19Lf mant:%.19Lf k:%.19Lf res:%08x\n",i,j,l,mant,k,res);
		j--;
	}
	return res;
}

/*
 * ndmant_to_ld
 *
 * converts a ND100 mantissa to a long double
 *
 */
void ndmant_to_ld(unsigned int ndmant, long double *val) {
	int i,j;
	long double k,l;
	j=31;
	k=0;
	for(i=-1; i>=-32;i--) {
		l=((ndmant>>j) &0x01)* pow2l(i);
		k+=l;
		j--;
	}
	*val = k;
	return;
}


/*
 * int NDFloat_Div(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r)
 * Emulates ND 48 bit float divide.
 * Parameters p_a - quotient; p_a - divisor; p_r - dividend
 * All the parameters are organized as arrays of 3, 16 bit elements.
 * The first element contains sign and exponent(reg T), the second MSword of mantisa(reg A), the last LSword of mantisa(reg D).
 * Look in "ND-100 Reference Manual, ND-06.014.02, Revision A" Section 3.1.2.5 for details of 48 bit float format.
 * Return value is used to indicate some of exeptions(overflow, underflow etc.)
 */
int NDFloat_Div(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r) {
	long double a,b,r;
	int exp;
	long double mant;
	long double k;
	unsigned int res;

	bool isneg_a = (p_a[0] & 0x8000)>> 15;
	bool isneg_b = (p_b[0] & 0x8000)>> 15;
	bool isneg_r = false;
	if (debug) fprintf(debugfile,"FDV: p_a[0]:%06o p_a[1]:%06o p_a[2]:%06o\n",(int)p_a[0],(int)p_a[1],(int)p_a[2]);
	if (debug) fprintf(debugfile,"FDV: p_b[0]:%06o p_b[1]:%06o p_b[2]:%06o\n",(int)p_b[0],(int)p_b[1],(int)p_b[2]);

	sshort exp_a = p_a[0] & 0x7fff;
	exp_a = exp_a - 16384; /* offset for ND100 exp */
	ndmant_to_ld(((unsigned int)p_a[1]<<16 | p_a[2]), &k);
	a = k * pow2l((int)exp_a);
	a = (isneg_a) ? -a : a;
	if (debug) fprintf(debugfile,"FDV: a:%Lf\n",a);

	sshort exp_b = p_b[0] & 0x7fff;
	exp_b = exp_b - 16384; /* offset for ND100 exp */
	ndmant_to_ld(((unsigned int)p_b[1]<<16 | p_b[2]), &k);
	b = k * pow2l((int)exp_b);
	b = (isneg_b) ? -b : b;
	if (debug) fprintf(debugfile,"FDV: b:%Lf\n",b);

	if ((p_b[1]==0) && (p_b[2]==0)) { /*division by zero */
		/* TODO:: Mostly guesswork for now */
		p_r[0] = 0x7fff;
		p_r[1] = 0xffff;
		p_r[2] = 0xffff;
		return(1);
	} else {
		r= a / b;
	}
	if (debug) fprintf(debugfile,"FDV: r:%Lf instr_counter=%d\n",r,(int)instr_counter);
	mant = frexpl(r,&exp); /* normalise */
	if (mant < 0) {/* negative number */
		mant = -mant;
		isneg_r = true;
	}

	res = ld_to_ndmant(mant);

	if (debug) fprintf(debugfile,"FDV: res:%d\n",(int)res);
	p_r[0] = (ushort) 16384 + exp;
	p_r[0] |= (isneg_r) ? 1<<15 : 0;
	p_r[1] = (res >> 16) & 0xffff;
	p_r[2] = res & 0xffff;
	if ((p_r[2] == 0) &&(p_r[1] == 0)) /* result is 0 */
		p_r[0] = 0; /* set exp and sign -> 0 too */
	if (debug) fprintf(debugfile,"FDV: p_r[0]:%06o p_r[1]:%06o p_r[2]:%06o\n",(int)p_r[0],(int)p_r[1],(int)p_r[2]);
	if (debug) fprintf(debugfile,"FDV: mant:%Lf exp:%d\n",mant,exp);
	if (debug) fprintf(debugfile,"FDV: ******************************\n");
	return(0);
}

/*
 * int NDFloat_Mul(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r)
 * Emulates ND 48 bit float multiply.
 * Parameters p_a - input operand; p_a - input operand; p_r - result
 * All the parameters are organized as arrays of 3, 16 bit elements.
 * The first element contains sign and exponent(reg T), the second MSword of mantisa(reg A), the last LSword of mantisa(reg D).
 * Look in "ND-100 Reference Manual, ND-06.014.02, Revision A" Section 3.1.2.5 for details of 48 bit float format.
 * Return value is used to indicate some of exeptions(overflow, underflow etc.)
 */
int NDFloat_Mul(unsigned short int* p_a,unsigned short int* p_b,unsigned short int* p_r) {
	long double a,b,r;
	int exp;
	long double mant;
	long double k;
	unsigned int res;

	bool isneg_a = (p_a[0] & 0x8000)>> 15;
	bool isneg_b = (p_b[0] & 0x8000)>> 15;
	bool isneg_r = false;
	if (debug) fprintf(debugfile,"FMU: p_a[0]:%06o p_a[1]:%06o p_a[2]:%06o\n",(int)p_a[0],(int)p_a[1],(int)p_a[2]);
	if (debug) fprintf(debugfile,"FMU: p_b[0]:%06o p_b[1]:%06o p_b[2]:%06o\n",(int)p_b[0],(int)p_b[1],(int)p_b[2]);

	sshort exp_a = p_a[0] & 0x7fff;
	exp_a = exp_a - 16384; /* offset for ND100 exp */
	ndmant_to_ld(((unsigned int)p_a[1]<<16 | p_a[2]), &k);
	a = k * pow2l((int)exp_a);
	a = (isneg_a) ? -a : a;
	if (debug) fprintf(debugfile,"FMU: a:%Lf\n",a);

	sshort exp_b = p_b[0] & 0x7fff;
	exp_b = exp_b - 16384; /* offset for ND100 exp */
	ndmant_to_ld(((unsigned int)p_b[1]<<16 | p_b[2]), &k);
	b = k * pow2l((int)exp_b);
	b = (isneg_b) ? -b : b;
	if (debug) fprintf(debugfile,"FMU: b:%Lf\n",b);

	r= a * b;
	if (debug) fprintf(debugfile,"FMU: r:%Lf instr_counter=%d\n",r,(int)instr_counter);
	mant = frexpl(r,&exp); /* normalise */
	if (mant < 0) {/* negative number */
		mant = -mant;
		isneg_r = true;
	}

	res = ld_to_ndmant(mant);

	if (debug) fprintf(debugfile,"FMU: res:%d\n",(int)res);
	p_r[0] = (ushort) 16384 + exp;
	p_r[0] |= (isneg_r) ? 1<<15 : 0;
	p_r[1] = (res >> 16) & 0xffff;
	p_r[2] = res & 0xffff;
	if ((p_r[2] == 0) &&(p_r[1] == 0)) /* result is 0 */
		p_r[0] = 0; /* set exp and sign -> 0 too */
	if (debug) fprintf(debugfile,"FMU: p_r[0]:%06o p_r[1]:%06o p_r[2]:%06o\n",(int)p_r[0],(int)p_r[1],(int)p_r[2]);
	if (debug) fprintf(debugfile,"FMU: mant:%Lf exp:%d\n",mant,exp);
	if (debug) fprintf(debugfile,"FMU: ******************************\n");
	return(0);
}

int NDFloat_Sub(ushort* p_a, ushort* p_b,ushort* p_r) {
	long double a,b,r;
	int exp;
	long double mant;
	long double k;
	unsigned int res;

	bool isneg_a = (p_a[0] & 0x8000)>> 15;
	bool isneg_b = (p_b[0] & 0x8000)>> 15;
	bool isneg_r = false;
	if (debug) fprintf(debugfile,"FSB: p_a[0]:%06o p_a[1]:%06o p_a[2]:%06o\n",(int)p_a[0],(int)p_a[1],(int)p_a[2]);
	if (debug) fprintf(debugfile,"FSB: p_b[0]:%06o p_b[1]:%06o p_b[2]:%06o\n",(int)p_b[0],(int)p_b[1],(int)p_b[2]);

	sshort exp_a = p_a[0] & 0x7fff;
	exp_a = exp_a - 16384; /* offset for ND100 exp */
	ndmant_to_ld(((unsigned int)p_a[1]<<16 | p_a[2]), &k);
	a = k * pow2l((int)exp_a);
	a = (isneg_a) ? -a : a;
	if (debug) fprintf(debugfile,"FSB: a:%Lf\n",a);

	sshort exp_b = p_b[0] & 0x7fff;
	exp_b = exp_b - 16384; /* offset for ND100 exp */
	ndmant_to_ld(((unsigned int)p_b[1]<<16 | p_b[2]), &k);
	b = k * pow2l((int)exp_b);
	b = (isneg_b) ? -b : b;
	if (debug) fprintf(debugfile,"FSB: b:%Lf\n",b);

	r= a - b;
	if (debug) fprintf(debugfile,"FSB: r:%Lf instr_counter=%d\n",r,(int)instr_counter);
	mant = frexpl(r,&exp); /* normalise */
	if (mant < 0) {/* negative number */
		mant = -mant;
		isneg_r = true;
	}

	res = ld_to_ndmant(mant);

	if (debug) fprintf(debugfile,"FSB: res:%d\n",(int)res);
	p_r[0] = (ushort) 16384 + exp;
	p_r[0] |= (isneg_r) ? 1<<15 : 0;
	p_r[1] = (res >> 16) & 0xffff;
	p_r[2] = res & 0xffff;
	if ((p_r[2] == 0) &&(p_r[1] == 0)) /* result is 0 */
		p_r[0] = 0; /* set exp and sign -> 0 too */
	if (debug) fprintf(debugfile,"FSB: p_r[0]:%06o p_r[1]:%06o p_r[2]:%06o\n",(int)p_r[0],(int)p_r[1],(int)p_r[2]);
	if (debug) fprintf(debugfile,"FSB: mant:%Lf exp:%d\n",mant,exp);
	if (debug) fprintf(debugfile,"FSB: ******************************\n");
	return(0);
}

/*
 *	Normalize floating point number.
 *	Converts an integer in register A to a floating point number in {T,A,D} according to scaling factor.
 *	Input parameters: a scaling factor.
 *	NOTE: D will be cleared as per manual.
 */
void DoNLZ (char scaling) {
	if (debug) fprintf(debugfile,"DoNLZ: A:%06o scaling:%d instr_counter=%d\n",gA,(int)scaling,(int)instr_counter);
	if (gA==0) { /* special case, return with TAD=0 */
		gT=0;
		gA=0;
		gD=0;
		return;
	}
	bool isneg = (gA & 0x8000)>> 15;
	int e,val;
	long double mantissa,x;
	int i,j;
	long double k,l;
	ushort res = 0;
	e = (int)scaling - 16; /* adjust offset, +16 = 2^0 */
	val = (int)(sshort)gA;
	x=(long double)val * pow2l(e);
	mantissa = frexpl(x,&e); /* normalise */
	if (mantissa < 0) /* negative number, ignore since we got sign bit already */
		mantissa = -mantissa;
	if (debug) fprintf(debugfile,"DoNLZ: x:%Lf val(lf):%Lf val(int):%d\n",x,(long double)val,val);
	if (debug) fprintf(debugfile,"DoNLZ: isneg:%d exp:%d mantissa:%08x\n",(int)isneg,e,(int)mantissa);
	if (debug) fprintf(debugfile,"DoNLZ: long double mantissa:%Lf\n",mantissa);
	j=15;
	i=-1;
	l=0; k=0;
	for(i=-1; i>-16;i--) {
		l=k+pow2l(i);
		if (l>mantissa) { /* 0 in this bit pos */
			res &= ~(1<<j);
		} else { /* 1 in this bit pos */
			res |= 1<<j;
			k=l;
		}
		j--;
	}
	if (debug) fprintf(debugfile,"DoNLZ: res:%d\n",(int)res);
	gT = (ushort) 16384 + e;
	gT |= (isneg) ? 1<<15 : 0;
	gA = res;
	gD = 0;
	if (debug) fprintf(debugfile,"DoNLZ: T:%06o A:%06o D:%06o\n",gT,gA,gD);
	if (debug) fprintf(debugfile,"DoNLZ: ******************************\n");
}

/*
 *	Denormalize floating point number.
 *	Converts a floating point number in {T,A,D} to an integer in register A according to scaling factor.
 *	Input parameters: a scaling factor.
 *	:NOTE: There are some remarks in the manual saying that there are some cases when
 *	this instruction gives erroneus results. Our implementation is much more precise than the original hardware.
 */
void DoDNZ(char scaling) {
	if (debug) fprintf(debugfile,"DoDNZ: T:%06o A:%06o D:%06o scaling:%d\n",gT,gA,gD,(int)scaling);
	bool isneg = (gT & 0x8000)>> 15;
	sshort exp = gT & 0x7fff;
	exp = exp - 16384; /* offset for ND100 exp */
	ulong mantissa = ((unsigned long)gA<<16) | gD;
	if (debug) fprintf(debugfile,"DoDNZ: isneg:%d exp:%d mantissa:%08x\n",(int)isneg,(int)exp,(int)mantissa);
	int j,k;
	long double i,t;
	k=31;
	i=0;
	for(j=-1; j>-32;j--) {
		t=((mantissa>>k) &0x01)* pow2l(j);
		i+=t;
		k--;
	}
	i= i* pow2l((int)exp);
	i = (isneg) ? -i : i;
	if (debug) fprintf(debugfile,"DoDNZ: long double i:%Lf\n",i);
	/* ok we now have the ND float in long double format */
	exp= scaling + 16;
	if (debug) fprintf(debugfile,"DoDNZ: exp:%d\n",(int)exp);
	i = i * pow2l((int)exp);
	i = truncl(i);
	gA= (sshort) i;
	if (debug) fprintf(debugfile,"DoDNZ: gA:%06o\n",gA);
	if (debug) fprintf(debugfile,"DoDNZ: ******************************\n");
	j= (int) i;
	if (abs(j) > 32767)	/* Overflow */
		setbit(_STS,_Z,1);
	gT=0;
	gD=0;
}
