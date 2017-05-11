#ifndef _Y86ASM_
#define _Y86ASM_

#include "lib/list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define error(message, value)						\
	do {								\
		fprintf(stderr, "%s: %s - %s\n",			\
				__func__, message, value);		\
		exit(EXIT_FAILURE);					\
	} while (0)

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

static LIST_HEAD(symbol_list);

struct symbol_entry {
	char *symbol;
	uint32 value;
	struct list_head list;
};

static inline void add_symbol(const char *symbol, int value)
{
	struct symbol_entry *ptr = malloc(sizeof(*ptr));

	ptr->symbol = strdup(symbol);
	ptr->value = value;

	INIT_LIST_HEAD(&ptr->list);
	list_add(&ptr->list, &symbol_list);
}

static uint32 lookup_symbol(const char *str)
{
	const struct symbol_entry *ptr;

	list_for_each_entry(ptr, &symbol_list, list)
		if (strcmp(ptr->symbol, str) == 0)
			return ptr->value;

	error("unknown symbol", str);
	return 0;
}

#define unpack_h(c) (((c) >> 4) & 0xF)
#define unpack_l(c) ((c) & 0xF)
#define pack(h, l) ((((h) & 0xF) << 4) | ((l) & 0xF))

/* Y86 register id */
enum {R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP,
      R_ESI, R_EDI, R_NONE = 0xF, R_ERR = 0x10};

/* Y86 instruction type */
enum {I_HALT, I_NOP, I_RRMOVL, I_IRMOVL, I_RMMOVL, I_MRMOVL, I_ALU, I_JXX,
      I_CALL, I_RET, I_PUSHL, I_POPL, I_LONG, I_POS, I_ALIGN, I_ERR = 0xF};

/* Y86 instruction extend */
enum {A_ADD, A_SUB, A_AND, A_XOR};
enum {C_NONE, C_LE, C_L, C_E, C_NE, C_GE, C_G};
enum {F_NONE = 0, F_INS = 1, F_REG = 2, F_IMM = 4};
enum {S_NONE = 0, S_INS = 1, S_REG = 1, S_IMM = 4};

#define part_size(flag, part) ((flag & F_##part) ? S_##part : 0)

static inline uint32 code_size(uint8 flag)
{
	return part_size(flag, INS)
	     + part_size(flag, REG)
	     + part_size(flag, IMM);
}

static uint64 encode(uint8 flag, uint8 ins, uint8 reg, uint32 imm)
{
	uint64 result = 0;
	uint8 *p = (uint8 *)&result;

	if ((flag & F_INS)) {
		*(uint8 *)p = ins;
		p += S_INS;
	}
	if ((flag & F_REG)) {
		*(uint8 *)p = reg;
		p += S_REG;
	}
	if ((flag & F_IMM)) {
		*(uint32 *)p = imm;
		p += S_IMM;
	}
	return result;
}

static uint8 parse_instruction(const char *str);
static uint8 parse_register(const char *str);
static uint8 parse_memory(char *str, uint32 *immp);

static uint32 parse_number(const char *str);
static uint32 parse_symbol(const char *str);
static uint32 parse_constant(const char *str);

static void parse_i(char **args, uint8 *regp, uint32 *immp);
static void parse_i_r_r(char **args, uint8 *regp, uint32 *immp);
static void parse_i_v_r(char **args, uint8 *regp, uint32 *immp);
static void parse_i_r_m(char **args, uint8 *regp, uint32 *immp);
static void parse_i_m_r(char **args, uint8 *regp, uint32 *immp);
static void parse_i_imm(char **args, uint8 *regp, uint32 *immp);
static void parse_i_r(char **args, uint8 *regp, uint32 *immp);

struct ins_type {
	uint8 flag;
	uint8 argn;
	void (*parse)(char **, uint8 *, uint32 *);
};

static struct ins_type Y86_INS_TYPE[] = {
	{F_INS,             1, parse_i    },	/* 0 halt */
	{F_INS,             1, parse_i    },	/* 1 nop */
	{F_INS|F_REG,       3, parse_i_r_r},	/* 2 rrmovl cmovxx */
	{F_INS|F_REG|F_IMM, 3, parse_i_v_r},	/* 3 irmovl */
	{F_INS|F_REG|F_IMM, 3, parse_i_r_m},	/* 4 rmmovl */
	{F_INS|F_REG|F_IMM, 3, parse_i_m_r},	/* 5 mrmovl */
	{F_INS|F_REG,       3, parse_i_r_r},	/* 6 opl */
	{F_INS|F_IMM,       2, parse_i_imm},	/* 7 jxx */
	{F_INS|F_IMM,       2, parse_i_imm},	/* 8 call */
	{F_INS,             1, parse_i    },	/* 9 ret */
	{F_INS|F_REG,       2, parse_i_r  },	/* A pushl */
	{F_INS|F_REG,       2, parse_i_r  },	/* B popl */
	{F_IMM,             2, parse_i_imm},	/* C .long */
	{F_NONE,            2, parse_i_imm},	/* D .pos */
	{F_NONE,            2, parse_i_imm},	/* E .align */
	{F_NONE,            0, NULL       },	/* F ERROR */
};

struct dict {
	char *name;
	uint8 code;
};

