#include "lib/Y86.h"
#include <stdio.h>
#include <string.h>

#define BINSIZE 4000
#define BUFSIZE 1000
#define MAXARGS 8

#define max(x, y) ((x) < (y) ? (y) : (x))

static byte binary[BINSIZE];
static size_t max_offset;

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

static void driver(FILE *in)
{
	char buf[BUFSIZE];
	char **argp;
	size_t len, line, offset;

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

		offset = assembler(argp, binary);
		max_offset = max(offset, max_offset);
	}
}

static void writeout(FILE *out)
{
	for (size_t i = 0; i < max_offset; i++)
		putc(binary[i], out);
}

int main(int argc, char *argv[])
{
	FILE *fileout;

	max_offset = 0;
	driver(stdin);

	fileout = fopen("y.out", "w");
	writeout(fileout);
	fclose(fileout);
	exit(EXIT_SUCCESS);
}
