#include "Y86.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define error(message, syntax, value)			\
	fprintf(stderr, "Error: %s: %s - "syntax"\n",	\
			__func__, message, value)

struct ins_dict {
	const char *str;
	ins_t ins;
};

static const struct ins_dict Y86_INS_DICT[] = {
	{"halt",   pack_ins(I_HALT, C_ALL)  },
	{"nop",    pack_ins(I_NOP, C_ALL)   },
	{"rrmovl", pack_ins(I_RRMOVL, C_ALL)},
	{"cmovle", pack_ins(I_RRMOVL, C_LE) },
	{"cmovl",  pack_ins(I_RRMOVL, C_L)  },
	{"cmove",  pack_ins(I_RRMOVL, C_E)  },
	{"cmovne", pack_ins(I_RRMOVL, C_NE) },
	{"cmovge", pack_ins(I_RRMOVL, C_GE) },
	{"cmovg",  pack_ins(I_RRMOVL, C_G)  },
	{"irmovl", pack_ins(I_IRMOVL, C_ALL)},
	{"rmmovl", pack_ins(I_RMMOVL, C_ALL)},
	{"mrmovl", pack_ins(I_MRMOVL, C_ALL)},
	{"addl",   pack_ins(I_OPL, A_ADD)   },
	{"subl",   pack_ins(I_OPL, A_SUB)   },
	{"andl",   pack_ins(I_OPL, A_AND)   },
	{"xorl",   pack_ins(I_OPL, A_XOR)   },
	{"jmp",    pack_ins(I_JXX, C_ALL)   },
	{"jle",    pack_ins(I_JXX, C_LE)    },
	{"jl",     pack_ins(I_JXX, C_L)     },
	{"je",     pack_ins(I_JXX, C_E)     },
	{"jne",    pack_ins(I_JXX, C_NE)    },
	{"jge",    pack_ins(I_JXX, C_GE)    },
	{"jg",     pack_ins(I_JXX, C_G)     },
	{"call",   pack_ins(I_CALL, C_ALL)  },
	{"ret",    pack_ins(I_RET, C_ALL)   },
	{"pushl",  pack_ins(I_PUSHL, C_ALL) },
	{"popl",   pack_ins(I_POPL, C_ALL)  },
};

ins_t parse_ins(const char *str)
{
	const struct ins_dict *ptr;

	for (ptr = Y86_INS_DICT; ptr->str != NULL; ptr++)
		if (strcmp(ptr->str, str) == 0)
			return ptr->ins;

	error("unknown instruction", "%s", str);
	exit(EXIT_FAILURE);
}

const char *ins_name(ins_t ins)
{
	const struct ins_dict *ptr;

	for (ptr = Y86_INS_DICT; ptr->str != NULL; ptr++)
		if (ptr->ins == ins)
			return ptr->str;

	error("unknown instruction", "%02X", ins);
	exit(EXIT_FAILURE);
}

struct regid_dict {
	const char *str;
	regid_t regid;
};

static const struct regid_dict Y86_REGID_DICT[] = {
	{"%eax", R_EAX},
	{"%ecx", R_ECX},
	{"%edx", R_EDX},
	{"%ebx", R_EBX},
	{"%esp", R_ESP},
	{"%ebp", R_EBP},
	{"%esi", R_ESI},
	{"%edi", R_EDI},
};

regid_t parse_regid(const char *str)
{
	const struct regid_dict *ptr;

	for (ptr = Y86_REGID_DICT; ptr->str != NULL; ptr++)
		if (strcmp(ptr->str, str) == 0)
			return ptr->regid;

	error("unknown register", "%s", str);
	exit(EXIT_FAILURE);
}

const char *regid_name(regid_t regid)
{
	const struct regid_dict *ptr;

	for (ptr = Y86_REGID_DICT; ptr->str != NULL; ptr++)
		if (ptr->regid == regid)
			return ptr->str;

	error("unknown register", "%02X", regid);
	exit(EXIT_FAILURE);
}

int has_reg_section(icode_t icode)
{
	switch (icode) {
	case I_RRMOVL:
	case I_IRMOVL:
	case I_RMMOVL:
	case I_MRMOVL:
	case I_OPL:
	case I_PUSHL:
	case I_POPL:
		return 1;
	default:
		return 0;
	}
}

int has_imm_section(icode_t icode)
{
	switch (icode) {
	case I_IRMOVL:
	case I_RMMOVL:
	case I_MRMOVL:
	case I_JXX:
	case I_CALL:
		return 1;
	default:
		return 0;
	}
}
