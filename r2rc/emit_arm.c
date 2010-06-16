/* TODO */
/* pancake // nopcode.org 2010 -- emit module for rcc */

#include "rcc.h"

#define EMIT_NAME emit_arm
#define R_ARCH "arm"
#define R_SZ 8
#define R_SP "r14"
#define R_BP "rbp" // XXX
#define R_AX "r0"
#define R_GP { "r0", "r1", "r2", "r3" }
#define R_NGP 4

static char *regs[] = R_GP;

#if 0
static void emit_sc (int num) {
#if SYNTAX_ATT
	rcc_printf ("  sc $0x%x\n", num);
#else
	rcc_printf ("  sc 0x%x\n", num);
#endif
}
#endif

static void emit_frame (int sz) {
	if (sz>0) rcc_printf (
#if SYNTAX_ATT
	"  push %%"R_BP"\n"
	"  mov %%"R_SP", %%"R_BP"\n"
	"  sub $%d, %%"R_SP"\n", sz);
#else
	"  push "R_BP"\n"
	"  mov "R_BP", "R_SP"\n"
	"  sub "R_SP", %d\n", sz);
#endif
}

static void emit_frame_end (int sz, int ctx) {
	if (sz>0) {
#if SYNTAX_ATT
		rcc_printf ("  add $%d, %%"R_SP"\n", sz);
		rcc_printf ("  pop %%"R_BP"\n");
#else
		rcc_printf ("  add "R_SP", %d\n", sz);
		rcc_printf ("  pop "R_BP"\n");
#endif
	}
	if (ctx>0)
		rcc_puts ("  ret\n");
}

static void emit_comment(const char *fmt, ...) {
	va_list ap;
	char buf[1024];
	va_start (ap, fmt);
	vsnprintf (buf, sizeof (buf), fmt, ap);
#if SYNTAX_ATT
	rcc_printf ("  /* %s */\n", buf);
#else
	rcc_printf ("# %s\n", buf);
#endif
	va_end (ap);
}

static void emit_equ (const char *key, const char *value) {
	rcc_printf (".equ %s,%s\n", key, value);
}

static void emit_syscall_args(int nargs) {
	int j, k;
	for (j=0; j<nargs; j++) {
		k = j*R_SZ;
#if SYNTAX_ATT
		rcc_printf ("  mov %d(%%"R_SP"), %%%s\n", k, regs[j+1]);
#else
		rcc_printf ("  mov %s, dword ["R_SP"%c%d]\n", regs[j+1], k>0?'+':' ', k);
#endif
	}
}

static void emit_set_string(const char *dstvar, const char *str, int j) {
	char *p, str2[64];
	int i, oj = j;
	for (i=0; i<oj; i+=4) {
		/* XXX endian and 32/64bit issues */
		int *n = (int *)(str+i);
		p = mk_var (str2, dstvar, j);
#if SYNTAX_ATT
		rcc_printf ("  movl $0x%x, %s\n", *n, p);
#else
		rcc_printf ("  mov %s, 0x%x\n", p, *n);
#endif
		j -= 4;
	}
	p = mk_var (str2, dstvar, oj);
#if SYNTAX_ATT
	rcc_printf ("  lea %s, %%"R_AX"\n", p);
#else
	rcc_printf ("  lea "R_AX", %s\n", p);
#endif
	p = mk_var(str2, dstvar, 0);
#if SYNTAX_ATT
	rcc_printf ("  mov %%"R_AX", %s\n", p);
#else
	rcc_printf ("  mov %s, "R_AX"\n", p);
#endif
}

static void emit_call(const char *str, int atr) {
#if SYNTAX_ATT
	if (atr) rcc_printf("  call *%s\n", str);
#else
	if (atr) rcc_printf("  call [%s]\n", str);
#endif
	else rcc_printf("  call %s\n", str);
}

