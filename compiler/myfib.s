	.comm	_seedDummyHead,8,3
	.text
	.globl _seedStackInit
_seedStackInit:
LFB4:
	pushq	%rbp
LCFI0:
	movq	%rsp, %rbp
LCFI1:
	movl	$56, %esi
	movl	$1, %edi
	call	_calloc
	movq	%rax, %rdx
	movq	_seedDummyHead@GOTPCREL(%rip), %rax
	movq	%rdx, (%rax)
	popq	%rbp
LCFI2:
	ret
LFE4:
	.comm	_readyDummyHead,8,3
	.text
	.globl _readQInit
_readQInit:
LFB5:
	pushq	%rbp
LCFI3:
	movq	%rsp, %rbp
LCFI4:
	movl	$16, %esi
	movl	$1, %edi
	call	_calloc
	movq	%rax, %rdx
	movq	_readyDummyHead@GOTPCREL(%rip), %rax
	movq	%rdx, (%rax)
	popq	%rbp
LCFI5:
	ret
LFE5:
	.comm	_seedStack,2048,5
	.globl _seedStackPtr
	.zerofill __DATA,__pu_bss2,_seedStackPtr,4,2
	.text
	.globl _popseed
_popseed:
LFB6:
	pushq	%rbp
LCFI6:
	movq	%rsp, %rbp
LCFI7:
	movl	_seedStackPtr(%rip), %eax
	testl	%eax, %eax
	jne	L4
	movl	$0, %eax
	jmp	L5
L4:
	movl	_seedStackPtr(%rip), %eax
	leal	-1(%rax), %edx
	movl	%edx, _seedStackPtr(%rip)
	cltq
	salq	$4, %rax
	movq	%rax, %rdx
	movq	_seedStack@GOTPCREL(%rip), %rax
	addq	%rdx, %rax
L5:
	popq	%rbp
LCFI8:
	ret
LFE6:
	.comm	_readyQ,2048,5
	.globl _readyQFront
	.zerofill __DATA,__pu_bss2,_readyQFront,4,2
	.globl _readyQBack
	.zerofill __DATA,__pu_bss2,_readyQBack,4,2
	.text
	.globl _dequeue
_dequeue:
LFB7:
	pushq	%rbp
LCFI9:
	movq	%rsp, %rbp
LCFI10:
	movl	_readyQFront(%rip), %eax
	addl	$1, %eax
	movl	%eax, _readyQFront(%rip)
	movl	_readyQFront(%rip), %eax
	andl	$3, %eax
	movl	%eax, _readyQFront(%rip)
	popq	%rbp
LCFI11:
	ret
LFE7:
	.cstring
LC0:
	.ascii "Error: %s\12\0"
	.text
	.globl _die
_die:
LFB8:
	pushq	%rbp
LCFI12:
	movq	%rsp, %rbp
LCFI13:
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	movq	___stderrp@GOTPCREL(%rip), %rax
	movq	(%rax), %rax
	movq	-8(%rbp), %rdx
	leaq	LC0(%rip), %rsi
	movq	%rax, %rdi
	movl	$0, %eax
	call	_fprintf
	movl	$-1, %edi
	call	_exit
LFE8:
	.globl _stubRoutine
_stubRoutine:
LFB9:
	pushq	%rbp
LCFI14:
	movq	%rsp, %rbp
LCFI15:
	popq	%rbp
LCFI16:
	ret
LFE9:
	.zerofill __DATA,__bss2,_suspCtr,4,2
	.data
	.align 2
_suspHere:
	.long	10
	.text
	.globl _suspend
_suspend:
LFB10:
	pushq	%rbp
LCFI17:
	movq	%rsp, %rbp
LCFI18:
	subq	$160, %rsp
	movl	%edi, -148(%rbp)
	cmpl	$1, -148(%rbp)
	jne	L10
# 163 "myfib.c" 1
	movq %rcx,-144(%rbp)   
