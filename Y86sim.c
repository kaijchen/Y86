#include "lib/Y86.h"
#include <stdio.h>
#include <stdlib.h>

#define MAXMEM 4000

static int ZF, SF, OF, CC;
static byte M[MAXMEM];
static val_t R[8];

static void run()
{
	val_t PC = 0, valP, mem_addr;
	ins_t ins;
	icode_t icode;
	ifun_t ifun, alufun;
	reg_t reg;
	regid_t rA, rB, srcA, srcB, dstE;
	val_t valA, valB, valC, valM;
	sval_t aluA, aluB, valE;
	int Cnd, set_cc, mem_read, mem_write;

	while (1) {
		/* fetch */
		valP = PC;
		ins = *(ins_t *)&M[valP];
		valP += sizeof(ins);
		icode = ins_icode(ins);
		ifun = ins_ifun(ins);
		printf("%04x %s", PC, ins_name(ins));
		if (need_reg(icode)) {
			reg = *(reg_t *)&M[valP];
			if ((rA = reg_rA(reg)) != R_NONE)
				printf(" %s", regid_name(rA));
			if ((rB = reg_rB(reg)) != R_NONE)
				printf(" %s", regid_name(rB));
			valP += sizeof(reg);
		}
		if (need_val(icode)) {
			valC = *(val_t *)&M[valP];
			printf(" %d", valC);
			valP += sizeof(valC);
		}
		printf("\n");
		if (icode == I_HALT)
			return;

		ZF = ((CC >> 2) & 1);
		SF = ((CC >> 1) & 1);
		OF = (CC & 1);
		switch (ifun) {
		case C_ALL:
			Cnd = 1;
			break;
		case C_LE:
			Cnd = ((SF != OF) || (ZF));
			break;
		case C_L:
			Cnd = (SF != OF);
			break;
		case C_E:
			Cnd = ZF;
			break;
		case C_NE:
			Cnd = (!ZF);
			break;
		case C_GE:
			Cnd = (!(SF != OF));
			break;
		case C_G:
			Cnd = (!(SF != OF) && !ZF);
			break;
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
		switch (alufun) {
		case A_ADD:
			valE = (aluB + aluA);
			OF = (((aluA < 0) == (aluB < 0))
			   && ((valE < 0) != (aluA < 0)));
			break;
		case A_SUB:
			valE = (aluB - aluA);
			OF = (((aluA < 0) != (aluB < 0))
			   && ((valE < 0) != (aluA < 0)));
			break;
		case A_AND:
			valE = (aluB & aluA);
			OF = 0;
			break;
		case A_XOR:
			valE = (aluB ^ aluA);
			OF = 0;
			break;
		default:
			;
		}
		ZF = (valE == 0);
		SF = (valE < 0);

		if (set_cc) {
			CC = ((ZF << 2) | (SF << 1) | OF);
		}

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
		case I_CALL:
			mem_write = 1;
			break;
		default:
			mem_write = 0;
		}

		if (mem_read)
			valM = *(val_t *)&M[mem_addr];

		if (mem_write) {
			switch (icode) {
			case I_RMMOVL:
			case I_PUSHL:
				*(val_t *)&M[mem_addr] = valA;
				break;
			case I_CALL:
				*(val_t *)&M[mem_addr] = valP;
				break;
			default:
				;
			}
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

int main()
{
	FILE *input = fopen("y.out", "r");

	/* init */
	fread(M, sizeof(byte), MAXMEM, input);
	fclose(input);

	run();
	printf("%s: %d\n", regid_name(R_EAX), R[R_EAX]);
	exit(EXIT_SUCCESS);
}
