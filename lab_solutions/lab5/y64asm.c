#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y64asm.h"

line_t *line_head = NULL;
line_t *line_tail = NULL;
int lineno = 0;

#define err_print(_s, _a ...) do { \
  if (lineno < 0) \
    fprintf(stderr, "[--]: "_s"\n", ## _a); \
  else \
    fprintf(stderr, "[L%d]: "_s"\n", lineno, ## _a); \
} while (0);


int64_t vmaddr = 0;    /* vm addr */

/* register table */
const reg_t reg_table[REG_NONE] = {
    {"%rax", REG_RAX, 4},
    {"%rcx", REG_RCX, 4},
    {"%rdx", REG_RDX, 4},
    {"%rbx", REG_RBX, 4},
    {"%rsp", REG_RSP, 4},
    {"%rbp", REG_RBP, 4},
    {"%rsi", REG_RSI, 4},
    {"%rdi", REG_RDI, 4},
    {"%r8",  REG_R8,  3},
    {"%r9",  REG_R9,  3},
    {"%r10", REG_R10, 4},
    {"%r11", REG_R11, 4},
    {"%r12", REG_R12, 4},
    {"%r13", REG_R13, 4},
    {"%r14", REG_R14, 4}
};
const reg_t* find_register(char *name)
{
    int i;
    for (i = 0; i < REG_NONE; i++)
        if (!strncmp(name, reg_table[i].name, reg_table[i].namelen))
            return &reg_table[i];
    return NULL;
}


/* instruction set */
instr_t instr_set[] = {
    {"nop", 3,   HPACK(I_NOP, F_NONE), 1 },
    {"halt", 4,  HPACK(I_HALT, F_NONE), 1 },
    {"rrmovq", 6,HPACK(I_RRMOVQ, F_NONE), 2 },
    {"cmovle", 6,HPACK(I_RRMOVQ, C_LE), 2 },
    {"cmovl", 5, HPACK(I_RRMOVQ, C_L), 2 },
    {"cmove", 5, HPACK(I_RRMOVQ, C_E), 2 },
    {"cmovne", 6,HPACK(I_RRMOVQ, C_NE), 2 },
    {"cmovge", 6,HPACK(I_RRMOVQ, C_GE), 2 },
    {"cmovg", 5, HPACK(I_RRMOVQ, C_G), 2 },
    {"irmovq", 6,HPACK(I_IRMOVQ, F_NONE), 10 },
    {"rmmovq", 6,HPACK(I_RMMOVQ, F_NONE), 10 },
    {"mrmovq", 6,HPACK(I_MRMOVQ, F_NONE), 10 },
    {"addq", 4,  HPACK(I_ALU, A_ADD), 2 },
    {"subq", 4,  HPACK(I_ALU, A_SUB), 2 },
    {"andq", 4,  HPACK(I_ALU, A_AND), 2 },
    {"xorq", 4,  HPACK(I_ALU, A_XOR), 2 },
    {"jmp", 3,   HPACK(I_JMP, C_YES), 9 },
    {"jle", 3,   HPACK(I_JMP, C_LE), 9 },
    {"jl", 2,    HPACK(I_JMP, C_L), 9 },
    {"je", 2,    HPACK(I_JMP, C_E), 9 },
    {"jne", 3,   HPACK(I_JMP, C_NE), 9 },
    {"jge", 3,   HPACK(I_JMP, C_GE), 9 },
    {"jg", 2,    HPACK(I_JMP, C_G), 9 },
    {"call", 4,  HPACK(I_CALL, F_NONE), 9 },
    {"ret", 3,   HPACK(I_RET, F_NONE), 1 },
    {"pushq", 5, HPACK(I_PUSHQ, F_NONE), 2 },
    {"popq", 4,  HPACK(I_POPQ, F_NONE),  2 },
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1 },
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2 },
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4 },
    {".quad", 5, HPACK(I_DIRECTIVE, D_DATA), 8 },
    {".pos", 4,  HPACK(I_DIRECTIVE, D_POS), 0 },
    {".align", 6,HPACK(I_DIRECTIVE, D_ALIGN), 0 },
    {NULL, 1,    0   , 0 } //end
};