movq %rdx,-136(%rbp)   
movq %rbx,-128(%rbp)   
movq %rsp,-120(%rbp)   
movq %rsi,-112(%rbp)   
movq %rdi,-104(%rbp)   
movq %r8,-96(%rbp)    
movq %r9,-88(%rbp)    
movq %r10,-80(%rbp)  
movq %r11,-72(%rbp)  
movq %r12,-64(%rbp)  
movq %r13,-56(%rbp)  
movq %r14,-48(%rbp)  
movq %r15,-40(%rbp)  
movq %rax,-32(%rbp)   

# 0 "" 2
	movl	$0, %eax
	call	_enqReadyQ
L10:
	movq	_seedDummyHead@GOTPCREL(%rip), %rax
	movq	(%rax), %rax
	movq	48(%rax), %rax
	movq	%rax, -8(%rbp)
	jmp	L11
L13:
	movq	-8(%rbp), %rax
	movl	32(%rax), %eax
	testl	%eax, %eax
	jne	L12
	movq	-8(%rbp), %rax
	movl	$1, 32(%rax)
	movq	-8(%rbp), %rax
	movl	$2, 36(%rax)
	movq	-8(%rbp), %rax
	movq	16(%rax), %rax
	movq	-8(%rbp), %rdx
	movq	24(%rdx), %rdx
	movq	%rdx, %rdi
	call	*%rax
	movq	-8(%rbp), %rax
	movl	36(%rax), %eax
	leal	-1(%rax), %ecx
	movq	-8(%rbp), %rdx
	movl	%ecx, 36(%rdx)
	testl	%eax, %eax
	jne	L12
	movq	-8(%rbp), %rax
	movq	(%rax), %rax
# 178 "myfib.c" 1
	movq %rax,%rsp
# 0 "" 2
	movq	-8(%rbp), %rax
	movq	8(%rax), %rax
	nop
	jmp	*%rax
L12:
	movq	-8(%rbp), %rax
	movq	48(%rax), %rax
	movq	%rax, -8(%rbp)
L11:
	cmpq	$0, -8(%rbp)
	jne	L13
	movq	_readyDummyHead@GOTPCREL(%rip), %rax
	movq	(%rax), %rax
	movq	(%rax), %rax
	movq	%rax, -16(%rbp)
	cmpq	$0, -16(%rbp)
	je	L9
	movl	$0, %eax
	call	_deqReadyQ
	movq	-16(%rbp), %rax
	movq	(%rax), %rax
# 192 "myfib.c" 1
	movq %rax,%rsp
# 0 "" 2
# 193 "myfib.c" 1
	movq -144(%rbp),%rcx   
movq -136(%rbp),%rdx   
movq -128(%rbp),%rbx   
movq -120(%rbp),%rsp   
movq -112(%rbp),%rsi   
movq -104(%rbp),%rdi   
movq -96(%rbp),%r8    
movq -88(%rbp),%r9    
movq -80(%rbp),%r10  
movq -72(%rbp),%r11  
movq -64(%rbp),%r12  
movq -56(%rbp),%r13  
movq -48(%rbp),%r14  
movq -40(%rbp),%r15  
movq -32(%rbp),%rax   

# 0 "" 2
L9:
	leave
LCFI19:
	ret
LFE10:
	.globl _testHack
_testHack:
LFB11:
	pushq	%rbp
LCFI20:
	movq	%rsp, %rbp
LCFI21:
	movl	_suspCtr(%rip), %eax
	leal	1(%rax), %edx
	movl	%edx, _suspCtr(%rip)
	cmpl	$9, %eax
	jle	L15
	movl	$0, _suspCtr(%rip)
	movl	$1, %edi
	call	_suspend
L15:
	popq	%rbp
LCFI22:
	ret
LFE11:
	.globl _fib
_fib:
LFB12:
	pushq	%rbp
LCFI23:
	movq	%rsp, %rbp
LCFI24:
	subq	$176, %rsp
	movq	%rdi, -168(%rbp)
	movq	-168(%rbp), %rax
	movl	(%rax), %eax
	cmpl	$2, %eax
	jg	L18
	movq	-168(%rbp), %rax
	movl	$1, 4(%rax)
	jmp	L17
