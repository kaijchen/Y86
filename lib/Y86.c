#include "Y86.h"
#include "list.h"
#include <ctype.h>
#include <string.h>

static LIST_HEAD(symbol_head);		/* symbol list head */

struct immp_entry {
	imm_t *immp;
	struct list_head immp_list;		/* immp list */
};

struct symbol_entry {
	char *symbol;
	int valid;
	imm_t value;
	struct list_head symbol_list;	/* symbol list */
	struct list_head immp_head;	/* immp list head */
};

static void add_immp(struct list_head *immp_headp, imm_t *immp)
{
	struct immp_entry *iptr = malloc(sizeof(*iptr));

	iptr->immp = immp;

	INIT_LIST_HEAD(&iptr->immp_list);
	list_add(&iptr->immp_list, immp_headp);
}

static struct symbol_entry *add_new_symbol(const char *symbol, imm_t value)
{
	struct symbol_entry *sptr = malloc(sizeof(*sptr));

	sptr->symbol = strdup(symbol);
	sptr->value = value;
	sptr->valid = 0;
	INIT_LIST_HEAD(&sptr->immp_head);

	INIT_LIST_HEAD(&sptr->symbol_list);
	list_add(&sptr->symbol_list, &symbol_head);
	return sptr;
}

void add_symbol(const char *symbol, imm_t value)
{
	struct symbol_entry *sptr;
	struct immp_entry *iptr, *itmp;

	list_for_each_entry(sptr, &symbol_head, symbol_list)
		if (strcmp(sptr->symbol, symbol) == 0)
			break;

	if (&sptr->symbol_list == &symbol_head)
		sptr = add_new_symbol(symbol, value);

	sptr->valid = 1;

	list_for_each_entry_safe(iptr, itmp, &sptr->immp_head, immp_list) {
		*iptr->immp = value;
		free(iptr);
	}
}

static void lookup_symbol(const char *symbol, imm_t *immp)
{
	struct symbol_entry *sptr;

	list_for_each_entry(sptr, &symbol_head, symbol_list)
		if (strcmp(sptr->symbol, symbol) == 0)
			break;

	if (&sptr->symbol_list == &symbol_head) {
		sptr = add_new_symbol(symbol, 0);
		sptr->valid = 0;
	}

	if (sptr->valid) {
		*immp = sptr->value;
	} else {
		add_immp(&sptr->immp_head, immp);
	}
}

#define pack(h, l) (((l) & 0xF) | (((h) & 0xF) << 4))
#define unpack_h(c) (((c) >> 4) & 0xF)
#define unpack_l(c) ((c) & 0xF)

#define ins_pack(code, func) pack(code, func)
#define ins_code(ins) unpack_h(ins)
#define ins_func(ins) unpack_l(ins)

#define reg_pack(rA, rB) pack(rA, rB)
#define reg_rA(reg) unpack_h(reg)
#define reg_rB(reg) unpack_l(reg)

struct ins_dict {
	const char *str;
	ins_t ins;
};

static const struct ins_dict Y86_INS_DICT[] = {
	{"halt",   ins_pack(I_HALT, C_NONE)  },
	{"nop",    ins_pack(I_NOP, C_NONE)   },
	{"rrmovl", ins_pack(I_RRMOVL, C_NONE)},
	{"cmovle", ins_pack(I_RRMOVL, C_LE)  },
	{"cmovl",  ins_pack(I_RRMOVL, C_L)   },
	{"cmove",  ins_pack(I_RRMOVL, C_E)   },
	{"cmovne", ins_pack(I_RRMOVL, C_NE)  },
	{"cmovge", ins_pack(I_RRMOVL, C_GE)  },
	{"cmovg",  ins_pack(I_RRMOVL, C_G)   },
	{"irmovl", ins_pack(I_IRMOVL, C_NONE)},
	{"rmmovl", ins_pack(I_RMMOVL, C_NONE)},
	{"mrmovl", ins_pack(I_MRMOVL, C_NONE)},
	{"addl",   ins_pack(I_ALU, A_ADD)    },
	{"subl",   ins_pack(I_ALU, A_SUB)    },
	{"andl",   ins_pack(I_ALU, A_AND)    },
	{"xorl",   ins_pack(I_ALU, A_XOR)    },
	{"jmp",    ins_pack(I_JXX, C_NONE)   },
	{"jle",    ins_pack(I_JXX, C_LE)     },
	{"jl",     ins_pack(I_JXX, C_L)      },
	{"je",     ins_pack(I_JXX, C_E)      },
	{"jne",    ins_pack(I_JXX, C_NE)     },
	{"jge",    ins_pack(I_JXX, C_GE)     },
	{"jg",     ins_pack(I_JXX, C_G)      },
	{"call",   ins_pack(I_CALL, C_NONE)  },
	{"ret",    ins_pack(I_RET, C_NONE)   },
	{"pushl",  ins_pack(I_PUSHL, C_NONE) },
	{"popl",   ins_pack(I_POPL, C_NONE) },
	{".long",  ins_pack(I_LONG, C_NONE)  },
	{".pos",   ins_pack(I_POS, C_NONE)   },
	{".align", ins_pack(I_ALIGN, C_NONE) },
	{NULL,     ins_pack(I_ERR, C_NONE)   }
};