instr_t *find_instr(char *name)
{
    int i;
    for (i = 0; instr_set[i].name; i++)
	if (strncmp(instr_set[i].name, name, instr_set[i].len) == 0)
	    return &instr_set[i];
    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{
	// declaration	
	symbol_t *temp = symtab -> next;

        while (temp != NULL) {
                if (!strcmp(temp -> name, name)) { // comparing 2 strings to see if they have same characters
                        return temp;
                }

                temp = temp -> next;
        }

        return NULL;
}

/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)
{
	/* check duplicate */
        if (find_symbol(name) != NULL) {
                return -1;
        }
	
        /* create new symbol_t (don't forget to free it)*/
        symbol_t *temp = (symbol_t *)malloc(sizeof(symbol_t));
	
        /* add the new symbol_t to symbol table */
	temp -> addr = vmaddr;
	temp -> name = name;
	temp -> next = symtab -> next;
	symtab -> next = temp;

	return 0;
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
void add_reloc(char *name, bin_t *bin)
{
	/* create new reloc_t (don't forget to free it)*/
        reloc_t *temp = (reloc_t *)malloc(sizeof(reloc_t));
	temp -> name = name;
        temp -> y64bin = bin;

        /* add the new reloc_t to relocation table */
        temp -> next = reltab -> next;
	reltab -> next = temp;
}


/* macro for parsing y64 assembly code */
#define IS_DIGIT(s) ((*(s)>='0' && *(s)<='9') || *(s)=='-' || *(s)=='+')
#define IS_LETTER(s) ((*(s)>='a' && *(s)<='z') || (*(s)>='A' && *(s)<='Z'))
#define IS_COMMENT(s) (*(s)=='#')
#define IS_REG(s) (*(s)=='%')
#define IS_IMM(s) (*(s)=='$')

#define IS_BLANK(s) (*(s)==' ' || *(s)=='\t')
#define IS_END(s) (*(s)=='\0')

#define SKIP_BLANK(s) do {  \
  while(!IS_END(s) && IS_BLANK(s))  \
    (s)++;    \
} while(0);

/* return value from different parse_xxx function */
typedef enum { PARSE_ERR=-1, PARSE_REG, PARSE_DIGIT, PARSE_SYMBOL, 
    PARSE_MEM, PARSE_DELIM, PARSE_INSTR, PARSE_LABEL} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovq')
 * args
 *     ptr: point to the start of string
 *     inst: point to the inst_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
	/* skip the blank */
	char *first = *ptr;

	SKIP_BLANK(first);

	if (IS_END(first)) {
		return PARSE_ERR;
	}

	/* find_instr and check end */
	instr_t *temp = find_instr(first);

	if (temp == NULL) {
		return PARSE_ERR;
	}
	
	first += temp -> len; // increment pointer to end of instruction

	if (!IS_END(first) && !IS_BLANK(first)) {
		return PARSE_ERR;
	}

	/* set 'ptr' and 'inst' */
	*inst = temp;
	*ptr = first;

	return PARSE_INSTR;
}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)
{
	/* skip the blank and check */
	char *temp = *ptr;

	SKIP_BLANK(temp);

	if (IS_END(temp)) {
		return PARSE_ERR;
	}
	
	/* set 'ptr' */
	if (*temp == delim) {
		temp++; // increment pointer to after ','
		*ptr = temp;

		return PARSE_DELIM;
	} else {
		return PARSE_ERR;
	}
}

/*
 * parse_reg: parse an expected register token (e.g., '%rax')
 * args
 *     p tr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token, 
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
	/* skip the blank and check */
	reg_t *registerTemp = (reg_t *)malloc(sizeof(reg_t));

	SKIP_BLANK(*ptr);

	if (!IS_REG(*ptr) || IS_END(*ptr)) { // check register
		return PARSE_ERR;
	}       
	
	/* find register */
	registerTemp = find_register(*ptr); // ignore constant

	if (registerTemp == NULL) {
		return PARSE_ERR;
	}
	 
	/* set 'ptr' and 'regid' */
	(*ptr) += registerTemp -> namelen;
	*regid = registerTemp -> id;
	
 	return PARSE_REG;
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
	/* skip the blank and check */
	char *temp = *ptr;
	int counter = 0;

	SKIP_BLANK(temp);
	SKIP_BLANK(*ptr); // add this to prevent error

	if (IS_END(temp) || !IS_LETTER(temp)) {
		return PARSE_ERR;
	}

	/* allocate name and copy to it */
    	while (IS_LETTER(temp) || IS_DIGIT(temp)) {
		counter++;
		temp++;
	}

    	char *c = malloc(sizeof(char) * (counter+1));
	memset(c, '\0', counter+1); // set string ending
    	memcpy(c, *ptr, counter); // copy string
	
	/* set 'ptr' and 'name' */
	*ptr = temp;
	*name = c;

	return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *value)
{
	/* skip the blank and check */
	char *temp = *ptr;

	SKIP_BLANK(temp);

	if (!IS_DIGIT(temp) || IS_END(temp)) {
		return PARSE_ERR;
	}

	/* calculate the digit, (NOTE: see strtoll()) */
	char *temp2 = temp;
	*value = strtoull(temp, &temp2, 0); // use strtoull instead to prevent overflow

	/* set 'ptr' and 'value' */
	*ptr = temp2;

	return PARSE_DIGIT;
}

