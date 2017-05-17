#include "lib/Y86.h"
#include <stdio.h>
#include <string.h>

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

		/* skip commits, ensure within limit */
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
	size_t w_offset;

	bin_size = 0;

	while (fgets(buf, BUFSIZE, in) != NULL) {
		argp = parse_line(buf);
		w_offset = assembler(argp, binary);
		bin_size = max(w_offset, bin_size);
	}
}

static void writeout(FILE *out)
{
	for (size_t i = 0; i < bin_size; i++)
		putc(binary[i], out);
}

int main(int argc, char *argv[])
{
	FILE *fileout;

	driver(stdin);

	fileout = fopen("y.out", "w");
	writeout(fileout);
	fclose(fileout);
	exit(EXIT_SUCCESS);
}
