#ifndef _Y86ASM_
#define _Y86ASM_

#include <stdlib.h>
#include <stdio.h>

#define error(message, syntax, value)			\
	fprintf(stderr, "Error: %s: %s - "syntax"\n",	\
			__func__, message, value)

typedef unsigned int imm_t;
typedef unsigned char reg_t;
typedef unsigned char ins_t;
typedef unsigned char byte;

typedef enum regid {
	R_EAX	= 0x0,
	R_ECX	= 0x1,
	R_EDX	= 0x2,
	R_EBX	= 0x3,
	R_ESP	= 0x4,
	R_EBP	= 0x5,
	R_ESI	= 0x6,
	R_EDI	= 0x7,
	R_ERR	= 0xE,
	R_NONE	= 0xF,
} regid_t;

typedef enum code {
	I_HALT		= 0x0,
	I_NOP		= 0x1,
	I_RRMOVL	= 0x2,
	I_IRMOVL	= 0x3,
	I_RMMOVL	= 0x4,
	I_MRMOVL	= 0x5,
	I_ALU		= 0x6,
	I_JXX		= 0x7,
	I_CALL		= 0x8,
	I_RET		= 0x9,
	I_PUSHL		= 0xA,
	I_POPL		= 0xB,
	I_LONG		= 0xC,
	I_POS		= 0xD,
	I_ALIGN		= 0xE,
	I_ERR		= 0xF,
} code_t;

typedef enum alu {
	A_ADD	= 0x0,
	A_SUB	= 0x1,
	A_AND	= 0x2,
	A_XOR	= 0x3,
} alu_t;

typedef enum cond {
	C_NONE	= 0x0,
	C_LE	= 0x1,
	C_L	= 0x2,
	C_E	= 0x3,
	C_NE	= 0x4,
	C_GE	= 0x5,
	C_G	= 0x6,
} cond_t;

extern void add_symbol(const char *symbol, imm_t value);

extern size_t instr_argn(ins_t ins);
extern size_t instr_size(ins_t ins);
extern code_t instr_code(ins_t ins);

extern size_t assembler(ins_t ins, byte *base, char **args);

extern ins_t parse_instr(const char *str);
extern imm_t parse_number(const char *str);

extern const char *ins_name(ins_t ins);
extern const char *regA_name(reg_t reg);
extern const char *regB_name(reg_t reg);

#endif
