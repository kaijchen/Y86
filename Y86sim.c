#include "lib/Y86.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum stat {
	S_AOK = 1,
	S_HLT = 2,
	S_ADR = 3,
	S_INS = 4,
};

static const char *stat_name[] = { NULL, "AOK", "HLT", "ADR", "INS" };

enum flag {
	F_OF = 0,
	F_SF = 1,
	F_ZF = 2,
};

#define setCC(cc, o, s, z)			\
	do {					\
		cc = ((((o) != 0) << F_OF)	\
		    | (((s) != 0) << F_SF)	\
		    | (((z) != 0) << F_ZF));	\
	} while (0)

#define getCC(cc, o, s, z)			\
	do {					\
		o = ((cc >> F_OF) & 1);		\
		s = ((cc >> F_SF) & 1);		\
		z = ((cc >> F_ZF) & 1);		\
	} while (0)

#define MAXMEM 4000

static byte M[MAXMEM], origM[MAXMEM];
static val_t PC, CC, R[8];
static unsigned int Step;
static enum stat Stat;

static int cond(ifun_t ifun)
{
	int of, sf, zf;

	getCC(CC, of, sf, zf);
	switch (ifun) {
	case C_LE:
		return (sf != of) || (zf);
	case C_L:
		return sf != of;
	case C_E:
		return zf;
	case C_NE:
		return !zf;
	case C_GE:
		return !(sf != of);
	case C_G:
		return !(sf != of) && !zf;
	default:
		return 1;
	}
}

static sval_t alu(alu_t alufun, sval_t aluA, sval_t aluB, int set_cc)
{
	int zf, sf, of;
	sval_t aluE;

	switch (alufun) {
	case A_ADD:
		aluE = (aluB + aluA);
		of = (((aluA < 0) == (aluB < 0))
		   && ((aluE < 0) != (aluA < 0)));
		break;
	case A_SUB:
		aluE = (aluB - aluA);
		of = (((aluA < 0) != (aluB < 0))
		   && ((aluE < 0) != (aluA < 0)));
		break;
	case A_AND:
		aluE = (aluB & aluA);
		of = 0;
		break;
	case A_XOR:
		aluE = (aluB ^ aluA);
		of = 0;
		break;
	}

	zf = (aluE == 0);
	sf = (aluE < 0);

	if (set_cc)
		setCC(CC, of, sf, zf);

	return aluE;
}

static int memory(val_t addr, int mem_read, int mem_write, val_t *valp) 
{
	if ((mem_read || mem_write) && (addr + sizeof(*valp) > MAXMEM))
		return -1;

	if (mem_write)
		*(val_t *)&M[addr] = *valp;
	if (mem_read)
		*valp = *(val_t *)&M[addr];
	return 0;
}

