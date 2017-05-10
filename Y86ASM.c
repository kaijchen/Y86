#include "Y86ASM.h"

#define MAXLINE 2000
#define BUFSIZE 1000
#define MAXARGS 8

static char *line_args[MAXLINE][MAXARGS];
static size_t line_argn[MAXLINE];

static size_t parse_line(char *str, char **args)
{
	char *token;
	size_t n = 0;

	while ((token = strsep(&str, "\n\t ,")) != NULL) {

		/* skip commits, ensure within limit */
		if (token[0] == '#' || n + 1 == MAXARGS)
			break;

		/* skip empty fields */
		if (token[0] == '\0')
			continue;

		args[n++] = strdup(token);
	}

	/* null terminate */
	args[n] = NULL;

	return n;
}

static size_t preprocesser(FILE *in)
{
	char buf[BUFSIZE];
	char **argp;
	size_t len, argn, line;
	uint32 file_offset, tmp, imm;
	uint8 ins, type, flag, size;

	for (line = 0; line < MAXLINE
			&& fgets(buf, BUFSIZE, in) != NULL; line++) {

		argn = line_argn[line] = parse_line(buf, line_args[line]);
		argp = line_args[line];

		if (argn == 0)
			continue;

		len = strlen(argp[0]);
		if (argp[0][len - 1] == ':') {	/* symbol */
			argp[0][len - 1] = '\0';
			add_symbol(argp[0], file_offset);
			argp[0][len - 1] = ':';
			argp++;
			if (--argn == 0)
				continue;
		}

		ins = parse_instruction(argp[0]);
		type = unpack_h(ins);
		flag = Y86_INS_TYPE[type].flag;
		size = code_size(flag);

		if (argn != Y86_INS_TYPE[type].argn)
			error("instruction syntax error", argp[0]);

		/* Y86_INS_TYPE[type].parse(argp, &reg, &imm); */

		switch (type) {
		case I_ERR:
			break;
		case I_POS:
			imm = parse_number(argp[1]);
			file_offset = imm;
			break;
		case I_ALIGN:
			imm = parse_number(argp[1]);
			if ((tmp = file_offset % imm) != 0)
				file_offset += imm - tmp;
			break;
		default:
			file_offset += size;
			/**
			 * code = encode(flag, ins, reg, imm);
			 * for (ptr = (uint8 *)&code; size--; ptr++)
			 *      printf(size ? "%02x " : "%02x\n", ptr[0]);
			 */
		}
	}
	return line;
}

static void assembler(FILE *out, size_t maxline)
{
	char **argp;
	size_t len, argn, line;
	uint64 code;
	uint32 tmp, imm;
	uint8 ins, reg, type, flag, size;
	uint8 *ptr;

	for (line = 0; line < maxline; line++) {

		argn = line_argn[line];
		argp = line_args[line];

		if (argn == 0)
			continue;

		len = strlen(argp[0]);
		if (argp[0][len - 1] == ':') {	/* symbol */
			argp++;
			if (--argn == 0)
				continue;
		}

		ins = parse_instruction(argp[0]);
		type = unpack_h(ins);
		flag = Y86_INS_TYPE[type].flag;
		size = code_size(flag);

		if (argn != Y86_INS_TYPE[type].argn)
			error("instruction syntax error", argp[0]);

		Y86_INS_TYPE[type].parse(argp, &reg, &imm);

		switch (type) {
		case I_ERR:
			break;
		case I_POS:
			fseek(out, imm, SEEK_SET);
			break;
		case I_ALIGN:
			if ((tmp = ftell(out) % imm) != 0)
				fseek(out, imm - tmp, SEEK_CUR);
			break;
		default:
			code = encode(flag, ins, reg, imm);
			for (ptr = (uint8 *)&code; size--; ptr++) {
			     printf(size ? "%02x " : "%02x\n", ptr[0]);
			     putc(ptr[0], out);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	FILE *fileout;

	fileout = fopen("y.out", "w");
	assembler(fileout, preprocesser(stdin));
	fclose(fileout);
	exit(EXIT_SUCCESS);
}