static struct dict Y86_INS_DICT[] = {
	{"halt",   pack(I_HALT, C_NONE)  },
	{"nop",    pack(I_NOP, C_NONE)   },
	{"rrmovl", pack(I_RRMOVL, C_NONE)},
	{"cmovle", pack(I_RRMOVL, C_LE)  },
	{"cmovl",  pack(I_RRMOVL, C_L)   },
	{"cmove",  pack(I_RRMOVL, C_E)   },
	{"cmovne", pack(I_RRMOVL, C_NE)  },
	{"cmovge", pack(I_RRMOVL, C_GE)  },
	{"cmovg",  pack(I_RRMOVL, C_G)   },
	{"irmovl", pack(I_IRMOVL, C_NONE)},
	{"rmmovl", pack(I_RMMOVL, C_NONE)},
	{"mrmovl", pack(I_MRMOVL, C_NONE)},
	{"addl",   pack(I_ALU, A_ADD)    },
	{"subl",   pack(I_ALU, A_SUB)    },
	{"andl",   pack(I_ALU, A_AND)    },
	{"xorl",   pack(I_ALU, A_XOR)    },
	{"jmp",    pack(I_JXX, C_NONE)   },
	{"jle",    pack(I_JXX, C_LE)     },
	{"jl",     pack(I_JXX, C_L)      },
	{"je",     pack(I_JXX, C_E)      },
	{"jne",    pack(I_JXX, C_NE)     },
	{"jge",    pack(I_JXX, C_GE)     },
	{"jg",     pack(I_JXX, C_G)      },
	{"call",   pack(I_CALL, C_NONE)  },
	{"ret",    pack(I_RET, C_NONE)   },
	{"pushl",  pack(I_PUSHL, C_NONE) },
	{"popl",   pack(I_POPL, C_NONE) },
	{".long",  pack(I_LONG, C_NONE)  },
	{".pos",   pack(I_POS, C_NONE)   },
	{".align", pack(I_ALIGN, C_NONE) },
	{NULL,     pack(I_ERR, C_NONE)   }
};

static struct dict Y86_REG_DICT[] = {
	{"%eax", R_EAX},
	{"%ecx", R_ECX},
	{"%edx", R_EDX},
	{"%ebx", R_EBX},
	{"%esp", R_ESP},
	{"%ebp", R_EBP},
	{"%esi", R_ESI},
	{"%edi", R_EDI},
	{NULL,   R_ERR},
};

static uint8 parse_instruction(const char *str)
{
	struct dict *ptr;

	for (ptr = Y86_INS_DICT; ptr->name != NULL; ptr++)
		if (strcmp(ptr->name, str) == 0)
			return ptr->code;

	error("unknown instruction", str);
	return ptr->code;
}

static uint8 parse_register(const char *str)
{
	struct dict *ptr;

	for (ptr = Y86_REG_DICT; ptr->name != NULL; ptr++)
		if (strcmp(ptr->name, str) == 0)
			return ptr->code;

	error("unknown register", str);
	return ptr->code;
}

static uint32 parse_number(const char *str)
{
	uint32 dec, hex;

	if (str[0] == '\0')
		return 0;

	sscanf(str, "%d", &dec);
	sscanf(str, "%x", &hex);

	return dec == 0 ? hex : dec;
}

static inline uint32 parse_symbol(const char *str)
{
	return lookup_symbol(str);
}

static uint32 parse_constant(const char *str)
{
	if (str[0] == '\0')
		return 0;

	if (str[0] == '$')
		return parse_number(str + 1);

	if (isdigit(str[0]) || str[0] == '-' || str[0] == '+')
		return parse_number(str);
	else
		return parse_symbol(str);
}

static uint8 parse_memory(char *str, uint32 *immp)
{
	char *x, *y;
	uint8 reg;

	x = strchr(str, '(');
	y = strrchr(str, ')');

	if (*x == '\0' || *y == '\0' || x > y || x[1] != '%' || y[1] != '\0') {
		error("wrong memory access syntax", str);
		return R_ERR;
	}

	*x = '\0';
	*y = '\0';

	*immp = parse_number(str);
	reg = parse_register(x + 1);

	*x = '(';
	*y = ')';

	return reg;
}

static void parse_i(char **args, uint8 *regp, uint32 *immp)
{
	return;
}

static void parse_i_r_r(char **args, uint8 *regp, uint32 *immp)
{
	*regp = pack(parse_register(args[1]), parse_register(args[2]));
}

static void parse_i_v_r(char **args, uint8 *regp, uint32 *immp)
{
	*immp = parse_constant(args[1]);
	*regp = pack(R_NONE, parse_register(args[2]));
}

static void parse_i_r_m(char **args, uint8 *regp, uint32 *immp)
{
	*regp = pack(parse_register(args[1]), parse_memory(args[2], immp));
}

static void parse_i_m_r(char **args, uint8 *regp, uint32 *immp)
{
	*regp = pack(parse_register(args[2]), parse_memory(args[1], immp));
}

static void parse_i_imm(char **args, uint8 *regp, uint32 *immp)
{
	*immp = parse_constant(args[1]);
}

static void parse_i_r(char **args, uint8 *regp, uint32 *immp)
{
	*regp = pack(parse_register(args[1]), R_NONE);
}

#endif