static void run()
{
	val_t valP, mem_addr;
	ins_t ins;
	icode_t icode;
	ifun_t ifun, alufun;
	reg_t reg;
	regid_t rA, rB, srcA, srcB, dstE;
	val_t valA, valB, valC, valE, valM;
	sval_t aluA, aluB;
	int Cnd, set_cc, mem_read, mem_write;

	PC = 0;
	Step = 0;
	Stat = S_AOK;
	while (1) {
		Step++;
		/* fetch */
		valP = PC;
		ins = *(ins_t *)&M[valP];
		if (ins_name(ins) == NULL) {
			Stat = S_INS;
			return;
		}
		valP += sizeof(ins);
		icode = ins_icode(ins);
		ifun = ins_ifun(ins);
		if (need_reg(icode)) {
			reg = *(reg_t *)&M[valP];
			rA = reg_rA(reg);
			rB = reg_rB(reg);
			valP += sizeof(reg);
		}
		if (need_val(icode)) {
			valC = *(val_t *)&M[valP];
			valP += sizeof(valC);
		}
		if (icode == I_HALT) {
			Stat = S_HLT;
			return;
		}

		/* decode */
		/**
		 * int srcA = [
		 * 	icode in { I_RRMOVL, I_IMMOVL, I_OPL, I_PUSHL } : rA;
		 * 	icode in { I_POPL, I_RET } : R_ESP;
		 * 	1 : R_NONE;
		 * ];
		 */
		switch(icode) {
		case I_RRMOVL:
		case I_RMMOVL:
		case I_OPL:
		case I_PUSHL:
			srcA = rA;
			break;
		case I_POPL:
		case I_RET:
			srcA = R_ESP;
			break;
		default:
			srcA = R_NONE;
		}
		valA = R[srcA];

		/**
		 * int srcB = [
		 * 	icode in { I_RMMOVL, I_MRMOVL, I_OPL } : rB;
		 * 	icode in { I_PUSHL, I_POPL, I_CALL, I_RET } : R_ESP;
		 * 	1 : R_NONE;
		 * ];
		 */
		switch(icode) {
		case I_RMMOVL:
		case I_MRMOVL:
		case I_OPL:
			srcB = rB;
			break;
		case I_PUSHL:
		case I_POPL:
		case I_CALL:
		case I_RET:
			srcB = R_ESP;
			break;
		default:
			srcB = R_NONE;
		}
		valB = R[srcB];

		/**
		 * int dstE = [
		 * 	icode == I_RRMOVL && Cnd : rB;
		 * 	icode in { I_IRMOVL, I_OPL } : rB;
		 * 	icode in { I_PUSHL, I_POPL, I_CALL, I_RET } : R_ESP;
		 * 	1 : R_NONE;
		 * ];
		 */
		switch(icode) {
		case I_RRMOVL:
			Cnd = cond(ifun);
			dstE = Cnd ? rB : R_NONE;
			break;
		case I_IRMOVL:
		case I_OPL:
			dstE = rB;
			break;
		case I_PUSHL:
		case I_POPL:
		case I_CALL:
		case I_RET:
			dstE = R_ESP;
			break;
		default:
			dstE = R_NONE;
		}

		/* execute */
		/*
		 * int aluA = [
		 * 	icode in { I_RRMOVL, I_OPL } : valA;
		 * 	icode in { I_IRMOVL, I_RMMOVL, I_MRMOVL } : valC;
		 * 	icode in { I_CALL, I_PUSHL } : -4;
		 * 	icode in { I_RET, I_POPL } : 4;
		 * ];
		 */
		switch (icode) {
		case I_RRMOVL:
		case I_OPL:
			aluA = valA;
			break;
		case I_IRMOVL:
		case I_RMMOVL:
		case I_MRMOVL:
			aluA = valC;
			break;
		case I_CALL:
		case I_PUSHL:
			aluA = -4;
			break;
		case I_RET:
		case I_POPL:
			aluA = 4;
			break;
		default:
			;
		}

		/**
		 * int aluB = [
		 * 	icode in {I_RRMOVL, I_IRMOVL} : 0;
		 * 	icode in {I_RMMOVL, I_MRMOVL, I_OPL,
		 * 		  I_PUSHL, I_POPL, I_CALL, I_RET} : valB;
		 * ];
		 */
		switch (icode) {
		case I_RRMOVL:
		case I_IRMOVL:
			aluB = 0;
			break;
		case I_RMMOVL:
		case I_MRMOVL:
		case I_OPL:
		case I_PUSHL:
		case I_POPL:
		case I_CALL:
		case I_RET:
			aluB = valB;
			break;
		default:
			;
		}

		/**
		 * int alufun = [
		 * 	icode == I_OPL : ifun;
		 * 	1 : A_ADD;
		 * ];
		 */
		switch (icode) {
		case I_OPL:
			alufun = ifun;
			break;
		default:
			alufun = A_ADD;
		}

		/**
		 * bool set_cc = icode in { I_OPL };
		 */
		set_cc = (icode == I_OPL);

		/* ALU */
		valE = alu(alufun, aluA, aluB, set_cc);

		/* memory */
		/**
		 * int mem_addr = [
		 * 	icode in { I_RMMOVL, I_PUSHL, I_CALL, I_MRMOVL } : valE;
		 * 	icode in { I_POPL, I_RET } : valA;
		 * ];
		 */
		switch (icode) {
		case I_RMMOVL:
		case I_PUSHL:
		case I_CALL:
		case I_MRMOVL:
			mem_addr = valE;
			break;
		case I_POPL:
		case I_RET:
			mem_addr = valA;
			break;
		default:
			;
		}

		/**
		 * int mem_read = icode in { I_MRMOVL, I_POPL, I_RET };
		 */
		switch (icode) {
		case I_MRMOVL:
		case I_POPL:
		case I_RET:
			mem_read = 1;
			break;
		default:
			mem_read = 0;
		}

		/**
		 * int mem_write = icode in { I_RMMOVL, I_PUSHL, I_CALL };
		 */
		switch (icode) {
		case I_RMMOVL:
		case I_PUSHL:
			valM = valA;
			mem_write = 1;
			break;
		case I_CALL:
			valM = valP;
			mem_write = 1;
			break;
		default:
			mem_write = 0;
		}

		if (memory(mem_addr, mem_read, mem_write, &valM)) {
			Stat = S_ADR;
			return;
		}

		/* write_back */
		if (dstE != R_NONE) {
			R[dstE] = valE;
		}
		switch (icode) {
		case I_MRMOVL:
		case I_POPL:
			R[rA] = valM;
			break;
		default:
			;
		}

		/* PC_update */
		/**
		 * int new_pc = [
		 * 	icode == I_CALL : valC;
		 * 	icode == I_JXX && Cnd : valC;
		 * 	icode == I_RET : valM;
		 * 	1 : valP;
		 * ];
		 */
		switch (icode) {
		case I_CALL:
			PC = valC;
			break;
		case I_JXX:
			Cnd = cond(ifun);
			PC = Cnd ? valC : valP;
			break;
		case I_RET:
			PC = valM;
			break;
		default:
			PC = valP;
		}
	}
}

int main(int argc, char *argv[])
{
	FILE *input;
	int zf, sf, of;
	size_t n;
	val_t now, orig;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: %s <input>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input = fopen(argv[1], "r");
	n = fread(M, sizeof(byte), MAXMEM, input);
	memcpy(origM, M, n);
	fclose(input);

	run();

	getCC(CC, of, sf, zf);
	printf("Stopped in %u steps at PC = 0x%x. ", Step, PC);
	printf("Status '%s', ", stat_name[Stat]);
	printf("CC Z=%d, S=%d, O=%d\n", zf, sf, of);
	printf("Changes to registers:\n");
	for (int i = 0; i < 8; i++)
		if (R[i] != 0)
			printf("%s:\t0x%08x\t0x%08x\n", regid_name(i), 0, R[i]);
	puts("");
	printf("Changes to memory:\n");
	for (int i = 0; i < MAXMEM; i += sizeof(val_t)) {
		now = *(val_t *)&M[i];
		orig = *(val_t *)&origM[i];
		if (now != orig) {
			printf("0x%04x:\t0x%08x\t0x%08x\n", i, orig, now);
		}
	}
	exit(EXIT_SUCCESS);
}