ins_t parse_instr(const char *str)
{
	const struct ins_dict *ptr;

	for (ptr = Y86_INS_DICT; ptr->str != NULL; ptr++)
		if (strcmp(ptr->str, str) == 0)
			return ptr->ins;

	error("unknown instruction", "%s", str);
	return ptr->ins;
}

const char *instr_name(ins_t ins)
{
	const struct ins_dict *ptr;

	for (ptr = Y86_INS_DICT; ptr->str != NULL; ptr++)
		if (ptr->ins == ins)
			return ptr->str;

	error("unknown instruction", "%02X", ins);
	return ptr->str;
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
	{NULL, R_ERR},
};

static regid_t parse_register(const char *str)
{
	const struct regid_dict *ptr;

	for (ptr = Y86_REGID_DICT; ptr->str != NULL; ptr++)
		if (strcmp(ptr->str, str) == 0)
			return ptr->regid;

	error("unknown register", "%s", str);
	return ptr->regid;
}

static const char *register_name(regid_t regid)
{
	const struct regid_dict *ptr;

	for (ptr = Y86_REGID_DICT; ptr->str != NULL; ptr++)
		if (ptr->regid == regid)
			return ptr->str;

	error("unknown register", "%02X", regid);
	return ptr->str;
}

const char *regA_name(reg_t reg)
{
	return register_name(reg_rA(reg));
}

const char *regB_name(reg_t reg)
{
	return register_name(reg_rB(reg));
}

typedef unsigned int flag_t;

enum Y86_SECTION_FLAG {
	F_NONE	= 0, 
	F_INS	= (1 << 0), 
	F_REG	= (1 << 1), 
	F_IMM	= (1 << 2),
};

enum Y86_SECTION_SIZE {
	S_NONE	= 0,
	S_INS	= sizeof(ins_t),
	S_REG	= sizeof(reg_t),
	S_IMM	= sizeof(imm_t),
};