/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *value)
{
	/* skip the blank and check */
	parse_t r;
	char *temp = *ptr;

	SKIP_BLANK(temp);

	if (IS_END(temp) || (!IS_IMM(temp) && !IS_LETTER(temp))) {
		return PARSE_ERR;
	}

	/* if IS_IMM, then parse the digit */
	if (IS_IMM(temp)) {
		temp++; // skip the $ sign
		r = parse_digit(&temp, value);

		if (r == PARSE_ERR) {
			return PARSE_ERR;
		}
	}

	/* if IS_LETTER, then parse the symbol */
	if (IS_LETTER(temp)) {
		r = parse_symbol(&temp, name);

		if (r == PARSE_ERR) {
			return PARSE_ERR;
		}
	}

	/* set 'ptr' and 'name' or 'value' */
	*ptr = temp;

	return r;
}

/*
 * parse_mem: parse an expected memory token (e.g., '8(%rbp)')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_MEM: success, move 'ptr' to the first char after token,
 *                          and store the value of digit to 'value',
 *                          and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr', 'value' and 'regid' are undefined
 */
parse_t parse_mem(char **ptr, long *value, regid_t *regid)
{
	/* skip the blank and check */
	char *temp = *ptr;
	*value = 0;

	SKIP_BLANK(temp);
	
	if (IS_END(temp)) {
		return PARSE_ERR;
	}

	if (IS_DIGIT(temp)) {
		char *temp2 = temp;
		*value = strtol(temp, &temp2, 0); // convert string to long
		temp = temp2;
	}

	/* calculate the digit and register, (ex: (%rbp) or 8(%rbp)) */
	if (*temp == '(') {
		temp++;

		if (parse_reg(&temp, regid) == PARSE_ERR) {
			return PARSE_ERR;
		} else if (*temp != ')') { // check for close bracket after parsing register
			return PARSE_ERR;
		}

		temp++;
	}

	/* set 'ptr', 'value' and 'regid' */
	if (temp == *ptr) {
		return PARSE_ERR;
	}
	
	*ptr = temp;

	return PARSE_MEM;
}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *value)
{
	/* skip the blank and check */
	char *temp = *ptr;

	SKIP_BLANK(temp);

	if (IS_END(temp)) {
		return PARSE_ERR;
	}

	/* if IS_DIGIT, then parse the digit */
	if (IS_DIGIT(temp)) {
		if (parse_digit(&temp, value) == PARSE_ERR) {
			return PARSE_DIGIT;
		}

		*ptr = temp;

		return PARSE_DIGIT;
	}

	/* if IS_LETTER, then parse the symbol */
	if (IS_LETTER(temp)) {
		if (parse_symbol(&temp, name) == PARSE_ERR) {
			return PARSE_ERR;
		}

		*ptr = temp;

		return PARSE_SYMBOL;
	}

	/* set 'ptr', 'name' and 'value' */

	return PARSE_ERR;
}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name)
{
	/* skip the blank and check */
	// declarations
	char *temp = *ptr;
	int counter = 0;

	SKIP_BLANK(temp);
	SKIP_BLANK(*ptr); // add this to prevent error

	if (IS_END(temp) || (find_instr(temp) != NULL)) {
		return PARSE_ERR;
	}

	/* allocate name and copy to it */
	while (IS_LETTER(temp) || IS_DIGIT(temp)) {
		counter++; // get counter to know length of string to copy
		temp++;
	}
	
	/* set 'ptr' and 'name' */
	if ((*temp) == ':') {
		char *newLabel = malloc(counter+1);
		memset(newLabel, '\0', counter+1); // add end of string
		memcpy(newLabel, *ptr, counter); // copy string
		temp++; // skip ':' after parsing
		*ptr = temp;
		*name = newLabel;

		return PARSE_LABEL;
	}

	return PARSE_ERR;
}

