#ifndef _Y86ASM_
#define _Y86ASM_

#include <stdlib.h>

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
	R_NONE	= 0xF,
} regid_t;

typedef enum icode {
	I_HALT		= 0x0,
	I_NOP		= 0x1,
	I_RRMOVL	= 0x2,
	I_IRMOVL	= 0x3,
	I_RMMOVL	= 0x4,
	I_MRMOVL	= 0x5,
	I_OPL		= 0x6,
	I_JXX		= 0x7,
	I_CALL		= 0x8,
	I_RET		= 0x9,
	I_PUSHL		= 0xA,
	I_POPL		= 0xB,
} icode_t;

typedef enum ifun {
	/* ALU */
	A_ADD	= 0x0,
	A_SUB	= 0x1,
	A_AND	= 0x2,
	A_XOR	= 0x3,

	/* COND */
	C_ALL	= 0x0,
	C_LE	= 0x1,
	C_L	= 0x2,
	C_E	= 0x3,
	C_NE	= 0x4,
	C_GE	= 0x5,
	C_G	= 0x6,
} ifun_t;

#define pack(h, l) (((l) & 0xF) | (((h) & 0xF) << 4))
#define unpack_h(c) (((c) >> 4) & 0xF)
#define unpack_l(c) ((c) & 0xF)

#define pack_ins(icode, func) pack(icode, func)
#define icode_of_ins(ins) unpack_h(ins)
#define func_of_ins(ins) unpack_l(ins)

#define pack_reg(rA, rB) pack(rA, rB)
#define rA_of_reg(reg) unpack_h(reg)
#define rB_of_reg(reg) unpack_l(reg)

extern regid_t parse_regid(const char *str);
extern const char *regid_name(regid_t regid);

extern ins_t parse_ins(const char *str);
extern const char *ins_name(ins_t ins);

extern int has_imm_section(icode_t icode);
extern int has_reg_section(icode_t icode);

#endif