L18:
	call	_testHack
	movl	$8, %esi
	movl	$1, %edi
	call	_calloc
	movq	%rax, -8(%rbp)
	movl	$8, %esi
	movl	$1, %edi
	call	_calloc
	movq	%rax, -16(%rbp)
	movq	-168(%rbp), %rax
	movl	(%rax), %eax
	leal	-1(%rax), %edx
	movq	-8(%rbp), %rax
	movl	%edx, (%rax)
	movq	-168(%rbp), %rax
	movl	(%rax), %eax
	leal	-2(%rax), %edx
	movq	-16(%rbp), %rax
	movl	%edx, (%rax)
# 226 "myfib.c" 1
	movq %rsp,%rax
# 0 "" 2
	movq	%rax, -24(%rbp)
	movq	-16(%rbp), %rdx
	movq	-24(%rbp), %rax
	movq	%rdx, %rcx
	leaq	_fib(%rip), %rdx
	movq	%rax, %rsi
	leaq	L20(%rip), %rdi
	movl	$0, %eax
	call	_initSeed
	cltq
	movq	%rax, -32(%rbp)
	movq	-32(%rbp), %rax
	movq	%rax, %rdi
	movl	$0, %eax
	call	_pushSeed
# 230 "myfib.c" 1
	movq %rcx,-160(%rbp)   
movq %rdx,-152(%rbp)   
movq %rbx,-144(%rbp)   
movq %rsp,-136(%rbp)   
movq %rsi,-128(%rbp)   
movq %rdi,-120(%rbp)   
movq %r8,-112(%rbp)    
movq %r9,-104(%rbp)    
movq %r10,-96(%rbp)  
movq %r11,-88(%rbp)  
movq %r12,-80(%rbp)  
movq %r13,-72(%rbp)  
movq %r14,-64(%rbp)  
movq %r15,-56(%rbp)  
movq %rax,-48(%rbp)   

# 0 "" 2
	movq	-8(%rbp), %rax
	movq	%rax, %rdi
	call	_fib
	movq	-32(%rbp), %rax
	movl	32(%rax), %eax
	testl	%eax, %eax
	je	L20
	movq	-32(%rbp), %rax
	movl	36(%rax), %eax
	leal	-1(%rax), %ecx
	movq	-32(%rbp), %rdx
	movl	%ecx, 36(%rdx)
	testl	%eax, %eax
	je	L20
	movl	$0, %edi
	call	_suspend
L20:
	movq	-32(%rbp), %rax
	movq	%rax, %rdi
	movl	$0, %eax
	call	_popSeed
	movq	-16(%rbp), %rax
	movq	%rax, %rdi
	call	_fib
	movq	-8(%rbp), %rax
	movl	4(%rax), %edx
	movq	-16(%rbp), %rax
	movl	4(%rax), %eax
	addl	%eax, %edx
	movq	-168(%rbp), %rax
	movl	%edx, 4(%rax)
	nop
L17:
	leave
LCFI25:
	ret
LFE12:
	.globl _startfib
_startfib:
LFB13:
	pushq	%rbp
LCFI26:
	movq	%rsp, %rbp
LCFI27:
	subq	$32, %rsp
	movl	%edi, -20(%rbp)
	movl	$0, %eax
	call	_seedStackInit
	movl	$8, %esi
	movl	$1, %edi
	call	_calloc
	movq	%rax, -8(%rbp)
	movq	-8(%rbp), %rax
	movl	-20(%rbp), %edx
	movl	%edx, (%rax)
	movq	-8(%rbp), %rax
	movq	%rax, %rdi
	call	_fib
	movq	-8(%rbp), %rax
	movl	4(%rax), %eax
	leave
LCFI28:
	ret
LFE13:
	.cstring
	.align 3
LC1:
	.ascii "Need at least one argument to run, n.  If 2, then second is # of threads\0"
	.align 3
LC2:
	.ascii "Will run fib(%d) on %d thread(s)\12\0"
