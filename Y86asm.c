#include "lib/Y86.h"
#include "lib/list.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define error(message, syntax, value)			\
	fprintf(stderr, "Error: %s: %s - "syntax"\n",	\
			__func__, message, value)

static LIST_HEAD(symbol_head);		/* head of symbol list */

struct immp_entry {
	imm_t *immp;
	struct list_head immp_list;	/* immp list node */
};

struct symbol_entry {
	char *symbol;
	int valid;
	imm_t value;
	struct list_head symbol_list;	/* symbol list node */
	struct list_head immp_head;	/* head of immp list of the symbol */
};

/**
 * add_immp(immp)
 *
 * @immp: a pointer to where the value should be stored.
 * @immp_headp: a pointer to the head of the immp lint.
 * 
 * add the immp to the immp list.
 */
static void add_immp(imm_t *immp, struct list_head *immp_headp)
{
	struct immp_entry *iptr = malloc(sizeof(*iptr));

	iptr->immp = immp;

	INIT_LIST_HEAD(&iptr->immp_list);
	list_add(&iptr->immp_list, immp_headp);
}

/**
 * add_symbol(symbol)
 *
 * @symbol: a string, name of the symbol (without trailing ':')
 *
 * add the symbol to the symbol list @symbol_head,
 * with an unassigned value and an empty immp list.
 */
static struct symbol_entry *add_symbol(const char *symbol)
{
	struct symbol_entry *sptr = malloc(sizeof(*sptr));

	sptr->symbol = strdup(symbol);
	sptr->valid = 0;
	INIT_LIST_HEAD(&sptr->immp_head);

	INIT_LIST_HEAD(&sptr->symbol_list);
	list_add(&sptr->symbol_list, &symbol_head);
	return sptr;
}

/**
 * assign_value(symbol, value)
 *
 * @symbol: a string, name of the symbol (without trailing ':').
 * @value: a number assigned to that symbol.
 *
 * assign the symbol with the value,
 * for each immp in immp list of the symbol,
 * fill *immp with the value.
 */
static void assign_value(const char *symbol, imm_t value)
{
	struct symbol_entry *sptr;
	struct immp_entry *iptr, *itmp;

	list_for_each_entry(sptr, &symbol_head, symbol_list)
		if (strcmp(sptr->symbol, symbol) == 0)
			break;

	if (&sptr->symbol_list == &symbol_head)
		sptr = add_symbol(symbol);

	if (sptr->valid == 1) {
		error("symbol already assigned with a value", "%s", symbol);
		exit(EXIT_FAILURE);
	}

	sptr->valid = 1;
	sptr->value = value;

	list_for_each_entry_safe(iptr, itmp, &sptr->immp_head, immp_list) {
		*iptr->immp = value;
		free(iptr);
	}
}

/**
 * lookup_symbol(symbol, immp)
 *
 * @symbol: a string, name of the symbol.
 * @immp: a pointer to where the value should be stored.
 *
 * fill *immp with the value of the symbol.
 * if the symbol haven't been assigned with a value yet,
 * store immp in the immp list of the symbol,
 * which will be filled when a value is assigned to the symbol.
 */
static void lookup_symbol(const char *symbol, imm_t *immp)
{
	struct symbol_entry *sptr;

	list_for_each_entry(sptr, &symbol_head, symbol_list)
		if (strcmp(sptr->symbol, symbol) == 0)
			break;

	if (&sptr->symbol_list == &symbol_head)
		sptr = add_symbol(symbol);

	if (sptr->valid) {
		*immp = sptr->value;
	} else {
		add_immp(immp, &sptr->immp_head);
	}
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
 * fill__(args, regp, immp)
 *
 *  _i: instruction
 *  _r: register
 *  _v: constant
 *  _m: memory
 *
 * fill *regp and *immp according to args.
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

/**
 * icode_argn(icode)
 *
 * @icode: instruction code
 *
 * return argument number of the icode
 */
static size_t icode_argn(icode_t icode)
{
	switch (icode) {
	case I_HALT:
	case I_NOP:
	case I_RET:
		return 1;
	case I_JXX:
	case I_CALL:
	case I_PUSHL:
	case I_POPL:
		return 2;
	case I_RRMOVL:
	case I_IRMOVL:
	case I_RMMOVL:
	case I_MRMOVL:
	case I_OPL:
		return 3;
	default:
		error("unknown icode", "%d", icode);
		exit(EXIT_FAILURE);
	}
}

static void (*icode_filler[])(char **, reg_t *, imm_t *) = {
	fill_i,		/* 0 halt */
	fill_i,		/* 1 nop */
	fill_i_r_r,	/* 2 rrmovl */
	fill_i_v_r,	/* 3 irmovl */
	fill_i_r_m,	/* 4 rmmovl */
	fill_i_m_r,	/* 5 mrmovl */
	fill_i_r_r,	/* 6 opl */
	fill_i_v,	/* 7 jxx */
	fill_i_v,	/* 8 call */
	fill_i,		/* 9 ret */
	fill_i_r,	/* A pushl */
	fill_i_r,	/* B popl */
};

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
		assign_value(args[0], s_offset);
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
	if (tmp != icode_argn(icode)) {
		error("wrong instruction syntax", "%s", args[0]);
		exit(EXIT_FAILURE);
	}

	/* fill sections */
	*insp = ins;
	icode_filler[icode](args, regp, immp);

	return e_offset;
}

#define MAXBIN 4000
#define BUFSIZE 1000
#define MAXARGN 8

#define max(x, y) ((x) < (y) ? (y) : (x))

static byte binary[MAXBIN];
static size_t bin_size;

static char **parse_line(char *str)
{
	static char *args[MAXARGN];
	char *token;
	size_t n = 0;

	while ((token = strsep(&str, "\n\t ,")) != NULL) {

		/* skip comments, ensure within limit */
		if (token[0] == '#' || n + 1 == MAXARGN)
			break;

		/* skip empty fields */
		if (token[0] == '\0')
			continue;

		args[n++] = token;
	}

	/* null terminate */
	args[n] = NULL;

	return args;
}

static void driver(FILE *in)
{
	char buf[BUFSIZE];
	char **argp;
	size_t e_offset;

	bin_size = 0;

	while (fgets(buf, BUFSIZE, in) != NULL) {
		argp = parse_line(buf);
		e_offset = assembler(argp, binary);
		bin_size = max(e_offset, bin_size);
	}
}

int main(int argc, char *argv[])
{
	FILE *output;

	driver(stdin);

	output = fopen("y.out", "w");
	fwrite(binary, sizeof(byte), bin_size, output);
	fclose(output);
	exit(EXIT_SUCCESS);
}
