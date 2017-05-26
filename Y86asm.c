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

struct valp_entry {
	val_t *valp;
	struct list_head valp_list;	/* valp list node */
};

struct symbol_entry {
	char *symbol;
	int valid;
	val_t value;
	struct list_head symbol_list;	/* symbol list node */
	struct list_head valp_head;	/* head of valp list of the symbol */
};

/**
 * add_valp(valp)
 *
 * @valp: a pointer to where the value should be stored.
 * @valp_headp: a pointer to the head of the valp lint.
 * 
 * add the valp to the valp list.
 */
static void add_valp(val_t *valp, struct list_head *valp_headp)
{
	struct valp_entry *iptr = malloc(sizeof(*iptr));

	iptr->valp = valp;

	INIT_LIST_HEAD(&iptr->valp_list);
	list_add(&iptr->valp_list, valp_headp);
}

/**
 * add_symbol(symbol)
 *
 * @symbol: a string, name of the symbol (without trailing ':')
 *
 * add the symbol to the symbol list @symbol_head,
 * with an unassigned value and an empty valp list.
 */
static struct symbol_entry *add_symbol(const char *symbol)
{
	struct symbol_entry *sptr = malloc(sizeof(*sptr));

	sptr->symbol = strdup(symbol);
	sptr->valid = 0;
	INIT_LIST_HEAD(&sptr->valp_head);

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
 * for each valp in valp list of the symbol,
 * fill *valp with the value.
 */
static void assign_value(const char *symbol, val_t value)
{
	struct symbol_entry *sptr;
	struct valp_entry *iptr, *itmp;

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

	list_for_each_entry_safe(iptr, itmp, &sptr->valp_head, valp_list) {
		*iptr->valp = value;
		free(iptr);
	}
}

/**
 * lookup_symbol(symbol, valp)
 *
 * @symbol: a string, name of the symbol.
 * @valp: a pointer to where the value should be stored.
 *
 * fill *valp with the value of the symbol.
 * if the symbol haven't been assigned with a value yet,
 * store valp in the valp list of the symbol,
 * which will be filled when a value is assigned to the symbol.
 */
static void lookup_symbol(const char *symbol, val_t *valp)
{
	struct symbol_entry *sptr;

	list_for_each_entry(sptr, &symbol_head, symbol_list)
		if (strcmp(sptr->symbol, symbol) == 0)
			break;

	if (&sptr->symbol_list == &symbol_head)
		sptr = add_symbol(symbol);

	if (sptr->valid) {
		*valp = sptr->value;
	} else {
		add_valp(valp, &sptr->valp_head);
	}
}

static val_t parse_number(const char *str)
{
	val_t dec, hex;

	if (str[0] == '\0')
		return 0;

	sscanf(str, "%d", &dec);
	sscanf(str, "%x", &hex);

	return dec == 0 ? hex : dec;
}

static void fill_constant(const char *str, val_t *valp)
{
	if (str[0] == '\0')
		*valp = 0;
	else if (str[0] == '$')
		*valp = parse_number(str + 1);
	else if (isdigit(str[0]) || str[0] == '-' || str[0] == '+')
		*valp = parse_number(str);
	else
		lookup_symbol(str, valp);
}

static regid_t parse_memory(char *str, val_t *valp)
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

	*valp = parse_number(str);
	regid = parse_regid(x + 1);

	*x = '(';
	*y = ')';

	return regid;
}

/**
 * fill__(args, regp, valp)
 *
 *  _i: instruction
 *  _r: register
 *  _v: constant
 *  _m: memory
 *
 * fill *regp and *valp according to args.
 */
static void fill_i(char **args, reg_t *regp, val_t *valp)
{
	return;
}

static void fill_i_r_r(char **args, reg_t *regp, val_t *valp)
{
	*regp = pack_reg(parse_regid(args[1]), parse_regid(args[2]));
}

static void fill_i_v_r(char **args, reg_t *regp, val_t *valp)
{
	fill_constant(args[1], valp);
	*regp = pack_reg(R_NONE, parse_regid(args[2]));
}

static void fill_i_r_m(char **args, reg_t *regp, val_t *valp)
{
	*regp = pack_reg(parse_regid(args[1]), parse_memory(args[2], valp));
}

static void fill_i_m_r(char **args, reg_t *regp, val_t *valp)
{
	*regp = pack_reg(parse_regid(args[2]), parse_memory(args[1], valp));
}

static void fill_i_v(char **args, reg_t *regp, val_t *valp)
{
	fill_constant(args[1], valp);
}

static void fill_i_r(char **args, reg_t *regp, val_t *valp)
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
static int icode_argn(icode_t icode)
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

static void (*icode_filler[])(char **, reg_t *, val_t *) = {
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

static val_t assembler(char **args, byte *base)
{
	static val_t s_offset = 0;	/* at the start of next instruction */
	static val_t e_offset = 0;	/* after the end of last instruction */
	byte *pos = base + s_offset;
	ins_t ins;
	icode_t icode;
	ins_t *insp = NULL;
	reg_t *regp = NULL;
	val_t *valp = NULL;
	val_t tmp;

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
		if (strcmp(args[0], ".long") == 0) {
			valp = (val_t *)pos;
			fill_constant(args[1], valp);
			pos = (byte *)(valp + 1);
			s_offset = e_offset = pos - base;
		} else if (strcmp(args[0], ".pos") == 0) {
			tmp = parse_number(args[1]);
			s_offset = tmp;
		} else if (strcmp(args[0], ".align") == 0) {
			tmp = parse_number(args[1]);
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
	icode = ins_icode(ins);

	insp = (ins_t *)pos;
	pos = (byte *)(insp + 1);
	if (need_reg(icode)) {
		regp = (reg_t *)pos;
		pos = (byte *)(regp + 1);
	}
	if (need_val(icode)) {
		valp = (val_t *)pos;
		pos = (byte *)(valp + 1);
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
	icode_filler[icode](args, regp, valp);

	return e_offset;
}

#define MAXBIN 4000
#define BUFSIZE 1000
#define MAXARGN 8

#define max(x, y) ((x) < (y) ? (y) : (x))

static byte binary[MAXBIN];
static val_t bin_size;

static char **parse_line(char *str)
{
	static char *args[MAXARGN];
	char *token;
	int n = 0;

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
	val_t e_offset;

	bin_size = 0;

	while (fgets(buf, BUFSIZE, in) != NULL) {
		argp = parse_line(buf);
		e_offset = assembler(argp, binary);
		bin_size = max(e_offset, bin_size);
	}
}

int main(int argc, char *argv[])
{
	FILE *input, *output;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: %s <input> [<output>]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input = fopen(argv[1], "r");
	driver(input);
	fclose(input);

	if (argc == 3) {
		output = fopen(argv[2], "w");
	} else {
		output = fopen("y.out", "w");
	}
	fwrite(binary, sizeof(byte), bin_size, output);
	fclose(output);
	exit(EXIT_SUCCESS);
}