/*
 * parse_line: parse a line of y64 code (e.g., 'Loop: mrmovq (%rcx), %rsi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y64 assembly code
 *
 * return
 *     PARSE_XXX: success, fill line_t with assembled y64 code
 *     PARSE_ERR: error, try to print err information (e.g., instr type and line number)
 */
type_t parse_line(line_t *line)
{
	// initialisation
	bin_t *y64bin = &line -> y64bin;
	char *y64asm = (char *)malloc(strlen(line -> y64asm) + 1);
	strcpy(y64asm, line -> y64asm); // make sure both strings of line are equal
	char *label = NULL;
	instr_t *instruction = NULL;
	char *temp = y64asm;
	
/* when finish parse an instruction or lable, we still need to continue check 
* e.g., 
*  Loop: mrmovl (%rbp), %rcx
*           call SUM  #invoke SUM function */
	
	loop: ;

	/* skip blank and check IS_END */
	SKIP_BLANK(temp);

	if (IS_END(temp)) {
		goto end;
	}

	/* is a comment ? */
	if (IS_COMMENT(temp)) {
		goto end;
	}

	/* is a label ? */
	if (parse_label(&temp, &label) == PARSE_LABEL) {
		if (add_symbol(label) < 0) {
			line -> type = TYPE_ERR;
			err_print("Dup symbol:%s", label);

			goto end;
		}

		line -> type = TYPE_INS;
		line -> y64bin.addr = vmaddr;

		goto loop;
	}

	/* is an instruction ? */
	if (parse_instr(&temp, &instruction) == PARSE_ERR) {
		line -> type = TYPE_ERR;
		err_print("Invalid");

		goto end;
	}

	/* set type and y64bin */
	line -> type = TYPE_INS;
	y64bin -> addr = vmaddr;
	y64bin -> codes[0] = instruction -> code;
	y64bin -> bytes = instruction -> bytes;

	/* update vmaddr */    
	vmaddr += instruction -> bytes;

	/* parse the rest of instruction according to the itype */
	switch (HIGH(instruction -> code)) {
		case I_HALT: case I_NOP: case I_RET:
		{
			goto loop;
		}
		case I_POPQ: case I_PUSHQ:
		{
			regid_t registerA;

			if (parse_reg(&temp, &registerA) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid REG");

				goto end;
			}

			y64bin -> codes[1] = HPACK(registerA, REG_NONE);

			goto loop;
		}
		case I_ALU: case I_RRMOVQ:
		{
			regid_t registerA, registerB;

			if (parse_reg(&temp, &registerA) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid REG");

				goto end;
			}
			
			char c = ',';
			if (parse_delim(&temp, c) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid ','");

				goto end;
			}

			if (parse_reg(&temp, &registerB) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid REG");

				goto end;
			}

			y64bin -> codes[1] = HPACK(registerA, registerB);

			goto loop;
		}
		case I_RMMOVQ:
		{
			regid_t registerA, registerB;

			if (parse_reg(&temp, &registerA) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid REG");

				goto end;
			}
			
			char c = ',';
			if (parse_delim(&temp, c) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid ','");

				goto end;
			}
			
			long l = 0;
			if (parse_mem(&temp, &l, &registerB) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid MEM");

				goto end;
			}

			y64bin -> codes[1] = (registerA << 4) + registerB;
			y64bin -> codes[2] = (l & 0xff);
			y64bin -> codes[3] = ((l >> 8) & 0xff);
			y64bin -> codes[4] = ((l >> 16) & 0xff);
			y64bin -> codes[5] = ((l >> 24) & 0xff);
			y64bin -> codes[6] = ((l >> 32) & 0xff);
			y64bin -> codes[7] = ((l >> 40) & 0xff);
			y64bin -> codes[8] = ((l >> 48) & 0xff);
			y64bin -> codes[9] = ((l >> 56) & 0xff);

			goto loop;
		}
		case I_IRMOVQ:
		{
			regid_t registerB;
			char c = ',';
			char *n;
			long a;
			parse_t val = parse_imm(&temp, &n, &a);

			if (val == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid Immediate")

				goto end;
			}
			
			if (val == PARSE_SYMBOL) {
				add_reloc(n, y64bin);
			}

			if (parse_delim(&temp, c) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid ','");

				goto end;
			}

			if (parse_reg(&temp, &registerB) == PARSE_ERR) {
				line -> type =  TYPE_ERR;
				err_print("Invalid REG");

				goto end;
			}

			y64bin -> codes[1] = HPACK(REG_NONE, registerB);
			y64bin -> codes[2] = (a & 0xff);
			y64bin -> codes[3] = ((a >> 8) & 0xff);
			y64bin -> codes[4] = ((a >> 16) & 0xff);
			y64bin -> codes[5] = ((a >> 24) & 0xff);
			y64bin -> codes[6] = ((a >> 32) & 0xff);
			y64bin -> codes[7] = ((a >> 40) & 0xff);
			y64bin -> codes[8] = ((a >> 48) & 0xff);
			y64bin -> codes[9] = ((a >> 56) & 0xff);

			goto loop;
		}
		case I_MRMOVQ:
		{
			regid_t registerA, registerB;
			long l = 0;
			char c = ',';
			
			if (parse_mem(&temp, &l, &registerB) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid MEM");

				goto end;
			}

			if (parse_delim(&temp, c) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid ','");

				goto end;
			}

			if (parse_reg(&temp, &registerA) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid REG");

				goto end;
			}
			y64bin -> codes[1] = ((registerA << 4) + registerB);
			y64bin -> codes[2] = ((l) & 0xff);
			y64bin -> codes[3] = ((l >> 8) & 0xff);
			y64bin -> codes[4] = ((l >> 16) & 0xff);
			y64bin -> codes[5] = ((l >> 24) & 0xff);
			y64bin -> codes[6] = ((l >> 32) & 0xff);
			y64bin -> codes[7] = ((l >> 40) & 0xff);
			y64bin -> codes[8] = ((l >> 48) & 0xff);
			y64bin -> codes[9] = ((l >> 56) & 0xff);

			goto loop;
		}
		case I_JMP: case I_CALL:
		{
			char *c;

			if (parse_symbol(&temp, &c) == PARSE_ERR) {
				line -> type = TYPE_ERR;
				err_print("Invalid DEST");

				goto end;
			}
			
			add_reloc(c, y64bin);

			goto loop;
		}
		case I_DIRECTIVE:
		{
			switch (LOW(instruction -> code)) {
				case D_DATA:
				{
					long l = 0;
					char *n;
					parse_t p = parse_data(&temp, &n, &l);
					
					if (p == PARSE_ERR) {
						line -> type = TYPE_ERR;
						err_print("Invalid");

						goto end;
					}

					if (p == PARSE_SYMBOL) {
						add_reloc(n, y64bin);

						goto loop;
					}
					
					long *digits = (long *)y64bin -> codes;
					*digits = l;

					goto loop;
				}
				case D_POS:
				{
					long l;

            				if (parse_digit(&temp, &l) == PARSE_ERR) {
                				line -> type = TYPE_ERR;
                				err_print("Invalid DEST");

                				goto end;
            				}
            
					if (l < 0) {
                				line -> type = TYPE_ERR;
                				err_print("Invalid DEST");

                				goto end;
            				}

           				vmaddr = (int)l;
            				y64bin -> addr = vmaddr;

            				goto loop;
				}
				case D_ALIGN:
				{
					long l;

  	    				if (parse_digit(&temp, &l) == PARSE_ERR) {
  	          				line -> type = TYPE_ERR;
  	          				err_print("Invalid DEST");

  	          				goto end;;
              				}
					
  	    				while (vmaddr % l) {
						vmaddr++;
					}
  	    				
					y64bin -> addr = vmaddr;
					
					goto loop;
				}
				default:
				{
					line -> type = TYPE_ERR;
					err_print("Invalid DEST");

					goto end;
				}

				break;
			}
		}
		default:
		{
			line -> type  = TYPE_ERR;
			err_print("Invalid");

			goto end;
		}
	}

	end: ;
	free(y64asm);

	return line -> type;
}

