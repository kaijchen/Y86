#include "Y86ASM.h"
#include <stdio.h>
#include <string.h>

#define BINSIZE 4000
#define BUFSIZE 1000
#define MAXARGS 8

static uint8 mem[BINSIZE];

static char **parse_line(char *str)
{
	static char *args[MAXARGS];
	char *token;
	size_t n = 0;

	while ((token = strsep(&str, "\n\t ,")) != NULL) {

		/* skip commits, ensure within limit */
		if (token[0] == '#' || n + 1 == MAXARGS)
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

static size_t assembler(FILE *in)
{
	char buf[BUFSIZE];
	char **argp;
	size_t len, line;
	uint32 offset, tmp, imm, size;
	uint8 ins, type, argn;

	uint32 *immp;
	uint8 *regp;
	uint8 *insp;

	for (line = 0; fgets(buf, BUFSIZE, in) != NULL; line++) {

		argp = parse_line(buf);

		if (*argp == NULL)
			continue;

		len = strlen(argp[0]);
		if (argp[0][len - 1] == ':') {	/* symbol */
			argp[0][len - 1] = '\0';
			add_symbol(argp[0], offset);
			if (*++argp == NULL)
				continue;
		}

		ins = parse_instruction(argp[0]);
		type = ins_type(ins);
		size = code_size(ins);

		for (argn = 0; argp[argn] != NULL; argn++)
			;
		if (argn != arg_number(ins))
			error("instruction syntax error", argp[0]);

		switch (type) {
		case I_ERR:
			break;
		case I_POS:
			imm = parse_number(argp[1]);
			offset = imm;
			break;
		case I_ALIGN:
			imm = parse_number(argp[1]);
			if ((tmp = offset % imm) != 0)
				offset += imm - tmp;
			break;
		default:
			offset += size;
			code_parser(ins, &mem[offset], &insp, &regp, &immp);
			code_writer(ins, argp, insp, regp, immp);
		}
	}
	return line;
}

static void writeout(FILE *out)
{
	return;
}

int main(int argc, char *argv[])
{
	FILE *fileout;

	assembler(stdin);

	fileout = fopen("y.out", "w");
	writeout(fileout);
	fclose(fileout);
	exit(EXIT_SUCCESS);
}
