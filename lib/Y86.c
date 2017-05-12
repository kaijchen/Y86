#include "Y86.h"
#include "list.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define error(message, syntax, value)			\
	fprintf(stderr, "Error: %s: %s - "syntax"\n",	\
			__func__, message, value)

static LIST_HEAD(symbol_head);		/* symbol list head */

struct immp_entry {
	imm_t *immp;
	struct list_head immp_list;	/* immp list */
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

#define pack_ins(code, func) pack(code, func)
#define code_of_ins(ins) unpack_h(ins)
#define func_of_ins(ins) unpack_l(ins)

#define pack_reg(rA, rB) pack(rA, rB)
#define rA_of_reg(reg) unpack_h(reg)
#define rB_of_reg(reg) unpack_l(reg)

struct ins_dict {
	const char *str;
	ins_t ins;
};

static const struct ins_dict Y86_INS_DICT[] = {
	{"halt",   pack_ins(I_HALT, C_NONE)  },
	{"nop",    pack_ins(I_NOP, C_NONE)   },
	{"rrmovl", pack_ins(I_RRMOVL, C_NONE)},
	{"cmovle", pack_ins(I_RRMOVL, C_LE)  },
	{"cmovl",  pack_ins(I_RRMOVL, C_L)   },
	{"cmove",  pack_ins(I_RRMOVL, C_E)   },
	{"cmovne", pack_ins(I_RRMOVL, C_NE)  },
	{"cmovge", pack_ins(I_RRMOVL, C_GE)  },
	{"cmovg",  pack_ins(I_RRMOVL, C_G)   },
	{"irmovl", pack_ins(I_IRMOVL, C_NONE)},
	{"rmmovl", pack_ins(I_RMMOVL, C_NONE)},
	{"mrmovl", pack_ins(I_MRMOVL, C_NONE)},
	{"addl",   pack_ins(I_ALU, A_ADD)    },
	{"subl",   pack_ins(I_ALU, A_SUB)    },
	{"andl",   pack_ins(I_ALU, A_AND)    },
	{"xorl",   pack_ins(I_ALU, A_XOR)    },
	{"jmp",    pack_ins(I_JXX, C_NONE)   },
	{"jle",    pack_ins(I_JXX, C_LE)     },
	{"jl",     pack_ins(I_JXX, C_L)      },
	{"je",     pack_ins(I_JXX, C_E)      },
	{"jne",    pack_ins(I_JXX, C_NE)     },
	{"jge",    pack_ins(I_JXX, C_GE)     },
	{"jg",     pack_ins(I_JXX, C_G)      },
	{"call",   pack_ins(I_CALL, C_NONE)  },
	{"ret",    pack_ins(I_RET, C_NONE)   },
	{"pushl",  pack_ins(I_PUSHL, C_NONE) },
	{"popl",   pack_ins(I_POPL, C_NONE) },
	{".long",  pack_ins(I_LONG, C_NONE)  },
	{".pos",   pack_ins(I_POS, C_NONE)   },
	{".align", pack_ins(I_ALIGN, C_NONE) },
	{NULL,     pack_ins(I_ERR, C_NONE)   }
};

static ins_t parse_ins(const char *str)
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

static regid_t parse_regid(const char *str)
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
	return register_name(rA_of_reg(reg));
}

