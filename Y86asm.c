#include "Y86.h"
#include "list.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

static void add_symbol(const char *symbol, imm_t value)
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

#define pack_ins(icode, func) pack(icode, func)
#define icode_of_ins(ins) unpack_h(ins)
#define func_of_ins(ins) unpack_l(ins)

#define pack_reg(rA, rB) pack(rA, rB)
#define rA_of_reg(reg) unpack_h(reg)
#define rB_of_reg(reg) unpack_l(reg)

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

static ins_t parse_ins(const char *str)
{
	const struct ins_dict *ptr;

	for (ptr = Y86_INS_DICT; ptr->str != NULL; ptr++)
		if (strcmp(ptr->str, str) == 0)
			return ptr->ins;

	error("unknown instruction", "%s", str);
	exit(EXIT_FAILURE);
}

const char *instr_name(ins_t ins)
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

static regid_t parse_regid(const char *str)
{
	const struct regid_dict *ptr;

	for (ptr = Y86_REGID_DICT; ptr->str != NULL; ptr++)
		if (strcmp(ptr->str, str) == 0)
			return ptr->regid;

	error("unknown register", "%s", str);
	exit(EXIT_FAILURE);
}

static const char *register_name(regid_t regid)
{
	const struct regid_dict *ptr;

	for (ptr = Y86_REGID_DICT; ptr->str != NULL; ptr++)
		if (ptr->regid == regid)
			return ptr->str;

	error("unknown register", "%02X", regid);
	exit(EXIT_FAILURE);
}

const char *regA_name(reg_t reg)
{
	return register_name(rA_of_reg(reg));
}

const char *regB_name(reg_t reg)
{
	return register_name(rB_of_reg(reg));
}

static void fill_i(char **args, reg_t *regp, imm_t *immp);
static void fill_i_r_r(char **args, reg_t *regp, imm_t *immp);
static void fill_i_v_r(char **args, reg_t *regp, imm_t *immp);
static void fill_i_r_m(char **args, reg_t *regp, imm_t *immp);
static void fill_i_m_r(char **args, reg_t *regp, imm_t *immp);
static void fill_i_v(char **args, reg_t *regp, imm_t *immp);
static void fill_i_r(char **args, reg_t *regp, imm_t *immp);

#define argn_of_icode(icode) (Y86_ICODE_INFO[icode].argn)
#define filler_of_icode(icode) (Y86_ICODE_INFO[icode].filler)

static int has_reg_section(icode_t icode)
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

static int has_imm_section(icode_t icode)
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

struct icode_info {
	size_t argn;
	void (*filler)(char **, reg_t *, imm_t *);
};

static const struct icode_info Y86_ICODE_INFO[] = {
	{1, fill_i,    },	/* 0 halt */
	{1, fill_i,    },	/* 1 nop */
	{3, fill_i_r_r,},	/* 2 rrmovl */
	{3, fill_i_v_r,},	/* 3 irmovl */
	{3, fill_i_r_m,},	/* 4 rmmovl */
	{3, fill_i_m_r,},	/* 5 mrmovl */
	{3, fill_i_r_r,},	/* 6 opl */
	{2, fill_i_v,  },	/* 7 jxx */
	{2, fill_i_v,  },	/* 8 call */
	{1, fill_i,    },	/* 9 ret */
	{2, fill_i_r,  },	/* A pushl */
	{2, fill_i_r,  },	/* B popl */
};

static imm_t parse_number(const char *str);

size_t assembler(char **args, byte *base)
{
	static size_t s_offset = 0;	/* at the start of next instruction */
	static size_t e_offset = 0;	/* after the end of last instruction */
	byte *pos = base + s_offset;
	ins_t ins;
	icode_t icode;
	ins_t *insp = NULL;
	reg_t *regp = NULL;
	imm_t *immp = NULL;
	size_t tmp;

	if (args[0] == NULL)
		return e_offset;

	/* check symbol */
	tmp = strlen(args[0]);
	if (args[0][tmp - 1] == ':') {
		args[0][tmp - 1] = '\0';
		add_symbol(args[0], s_offset);
		if (*++args == NULL)
			return e_offset;
	}

	/* check command */
	if (args[0][0] == '.') {
		if (args[1] == NULL || args[2] != NULL) {
			error("wrong command syntax", "%s", args[0]);
			exit(EXIT_FAILURE);
		}
		tmp = parse_number(args[1]);
		if (strcmp(args[0], ".long") == 0) {
			immp = (imm_t *)pos;
			pos = (byte *)(immp + 1);
			*immp = tmp;
			s_offset = e_offset = pos - base;
		} else if (strcmp(args[0], ".pos") == 0) {
			s_offset = tmp;
		} else if (strcmp(args[0], ".align") == 0) {
			if (s_offset % tmp != 0)
				s_offset += tmp - s_offset % tmp;
		} else {
			error("unknown command", "%s", args[0]);
			exit(EXIT_FAILURE);
		}
		return e_offset;
	}

	/* instruction */
	ins = parse_ins(args[0]);
	icode = icode_of_ins(ins);

	insp = (ins_t *)pos;
	pos = (byte *)(insp + 1);
	if (has_reg_section(icode)) {
		regp = (reg_t *)pos;
		pos = (byte *)(regp + 1);
	}
	if (has_imm_section(icode)) {
		immp = (imm_t *)pos;
		pos = (byte *)(immp + 1);
	}

	/* next instruction follows the last */
	s_offset = e_offset = pos - base;

	/* check argn */
	for (tmp = 0; args[tmp] != NULL; tmp++)
		;
	if (tmp != argn_of_icode(icode)) {
		error("wrong instruction syntax", "%s", args[0]);
		exit(EXIT_FAILURE);
	}

	/* fill sections */
	*insp = ins;
	filler_of_icode(icode)(args, regp, immp);

	return e_offset;
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
		exit(EXIT_FAILURE);
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