static void emit_arg (int xs, int num, const char *str) {
	int d = atoi (str);
#if !SYNTAX_ATT
	if (*str=='$')
		str = str +1;
#endif
	switch(xs) {
	case 0:
		rcc_printf ("  push %s\n", str);
		break;
	case '*':
#if SYNTAX_ATT
		rcc_printf ("  push (%s)\n", str);
#else
		rcc_printf ("  push [%s]\n", str);
#endif
		break;
	case '&':
#if SYNTAX_ATT
		if (d != 0) rcc_printf ("  addl $%d, %%"R_BP"\n", d);
		rcc_printf ("  pushl %%"R_BP"\n");
		if (d != 0) rcc_printf ("  subl $%d, %%"R_BP"\n", d);
#else
		if (d != 0) rcc_printf ("  add "R_BP", %d\n", d);
		rcc_printf ("  push "R_BP"\n");
		if (d != 0) rcc_printf ("  sub "R_BP", %d\n", d);
#endif
		break;
	}
}

static void emit_get_result(const char *ocn) {
#if SYNTAX_ATT
	rcc_printf ("  mov %%"R_AX", %s\n", ocn);
#else
	rcc_printf ("  mov %s, "R_AX"\n", ocn);
#endif
}

static void emit_restore_stack (int size) {
#if SYNTAX_ATT
	rcc_printf("  add $%d, %%"R_SP" /* args */\n", size);
#else
	rcc_printf("  add "R_SP", %d\n", size);
#endif
}

static void emit_get_while_end (char *str, const char *ctxpush, const char *label) {
#if SYNTAX_ATT
	sprintf (str, "  push %s\n  jmp %s /* ---- */\n", ctxpush, label);
#else
	sprintf (str, "  push %s\n  jmp %s\n", ctxpush, label);
#endif
}

static void emit_while_end (const char *labelback) {
#if SYNTAX_ATT
	rcc_printf ("  pop %%"R_AX"\n");
	rcc_printf ("  cmp $0, %%"R_AX"\n"); // XXX MUST SUPPORT != 0 COMPARE HERE
	rcc_printf ("  jnz %s\n", labelback);
#else
	rcc_printf ("  pop "R_AX"\n");
	rcc_printf ("  test "R_AX", "R_AX"\n"); // XXX MUST SUPPORT != 0 COMPARE HERE
	rcc_printf ("  jnz %s\n", labelback);
#endif
}

static void emit_get_var (int type, char *out, int idx) {
	switch (type) {
#if SYNTAX_ATT
	case 0: sprintf (out, "%d(%%"R_BP")", -idx); break; /* variable */
	case 1: sprintf(out, "%d(%%"R_SP")", idx); break; /* argument */
#else
	case 0: sprintf (out, "dword ["R_BP"%c%d]", idx>0?' ':'+', -idx); break; /* variable */
	case 1: sprintf(out, "dword ["R_SP"%c%d]", idx>0?'+':' ', idx); break; /* argument */
#endif
	}
}

static void emit_trap () {
	rcc_printf ("  int3\n");
}

static void emit_load_ptr(const char *dst) {
	int d = atoi (dst);
	eprintf ("HACK HACK HACK\n");
#if SYNTAX_ATT
	rcc_printf ("  leal %d(%%"R_BP"), %%"R_AX"\n", d);
#else
	// XXX: 32/64bit care
	rcc_printf ("  leal "R_AX", dword ["R_BP"+%d]\n", d);
#endif
	//rcc_printf ("  movl %%"R_BP", %%"R_AX"\n");
	//rcc_printf ("  addl $%d, %%"R_AX"\n", d);
}

static void emit_branch(char *b, char *g, char *e, char *n, int sz, const char *dst) {
	char *p, str[64];
	char *arg = NULL;
	char *op = "jz";
	/* NOTE that jb/ja are inverted to fit cmp opcode */
	if (b) {
		*b = '\0';
		if (e) op = "jae";
		else op = "ja";
		arg = b+1;
	} else
	if (g) {
		*g = '\0';
		if (e) op = "jbe";
		else op = "jb";
		arg = g+1;
	}
	if (arg == NULL) {
		if (e) {
			arg = e+1;
			op = "jne";
		} else {
			arg = "$0";
			if (n) op = "jnz";
			else op ="jz";
		}
	}

	if (*arg=='=') arg++; /* for <=, >=, ... */
	p = mk_var (str, arg, 0);
#if SYNTAX_ATT
	rcc_printf ("  pop %%"R_AX"\n"); /* TODO: add support for more than one arg get arg0 */
	rcc_printf ("  cmp%c %s, %%"R_AX"\n", sz, p);
#else
	rcc_printf ("  pop "R_AX"\n"); /* TODO: add support for more than one arg get arg0 */
	rcc_printf ("  cmp %s, "R_AX"\n", p);
#endif
	// if (context>0)
	rcc_printf ("  %s %s\n", op, dst);
}

