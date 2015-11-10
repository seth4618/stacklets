	.section	__TEXT,__text,regular,pure_instructions
	.macosx_version_min 10, 11
	.globl	_foo
	.align	4, 0x90
_foo:                                   ## @foo
	.cfi_startproc
## BB#0:
	pushq	%rbp
Ltmp0:
	.cfi_def_cfa_offset 16
Ltmp1:
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
Ltmp2:
	.cfi_def_cfa_register %rbp
	movq	_global@GOTPCREL(%rip), %rax
	movl	$1, (%rax)
	leaq	L_str(%rip), %rdi
	callq	_puts
	movq	_buf@GOTPCREL(%rip), %rdi
	movl	$1, %esi
	callq	_longjmp
	.cfi_endproc

	.globl	_main
	.align	4, 0x90
_main:                                  ## @main
	.cfi_startproc
## BB#0:
	pushq	%rbp
Ltmp3:
	.cfi_def_cfa_offset 16
Ltmp4:
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
Ltmp5:
	.cfi_def_cfa_register %rbp
	movl	$3, -4(%rbp)
	movl	-4(%rbp), %eax
	popq	%rbp
	retq
	.cfi_endproc

	.comm	_global,4,2             ## @global
	.comm	_buf,148,4              ## @buf
	.section	__TEXT,__cstring,cstring_literals
L_str:                                  ## @str
	.asciz	"foo"


.subsections_via_symbols