/*
 * assemble: assemble an y64 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y64 assembly file)
 *
 * return
 *     0: success, assmble the y64 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y64asm;

    /* read y64 code line-by-line, and parse them to generate raw y64 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL) {
        slen  = strlen(asm_buf);
        while ((asm_buf[slen-1] == '\n') || (asm_buf[slen-1] == '\r')) { 
            asm_buf[--slen] = '\0'; /* replace terminator */
        }

        /* store y64 assembly code */
        y64asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y64asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        line->type = TYPE_COMM;
        line->y64asm = y64asm;
        line->next = NULL;

        line_tail->next = line;
        line_tail = line;
        lineno ++;
	
        if (parse_line(line) == TYPE_ERR) {
            return -1;
        }
    }

	lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y64 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
	// initialisaton	
	reloc_t *rtmp = reltab -> next;

    	while (rtmp) {
        	/* find symbol */
		symbol_t *s = find_symbol(rtmp -> name);

		if (s == NULL) {
			err_print("Unknown symbol:'%s'", rtmp -> name);

			return -1;
		}

		long *a;
		byte_t type = HIGH(rtmp -> y64bin -> codes[0]);

		/* relocate y64bin according itype */
		if (type == I_IRMOVQ) {
			a = (long *)&rtmp -> y64bin -> codes[2];
		} else if (type == I_CALL || type == I_JMP) {
			a = (long *)&rtmp -> y64bin -> codes[1];
		} else { /* next */
			a = (long *)&rtmp -> y64bin -> codes[0];
		}

		*a = s -> addr;
		rtmp = rtmp -> next;
	}

	return 0;
}