const char *regB_name(reg_t reg)
{
	return register_name(rB_of_reg(reg));
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

static void fill_i(char **args, reg_t *regp, imm_t *immp);
static void fill_i_r_r(char **args, reg_t *regp, imm_t *immp);
static void fill_i_v_r(char **args, reg_t *regp, imm_t *immp);
static void fill_i_r_m(char **args, reg_t *regp, imm_t *immp);
static void fill_i_m_r(char **args, reg_t *regp, imm_t *immp);
static void fill_i_v(char **args, reg_t *regp, imm_t *immp);
static void fill_i_r(char **args, reg_t *regp, imm_t *immp);

static size_t size_pos(char **args, size_t offset);
static size_t size_align(char **args, size_t offset);

#define flag_of_code(code) (Y86_CODE_INFO[code].flag)
#define argn_of_code(code) (Y86_CODE_INFO[code].argn)
#define filler_of_code(code) (Y86_CODE_INFO[code].filler)
#define sizer_of_code(code) (Y86_CODE_INFO[code].sizer)

struct code_info {
	flag_t flag;
	size_t argn;
	void (*filler)(char **, reg_t *, imm_t *);
	size_t (*sizer)(char **, size_t);
};

static const struct code_info Y86_CODE_INFO[] = {
	{F_INS,             1, fill_i,     NULL      }, /* 0 halt */
	{F_INS,             1, fill_i,     NULL      }, /* 1 nop */
	{F_INS|F_REG,       3, fill_i_r_r, NULL      }, /* 2 rrmovl cmovxx */
	{F_INS|F_REG|F_IMM, 3, fill_i_v_r, NULL      }, /* 3 irmovl */
	{F_INS|F_REG|F_IMM, 3, fill_i_r_m, NULL      }, /* 4 rmmovl */
	{F_INS|F_REG|F_IMM, 3, fill_i_m_r, NULL      }, /* 5 mrmovl */
	{F_INS|F_REG,       3, fill_i_r_r, NULL      }, /* 6 opl */
	{F_INS|F_IMM,       2, fill_i_v,   NULL      }, /* 7 jxx */
	{F_INS|F_IMM,       2, fill_i_v,   NULL      }, /* 8 call */
	{F_INS,             1, fill_i,     NULL      }, /* 9 ret */
	{F_INS|F_REG,       2, fill_i_r,   NULL      }, /* A pushl */
	{F_INS|F_REG,       2, fill_i_r,   NULL      }, /* B popl */
	{F_IMM,             2, fill_i_v,   NULL      }, /* C .long */
	{F_NONE,            2, NULL,       size_pos  }, /* D .pos */
	{F_NONE,            2, NULL,       size_align}, /* E .align */
	{F_NONE,            0, NULL,       NULL      }, /* F ERROR */
};

size_t assembler(char **args, byte *base)
{
	static size_t offset = 0;	/* will be init only once */
	ins_t ins = parse_ins(args[0]);
	code_t code = code_of_ins(ins);
	flag_t flag = flag_of_code(code);
	ins_t *insp = NULL;
	reg_t *regp = NULL;
	imm_t *immp = NULL;
	byte *pos = base + offset;
	size_t cnt;

	for (cnt = 0; args[cnt] != NULL; cnt++)
		;
	if (cnt != argn_of_code(code))
		error("instruction syntax error", "%s", args[0]);

	if ((flag & F_INS)) {
		insp = (ins_t *)pos;
		pos = (byte *)(insp + 1);
	}
	if ((flag & F_REG)) {
		regp = (reg_t *)pos;
		pos = (byte *)(regp + 1);
	}
	if ((flag & F_IMM)) {
		immp = (imm_t *)pos;
		pos = (byte *)(immp + 1);
	}

	/* fill ins */
	if (insp != NULL)
		*insp = ins;

	/* fill other sections */
	if (filler_of_code(code) != NULL)
		filler_of_code(code)(args, regp, immp);

	if (sizer_of_code(code) != NULL)
		offset = sizer_of_code(code)(args, offset);
	else
		offset = pos - base;

	return offset;
}

static imm_t parse_number(const char *str)
{
	imm_t dec, hex;

	if (str[0] == '\0')
		return 0;

	sscanf(str, "%d", &dec);
	sscanf(str, "%x", &hex);

	return dec == 0 ? hex : dec;
}

static void fill_constant(const char *str, imm_t *immp)
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
	regid = parse_regid(x + 1);

	*x = '(';
	*y = ')';

	return regid;
}

/**
 * fillers(args, regp, immp)
 *
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
	*regp = pack_reg(parse_regid(args[1]), parse_regid(args[2]));
}

static void fill_i_v_r(char **args, reg_t *regp, imm_t *immp)
{
	fill_constant(args[1], immp);
	*regp = pack_reg(R_NONE, parse_regid(args[2]));
}

static void fill_i_r_m(char **args, reg_t *regp, imm_t *immp)
{
	*regp = pack_reg(parse_regid(args[1]), parse_memory(args[2], immp));
}

static void fill_i_m_r(char **args, reg_t *regp, imm_t *immp)
{
	*regp = pack_reg(parse_regid(args[2]), parse_memory(args[1], immp));
}

static void fill_i_v(char **args, reg_t *regp, imm_t *immp)
{
	fill_constant(args[1], immp);
}

static void fill_i_r(char **args, reg_t *regp, imm_t *immp)
{
	*regp = pack_reg(parse_regid(args[1]), R_NONE);
}

static size_t size_pos(char **args, size_t offset)
{
	imm_t imm = parse_number(args[1]);

	offset = imm;
	return offset;
}

static size_t size_align(char **args, size_t offset)
{
	imm_t imm = parse_number(args[1]);
	size_t tmp = offset % imm;

	if (tmp != 0)
		offset += imm - tmp;

	return offset;
}