#define section_size(flag, SECTION)		\
	((flag & F_##SECTION) ? S_##SECTION : 0)

#define flag_size(flag) 			\
	  (section_size(flag, INS)		\
	 + section_size(flag, REG)		\
	 + section_size(flag, IMM))

static void fill_i(char **args, reg_t *regp, imm_t *immp);
static void fill_i_r_r(char **args, reg_t *regp, imm_t *immp);
static void fill_i_v_r(char **args, reg_t *regp, imm_t *immp);
static void fill_i_r_m(char **args, reg_t *regp, imm_t *immp);
static void fill_i_m_r(char **args, reg_t *regp, imm_t *immp);
static void fill_i_v(char **args, reg_t *regp, imm_t *immp);
static void fill_i_r(char **args, reg_t *regp, imm_t *immp);

#define code_flag(ins) (Y86_CODE_INFO[ins].flag)
#define code_argn(ins) (Y86_CODE_INFO[ins].argn)
#define code_filler(ins) (Y86_CODE_INFO[ins].filler)

#define ins_flag(ins) code_flag(ins_code(ins))
#define ins_argn(ins) code_argn(ins_code(ins))
#define ins_filler(ins) code_filler(ins_code(ins))

struct code_info {
	flag_t flag;
	size_t argn;
	void (*filler)(char **, reg_t *, imm_t *);
};

static const struct code_info Y86_CODE_INFO[] = {
	{F_INS,             1, fill_i    },	/* 0 halt */
	{F_INS,             1, fill_i    },	/* 1 nop */
	{F_INS|F_REG,       3, fill_i_r_r},	/* 2 rrmovl cmovxx */
	{F_INS|F_REG|F_IMM, 3, fill_i_v_r},	/* 3 irmovl */
	{F_INS|F_REG|F_IMM, 3, fill_i_r_m},	/* 4 rmmovl */
	{F_INS|F_REG|F_IMM, 3, fill_i_m_r},	/* 5 mrmovl */
	{F_INS|F_REG,       3, fill_i_r_r},	/* 6 opl */
	{F_INS|F_IMM,       2, fill_i_v  },	/* 7 jxx */
	{F_INS|F_IMM,       2, fill_i_v  },	/* 8 call */
	{F_INS,             1, fill_i    },	/* 9 ret */
	{F_INS|F_REG,       2, fill_i_r  },	/* A pushl */
	{F_INS|F_REG,       2, fill_i_r  },	/* B popl */
	{F_IMM,             2, fill_i_v  },	/* C .long */
	{F_NONE,            2, NULL       },	/* D .pos */
	{F_NONE,            2, NULL       },	/* E .align */
	{F_NONE,            0, NULL       },	/* F ERROR */
};

code_t instr_code(ins_t ins)
{
	return ins_code(ins);
}

size_t instr_argn(ins_t ins)
{
	return ins_argn(ins);
}

size_t instr_size(ins_t ins)
{
	flag_t flag = ins_flag(ins);

	return flag_size(flag);
}

size_t assembler(ins_t ins, byte *base, char **args)
{
	flag_t flag = ins_flag(ins);
	ins_t *insp = NULL;
	reg_t *regp = NULL;
	imm_t *immp = NULL;
	byte *ptr = base;

	if ((flag & F_INS)) {
		insp = (ins_t *)ptr;
		ptr = (byte *)(insp + 1);
	}
	if ((flag & F_REG)) {
		regp = (reg_t *)ptr;
		ptr = (byte *)(regp + 1);
	}
	if ((flag & F_IMM)) {
		immp = (imm_t *)ptr;
		ptr = (byte *)(immp + 1);
	}

	if (insp != NULL)
		*insp = ins;

	ins_filler(ins)(args, regp, immp);
	return ptr - base;
}

imm_t parse_number(const char *str)
{
	imm_t dec, hex;

	if (str[0] == '\0')
		return 0;

	sscanf(str, "%d", &dec);
	sscanf(str, "%x", &hex);

	return dec == 0 ? hex : dec;
}

static void parse_constant(const char *str, imm_t *immp)
{
	if (str[0] == '\0')
		*immp = 0;
	else if (str[0] == '$')
		*immp = parse_number(str + 1);
	else if (isdigit(str[0]) || str[0] == '-' || str[0] == '+')
		*immp = parse_number(str);
	else
		lookup_symbol(str, immp);
}

static regid_t parse_memory(char *str, imm_t *immp)
{
	char *x, *y;
	regid_t regid;

	x = strchr(str, '(');
	y = strrchr(str, ')');

	if (*x == '\0' || *y == '\0' || x > y
			|| x[1] != '%' || y[1] != '\0') {
		error("wrong memory access syntax", "%s", str);
		return R_ERR;
	}

	*x = '\0';
	*y = '\0';

	*immp = parse_number(str);
	regid = parse_register(x + 1);

	*x = '(';
	*y = ')';

	return regid;
}

/**
 * fillers
 *  _i: instruction
 *  _r: register
 *  _v: constant
 *  _m: memory
 *
 * parse @args, and store reg at @regp, imm at @immp
 */

static void fill_i(char **args, reg_t *regp, imm_t *immp)
{
	return;
}

static void fill_i_r_r(char **args, reg_t *regp, imm_t *immp)
{
	*regp = reg_pack(parse_register(args[1]), parse_register(args[2]));
}

static void fill_i_v_r(char **args, reg_t *regp, imm_t *immp)
{
	parse_constant(args[1], immp);
	*regp = reg_pack(R_NONE, parse_register(args[2]));
}

static void fill_i_r_m(char **args, reg_t *regp, imm_t *immp)
{
	*regp = reg_pack(parse_register(args[1]), parse_memory(args[2], immp));
}

static void fill_i_m_r(char **args, reg_t *regp, imm_t *immp)
{
	*regp = reg_pack(parse_register(args[2]), parse_memory(args[1], immp));
}

static void fill_i_v(char **args, reg_t *regp, imm_t *immp)
{
	parse_constant(args[1], immp);
}

static void fill_i_r(char **args, reg_t *regp, imm_t *immp)
{
	*regp = reg_pack(parse_register(args[1]), R_NONE);
}