/*
 * binfile: generate the y64 binary file
 * args
 *     out: point to output file (an y64 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
	// declarations	
	line_t *line = line_head;
	int currentPosition = 0;

	/* prepare image with y64 binary code */
	while (line != NULL) {
		// initialisation
		line_t *line2 = line;
		line2 = line2 -> next;
		bool_t endCheck = 0; // error if using mere bool type, so use bool_t instead
		
		// do loop to iterate to all possible binaries
		while (line2 != NULL) {
			if (line2 -> y64bin.bytes != 0) {
				endCheck = 1;
				break;
			} else {
				line2 = line2 -> next;
			}
		}
		
		// do required checking
		if (currentPosition < line -> y64bin.addr && endCheck) {
			int positionShift = line -> y64bin.addr - currentPosition;
			
			// fprintf to send formatted output into a stream for latter use
			for (int i = 0; i < positionShift; i++) {
				fprintf(out, "%c", 0);	
			}

			currentPosition = line -> y64bin.addr; // go to next address position
		}

		/* binary write y64 code to output file (NOTE: see fwrite()) */
		fwrite(line -> y64bin.codes, sizeof(byte_t), line -> y64bin.bytes, out); // write binary codes to output file
		currentPosition += line -> y64bin.bytes;
		line = line -> next;
	}

	return 0;
}


/* whether print the readable output to screen or not ? */
bool_t screen = FALSE; 

static void hexstuff(char *dest, int value, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        char c;
        int h = (value >> 4*i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len-i-1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[34];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS) {
        bin_t *y64bin = &line->y64bin;
        int i;
        
        strcpy(buf, "  0x000:                      | ");
        
        hexstuff(buf+4, y64bin->addr, 3);
        if (y64bin->bytes > 0)
            for (i = 0; i < y64bin->bytes; i++)
                hexstuff(buf+9+2*i, y64bin->codes[i]&0xFF, 2);
    } else {
        strcpy(buf, "                              | ");
    }

    printf("%s%s\n", buf, line->y64asm);
}

/* 
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = line_head->next;
    while (tmp != NULL) {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    line_head = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(line_head, 0, sizeof(line_t));
    line_tail = line_head;
    lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do {
        rtmp = reltab->next;
        if (reltab->name) 
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);
    
    symbol_t *stmp = NULL;
    do {
        stmp = symtab->next;
        if (symtab->name) 
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do {
        ltmp = line_head->next;
        if (line_head->y64asm) 
            free(line_head->y64asm);
        free(line_head);
        line_head = ltmp;
    } while (line_head);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;
    
    if (argc < 2)
        usage(argv[0]);
    
    if (argv[nextarg][0] == '-') {
        char flag = argv[nextarg][1];
        switch (flag) {
          case 'v':
            screen = TRUE;
            nextarg++;
            break;
          default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg])-3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg]+rootlen, ".ys"))
        usage(argv[0]);
    
    if (rootlen > 500) {
        err_print("File name too long");
        exit(1);
    }
 

    /* init */
    init();

    
    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname+rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in) {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }
    
    if (assemble(in) < 0) {
        err_print("Assemble y64 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);

    /* relocate binary code */
    if (relocate() < 0) {
        err_print("Relocate binary code error");
        exit(1);
    }

    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname+rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out) {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0) {
        err_print("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);
 
    /* print to screen (.yo file) */
    if (screen)
       print_screen(); 

    /* finit */
    finit();
 
    return 0;
}


