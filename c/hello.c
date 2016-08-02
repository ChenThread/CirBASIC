/*
CirBASIC Hello profile
C Reference Implementation
by Ben "GreaseMonkey" Russell, 2016 - Public Domain
*/

#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

// normal implementations will use a key-value list
char *lines[65536];
bool run_mode = false;
int run_line = 0;

void do_error(const char *s)
{
	printf("ERR: %s\n", s);
	run_mode = false;
}

void exec_line(const char *s)
{
	if(!strcasecmp(s, "RUN")) {
		run_mode = true;
		run_line = 0;
	} else {
		printf("line: %s\n", s);
	}
}

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;

	char ebuf[257];

	printf("CirBASIC Hello Profile\n");
	printf("C99 Ref Impl\n");

	// Setup
	for(int i = 0; i < 65536; i++) {
		lines[i] = NULL;
	}

	// Main loop
	printf("READY\n");
	for(;;) {
		if(run_mode) {
			// Run mode

			// Range check
			if(run_line < 0 || run_line >= 65536) {
				printf("\nREADY\n");
				run_mode = false;
				continue;
			}

			// Get line and run it
			const char *line_str = lines[run_line++];
			if(line_str != NULL) {
				exec_line(line_str);
			}

		} else {
			// Immediate mode

			fgets(ebuf, sizeof(ebuf), stdin);
			size_t eblen = strlen(ebuf);

			// Strip newline chars
			while(eblen >= 1 && ebuf[eblen-1] == '\n') {
				ebuf[--eblen] = '\x00';
			}

			// Start our pointer
			char *ps = &ebuf[0];

			// Skip whitespace
			ps += strspn(ps, "\t ");

			// Ignore empty lines
			if(eblen < 1) {
				continue;
			}

			// Ensure we didn't overflow our input buffer
			if(eblen > 255) {
				do_error("eval buf overflow");
				continue;
			}

			// Check if we have a line number
			if(*ps >= '0' && *ps <= '9') {
				// Ensure number is valid
				size_t lnumlen = strspn(ps, "0123456789");

				// Parse number
				char *lnumend = ps+lnumlen;
				long int lnum_l = strtol(ps, &lnumend, 10);
				if(lnum_l < (long int)0 || lnum_l >= (long int)65536) {
					do_error("linenum overflow");
					continue;
				}
				int lnum = (int)lnum_l;

				// Skip number + whitespace
				ps += lnumlen;
				size_t spacelen = strspn(ps, "\t ");
				ps += spacelen;

				// Determine course of action
				if(*ps == '\x00') {
					// Clear this line
					if(lines[lnum] != NULL) {
						free(lines[lnum]);
						lines[lnum] = NULL;
					}

				} else if(spacelen >= 1) {
					// Clear any line that is already here
					if(lines[lnum] != NULL) {
						free(lines[lnum]);
					}

					// Set up a line here
					size_t lslen = strlen(ps);
					lines[lnum] = malloc(lslen+1);
					memcpy(lines[lnum], ps, lslen+1);

				} else {
					do_error("need space after linenum");
					continue;
				}

			} else {
				exec_line(ps);

			}
		}
	}

	return 0;
}