static void emit_load(const char *dst, int sz) {
#if SYNTAX_ATT
	switch (sz) {
	case 'l':
		rcc_printf ("  movl %s, %%"R_AX"\n", dst);
		rcc_printf ("  movl (%%"R_AX"), %%"R_AX"\n");
	case 'b':
		rcc_printf ("  movl %s, %%"R_AX"\n", dst);
		rcc_printf ("  movzb (%%"R_AX"), %%"R_AX"\n");
		break;
	default:
		// TODO: unhandled?!?
		rcc_printf ("  mov%c %s, %%"R_AX"\n", sz, dst);
		rcc_printf ("  mov%c (%%"R_AX"), %%"R_AX"\n", sz);
	}
#else
	switch (sz) {
	case 'l':
		rcc_printf ("  mov "R_AX", %s\n", dst);
		rcc_printf ("  mov "R_AX", ["R_AX"]\n");
	case 'b':
		rcc_printf ("  mov "R_AX", %s\n", dst);
		rcc_printf ("  movz "R_AX", ["R_AX"]\n");
		break;
	default:
		// TODO: unhandled?!?
		rcc_printf ("  mov "R_AX", %s\n", dst);
		rcc_printf ("  mov "R_AX", ["R_AX"]\n");
	}
#endif
}

static void emit_mathop(int ch, int vs, int type, const char *eq, const char *p) {
	char *op;
	switch (ch) {
	case '^': op = "xor"; break;
	case '&': op = "and"; break;
	case '|': op = "or";  break;
	case '-': op = "sub"; break;
	case '+': op = "add"; break;
	case '*': op = "mul"; break;
	case '/': op = "div"; break;
	default:  op = "mov"; break;
	}
#if SYNTAX_ATT
	if (eq == NULL) eq = "%"R_AX;
	if (p == NULL) p = "%"R_AX;
	rcc_printf ("  %s%c %c%s, %s\n", op, vs, type, eq, p);
#else
	if (eq == NULL) eq = R_AX;
	if (p == NULL) p = R_AX;
// TODO: 
	eprintf ("TYPE = %c\n", type);
	eprintf ("  %s%c %c%s, %s\n", op, vs, type, eq, p);
	eprintf ("  %s %s, [%s]\n", op, p, eq);
	if (type == '*') {
		rcc_printf ("  %s %s, [%s]\n", op, p, eq);
	} else rcc_printf ("  %s %s, %s\n", op, p, eq);
#endif
}

static const char* emit_regs(int idx) {
	return regs[idx%R_NGP];
}

struct emit_t EMIT_NAME = {
	.arch = R_ARCH,
	.size = R_SZ,
	.call = emit_call,
	.equ = emit_equ,
	.regs = emit_regs,
	//.sc = emit_sc,
	.trap = emit_trap,
	.frame = emit_frame,
	.frame_end = emit_frame_end,
	.comment = emit_comment,
	.push_arg = emit_arg,
	.restore_stack = emit_restore_stack,
	.get_result = emit_get_result,
	.syscall_args = emit_syscall_args,
	.set_string = emit_set_string,
	.get_var = emit_get_var,
	.while_end = emit_while_end,
	.get_while_end = emit_get_while_end,
	.branch = emit_branch,
	.load = emit_load,
	.load_ptr = emit_load_ptr,
	.mathop = emit_mathop,
#if SYNTAX_ATT
	.syscall_body = ": mov $`.arg`, %"R_AX"\n: int $0x80\n",
#else
	.syscall_body = ": mov "R_AX", `.arg`\n: int 0x80\n",
#endif
};
