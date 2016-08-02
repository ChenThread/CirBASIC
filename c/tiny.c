/*
CirBASIC Tiny profile
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

#define WSSTR "\t "
#define NUMSTR "0123456789"

// normal implementations will use a key-value list
char *lines[65536];
int ivars[26];
char ebuf[257];
bool run_mode = false;
int run_last_line = 0;
int run_line = 0;
#define LINE_STACK_MAX 63
int line_stack_list[LINE_STACK_MAX];
int line_stack_ptr = 0;

void do_error(const char *s)
{
	printf("ERR: %s\n", s);
	if(run_mode) {
		printf("BREAK at %d\n", run_last_line);
	}
	run_mode = false;
	line_stack_ptr = 0;
}

int exec_expr(char **rs);

int exec_term(char **rs)
{
	// Skip whitespace
	*rs += strspn(*rs, WSSTR);

	// Branch out
	if(**rs == '(') {
		// Skip whitespace
		(*rs)++;
		*rs += strspn(*rs, WSSTR);

		// Get subexpr
		int num = exec_expr(rs);
		if(*rs == NULL) {
			return 0;
		}

		// Validate syntax
		*rs += strspn(*rs, WSSTR);
		if(**rs != ')') {
			do_error("syntax error");
			*rs = NULL;
			return 0;
		}

		// Skip whitespace
		(*rs)++;
		*rs += strspn(*rs, WSSTR);
		return num;

	} else if((**rs|0x20) >= 'a' && (**rs|0x20) <= 'z') {
		// Get variable name
		char varch = (**rs|0x20);
		int varidx = varch-'a';
		(*rs)++;

		// Get variable
		assert(varidx >= 0 && varidx < 26);
		int num = ivars[varidx];

		// Skip whitespace
		*rs += strspn(*rs, WSSTR);
		return num;

	} else if((**rs >= '0' && **rs <= '9') || **rs == '-' || **rs == '+') {
		bool is_neg = (**rs == '-');
		if(**rs == '-' || **rs == '+') {
			(*rs)++;
			if(!(**rs >= '0' && **rs <= '9')) {
				*rs = NULL;
				do_error("syntax error");
				return 0;
			}
		}

		// Parse number
		size_t numlen = strspn(*rs, NUMSTR);
		if(numlen < 1) {
			do_error("syntax error");
			return 0;
		}

		char *numend = *rs+numlen;
		int num = (int)strtol(*rs, &numend, 10);
		if(is_neg) {
			num = -num;
		}

		*rs += numlen;

		// Skip whitespace
		*rs += strspn(*rs, WSSTR);
		return num;

	} else {
		*rs = NULL;
		do_error("syntax error");
		return 0;
	}
}

int exec_op2(char **rs)
{
	// Parse
	int num = exec_term(rs);
	if(*rs == NULL) { return 0; };

	for(;;) {
		// Skip whitespace
		*rs += strspn(*rs, WSSTR);

		// Check if binop
		if(**rs == '*') {
			// Multiply
			(*rs)++;
			*rs += strspn(*rs, WSSTR);
			num *= exec_term(rs);
			if(*rs == NULL) { return 0; };

		} else if(**rs == '/') {
			// Divide
			(*rs)++;
			*rs += strspn(*rs, WSSTR);
			num /= exec_term(rs);
			if(*rs == NULL) { return 0; };

		} else if(**rs == '%') {
			// Modulo
			(*rs)++;
			*rs += strspn(*rs, WSSTR);
			num %= exec_term(rs);
			if(*rs == NULL) { return 0; };

		} else {
			// Pass back
			return num;
		}
	}

	return num;
}

int exec_op1(char **rs)
{
	// Parse
	int num = exec_op2(rs);
	if(*rs == NULL) { return 0; };

	for(;;) {
		// Skip whitespace
		*rs += strspn(*rs, WSSTR);

		// Check if binop
		if(**rs == '+') {
			// Add
			(*rs)++;
			*rs += strspn(*rs, WSSTR);
			num += exec_op2(rs);
			if(*rs == NULL) { return 0; };

		} else if(**rs == '-') {
			// Subtract
			(*rs)++;
			*rs += strspn(*rs, WSSTR);
			num -= exec_op2(rs);
			if(*rs == NULL) { return 0; };

		} else {
			// Pass back
			return num;
		}
	}

	return num;
}

int exec_expr(char **rs)
{
	return exec_op1(rs);
}

void exec_stmt(char *s)
{
	if(!strcasecmp(s, "END")) {
		run_mode = false;
		printf("\nREADY\n");

	} else if((!strncasecmp(s, "GOTO", 4)) || (!strncasecmp(s, "GOSUB", 5))) {
		bool is_gosub = ((s[2]|0x20) == 's'); // goSub vs goTo
		s += (is_gosub ? 5 : 4);
		if(*s != '\x00' && *s != '\t' && *s != ' ') {
			do_error("syntax error");
			return;
		}
		s += strspn(s, WSSTR);

		// Parse expr
		int lnum = exec_expr(&s);
		if(s == NULL) { return; }

		if(lnum < 0 || lnum >= 65536) {
			do_error("linenum overflow");
			return;
		}

		// Validate syntax
		s += strspn(s, WSSTR);
		if(*s != '\x00') {
			do_error("syntax error");
			return;
		}

		// Apply GOSUB if necessary
		if(is_gosub) {
			if(line_stack_ptr >= LINE_STACK_MAX) {
				do_error("GOSUB overflow");
				return;
			}
			assert(line_stack_ptr >= 0 && line_stack_ptr < LINE_STACK_MAX);
			line_stack_list[line_stack_ptr++] = run_line;
		}

		run_line = lnum;

	} else if((!strncasecmp(s, "IF", 2))) {
		s += 2;
		if(*s != '\x00' && *s != '\t' && *s != ' ') {
			do_error("syntax error");
			return;
		}
		s += strspn(s, WSSTR);

		// Parse expr 1
		int num1 = exec_expr(&s);
		if(s == NULL) { return; }

		// Parse relop
		bool result = false;
		char *relop = s;
		size_t reloplen = strspn(s, "<>=");
		if(reloplen < 1) {
			do_error("syntax error");
			return;
		}
		s += reloplen;
		s += strspn(s, WSSTR);

		// Parse expr 2
		int num2 = exec_expr(&s);
		if(s == NULL) { return; }

		// Check for "THEN"
		if(strncasecmp(s, "THEN", 4)) {
			do_error("syntax error");
			return;
		}
		s += 4;
		if(*s != '\x00' && *s != '\t' && *s != ' ') {
			do_error("syntax error");
			return;
		}
		s += strspn(s, WSSTR);

		// Scan through ops
		for(;;) {
			if(*relop == '<') {
				result = result || (num1 < num2);
			} else if(*relop == '>') {
				result = result || (num1 > num2);
			} else if(*relop == '=') {
				result = result || (num1 == num2);
			} else {
				break;
			}

			relop++;
		}

		// Run if result passes
		if(result) {
			exec_stmt(s);
		}

	} else if((!strncasecmp(s, "INPUT", 5))) {
		s += 5;
		if(*s != '\x00' && *s != '\t' && *s != ' ') {
			do_error("syntax error");
			return;
		}
		s += strspn(s, WSSTR);

		// Parse var name
		if((*s|0x20) < 'a' || (*s|0x20) > 'z') {
			do_error("syntax error");
			return;
		}

		char varch = (*s|0x20);
		int varidx = varch-'a';
		s++;

		// Validate syntax
		s += strspn(s, WSSTR);
		if(*s != '\x00') {
			do_error("syntax error");
			return;
		}

		// Input variable
		printf("? ");
		fflush(stdout);
		fgets(ebuf, sizeof(ebuf), stdin);
		int num = (int)strtol(ebuf, NULL, 10);

		// Set variable
		assert(varidx >= 0 && varidx < 26);
		ivars[varidx] = num;

	} else if((!strncasecmp(s, "LET", 3))) {
		s += 3;
		if(*s != '\x00' && *s != '\t' && *s != ' ') {
			do_error("syntax error");
			return;
		}
		s += strspn(s, WSSTR);

		// Parse var name
		if((*s|0x20) < 'a' || (*s|0x20) > 'z') {
			do_error("syntax error");
			return;
		}

		char varch = (*s|0x20);
		int varidx = varch-'a';
		s++;

		// Skip to '='
		s += strspn(s, WSSTR);
		if(*s != '=') {
			do_error("syntax error");
			return;
		}
		s++;

		// Parse expr
		int num = exec_expr(&s);
		if(s == NULL) { return; }

		// Validate syntax
		s += strspn(s, WSSTR);
		if(*s != '\x00') {
			do_error("syntax error");
			return;
		}

		// Set variable
		assert(varidx >= 0 && varidx < 26);
		ivars[varidx] = num;

	} else if(!strcasecmp(s, "LIST")) {
		for(int i = 0; i < 65536; i++) {
			if(lines[i] != NULL) {
				printf("%5d %s\n", i, lines[i]);
			}
		}

	} else if(!strcasecmp(s, "NEW")) {
		for(int i = 0; i < 65536; i++) {
			if(lines[i] != NULL) {
				free(lines[i]);
				lines[i] = NULL;
			}
		}

	} else if((!strncasecmp(s, "PRINT", 5))) {
		s += 5;
		if(*s != '\x00' && *s != '\t' && *s != ' ') {
			do_error("syntax error");
			return;
		}
		s += strspn(s, WSSTR);

		// Parse expr
		int num = exec_expr(&s);
		if(s == NULL) { return; }

		// Validate syntax
		s += strspn(s, WSSTR);
		if(*s != '\x00') {
			do_error("syntax error");
			return;
		}

		// Print
		printf("%d\n", num);

	} else if(!strncasecmp(s, "REM", 3)) {
		s += 3;
		if(*s != '\x00' && *s != '\t' && *s != ' ') {
			do_error("syntax error");
			return;
		}

	} else if(!strcasecmp(s, "RETURN")) {
		line_stack_ptr--;
		if(line_stack_ptr < 0) {
			do_error("RETURN underflow");
			return;
		}
		assert(line_stack_ptr >= 0 && line_stack_ptr < LINE_STACK_MAX);
		run_line = line_stack_list[line_stack_ptr]+1;

	} else if(!strcasecmp(s, "RUN")) {
		run_mode = true;
		run_line = 0;
		run_last_line = 0;

	} else {
		do_error("syntax error");
	}
}

void exec_line(char *s)
{
	exec_stmt(s);
}

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;

	printf("CirBASIC Tiny Profile\n");
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
			run_last_line = run_line;
			char *line_str = lines[run_line++];
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
			ps += strspn(ps, WSSTR);

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
				size_t lnumlen = strspn(ps, NUMSTR);

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
				size_t spacelen = strspn(ps, WSSTR);
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