LC3:
	.ascii "fib(%d) = %d\12\0"
	.text
	.globl _main
_main:
LFB14:
	pushq	%rbp
LCFI29:
	movq	%rsp, %rbp
LCFI30:
	subq	$32, %rsp
	movl	%edi, -20(%rbp)
	movq	%rsi, -32(%rbp)
	movl	$1, -4(%rbp)
	cmpl	$0, -20(%rbp)
	jg	L24
	leaq	LC1(%rip), %rdi
	call	_die
L24:
	movq	-32(%rbp), %rax
	movq	(%rax), %rax
	movq	%rax, %rdi
	call	_atoi
	movl	%eax, -8(%rbp)
	cmpl	$2, -20(%rbp)
	jne	L25
	movq	-32(%rbp), %rax
	addq	$8, %rax
	movq	(%rax), %rax
	movq	%rax, %rdi
	call	_atoi
	movl	%eax, -4(%rbp)
L25:
	movl	-4(%rbp), %edx
	movl	-8(%rbp), %eax
	movl	%eax, %esi
	leaq	LC2(%rip), %rdi
	movl	$0, %eax
	call	_printf
	movl	-8(%rbp), %eax
	movl	%eax, %edi
	call	_startfib
	movl	%eax, -12(%rbp)
	movl	-12(%rbp), %edx
	movl	-8(%rbp), %eax
	movl	%eax, %esi
	leaq	LC3(%rip), %rdi
	movl	$0, %eax
	call	_printf
	movl	$0, %edi
	call	_exit
LFE14:
	.section __TEXT,__eh_frame,coalesced,no_toc+strip_static_syms+live_support
EH_frame1:
	.set L$set$0,LECIE1-LSCIE1
	.long L$set$0
LSCIE1:
	.long	0
	.byte	0x1
	.ascii "zR\0"
	.byte	0x1
	.byte	0x78
	.byte	0x10
	.byte	0x1
	.byte	0x10
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.byte	0x90
	.byte	0x1
	.align 3
LECIE1:
LSFDE1:
	.set L$set$1,LEFDE1-LASFDE1
	.long L$set$1
LASFDE1:
	.long	LASFDE1-EH_frame1
	.quad	LFB4-.
	.set L$set$2,LFE4-LFB4
	.quad L$set$2
	.byte	0
	.byte	0x4
	.set L$set$3,LCFI0-LFB4
	.long L$set$3
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$4,LCFI1-LCFI0
	.long L$set$4
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.set L$set$5,LCFI2-LCFI1
	.long L$set$5
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 3
LEFDE1:
LSFDE3:
	.set L$set$6,LEFDE3-LASFDE3
	.long L$set$6
LASFDE3:
	.long	LASFDE3-EH_frame1
	.quad	LFB5-.
	.set L$set$7,LFE5-LFB5
	.quad L$set$7
	.byte	0
	.byte	0x4
	.set L$set$8,LCFI3-LFB5
	.long L$set$8
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$9,LCFI4-LCFI3
	.long L$set$9
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.set L$set$10,LCFI5-LCFI4
	.long L$set$10
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 3
LEFDE3:
LSFDE5:
	.set L$set$11,LEFDE5-LASFDE5
	.long L$set$11
LASFDE5:
	.long	LASFDE5-EH_frame1
	.quad	LFB6-.
	.set L$set$12,LFE6-LFB6
	.quad L$set$12
	.byte	0
	.byte	0x4
	.set L$set$13,LCFI6-LFB6
	.long L$set$13
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$14,LCFI7-LCFI6
	.long L$set$14
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.set L$set$15,LCFI8-LCFI7
	.long L$set$15
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 3
LEFDE5:
LSFDE7:
	.set L$set$16,LEFDE7-LASFDE7
	.long L$set$16
LASFDE7:
	.long	LASFDE7-EH_frame1
	.quad	LFB7-.
	.set L$set$17,LFE7-LFB7
	.quad L$set$17
	.byte	0
	.byte	0x4
	.set L$set$18,LCFI9-LFB7
	.long L$set$18
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$19,LCFI10-LCFI9
	.long L$set$19
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.set L$set$20,LCFI11-LCFI10
	.long L$set$20
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 3
LEFDE7:
LSFDE9:
	.set L$set$21,LEFDE9-LASFDE9
	.long L$set$21
LASFDE9:
	.long	LASFDE9-EH_frame1
	.quad	LFB8-.
	.set L$set$22,LFE8-LFB8
	.quad L$set$22
	.byte	0
	.byte	0x4
	.set L$set$23,LCFI12-LFB8
	.long L$set$23
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$24,LCFI13-LCFI12
	.long L$set$24
	.byte	0xd
	.byte	0x6
	.align 3
LEFDE9:
LSFDE11:
	.set L$set$25,LEFDE11-LASFDE11
	.long L$set$25
LASFDE11:
	.long	LASFDE11-EH_frame1
	.quad	LFB9-.
	.set L$set$26,LFE9-LFB9
	.quad L$set$26
	.byte	0
	.byte	0x4
	.set L$set$27,LCFI14-LFB9
	.long L$set$27
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$28,LCFI15-LCFI14
	.long L$set$28
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.set L$set$29,LCFI16-LCFI15
	.long L$set$29
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 3
LEFDE11:
LSFDE13:
	.set L$set$30,LEFDE13-LASFDE13
	.long L$set$30
LASFDE13:
	.long	LASFDE13-EH_frame1
	.quad	LFB10-.
	.set L$set$31,LFE10-LFB10
	.quad L$set$31
	.byte	0
	.byte	0x4
	.set L$set$32,LCFI17-LFB10
	.long L$set$32
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$33,LCFI18-LCFI17
	.long L$set$33
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.set L$set$34,LCFI19-LCFI18
	.long L$set$34
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 3
LEFDE13:
LSFDE15:
	.set L$set$35,LEFDE15-LASFDE15
	.long L$set$35
LASFDE15:
	.long	LASFDE15-EH_frame1
	.quad	LFB11-.
	.set L$set$36,LFE11-LFB11
	.quad L$set$36
	.byte	0
	.byte	0x4
	.set L$set$37,LCFI20-LFB11
	.long L$set$37
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$38,LCFI21-LCFI20
	.long L$set$38
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.set L$set$39,LCFI22-LCFI21
	.long L$set$39
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 3
LEFDE15:
LSFDE17:
	.set L$set$40,LEFDE17-LASFDE17
	.long L$set$40
LASFDE17:
	.long	LASFDE17-EH_frame1
	.quad	LFB12-.
	.set L$set$41,LFE12-LFB12
	.quad L$set$41
	.byte	0
	.byte	0x4
	.set L$set$42,LCFI23-LFB12
	.long L$set$42
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$43,LCFI24-LCFI23
	.long L$set$43
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.set L$set$44,LCFI25-LCFI24
	.long L$set$44
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 3
LEFDE17:
LSFDE19:
	.set L$set$45,LEFDE19-LASFDE19
	.long L$set$45
LASFDE19:
	.long	LASFDE19-EH_frame1
	.quad	LFB13-.
	.set L$set$46,LFE13-LFB13
	.quad L$set$46
	.byte	0
	.byte	0x4
	.set L$set$47,LCFI26-LFB13
	.long L$set$47
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$48,LCFI27-LCFI26
	.long L$set$48
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.set L$set$49,LCFI28-LCFI27
	.long L$set$49
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 3
LEFDE19:
LSFDE21:
	.set L$set$50,LEFDE21-LASFDE21
	.long L$set$50
LASFDE21:
	.long	LASFDE21-EH_frame1
	.quad	LFB14-.
	.set L$set$51,LFE14-LFB14
	.quad L$set$51
	.byte	0
	.byte	0x4
	.set L$set$52,LCFI29-LFB14
	.long L$set$52
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.set L$set$53,LCFI30-LCFI29
	.long L$set$53
	.byte	0xd
	.byte	0x6
	.align 3
LEFDE21:
	.subsections_via_symbols
