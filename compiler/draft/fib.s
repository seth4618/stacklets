	.comm	_seedStack,2048,5
	.globl _seedStackPtr
	.zerofill __DATA,__pu_bss2,_seedStackPtr,4,2
	.text
	.globl _popseed
_popseed:
LFB4:
	pushq	%rbp
LCFI0:
	movq	%rsp, %rbp
LCFI1:
	movl	_seedStackPtr(%rip), %eax
	testl	%eax, %eax
	jne	L2
	movl	$0, %eax
	jmp	L3
L2:
	movl	_seedStackPtr(%rip), %eax
	leal	-1(%rax), %edx
	movl	%edx, _seedStackPtr(%rip)
	cltq
	salq	$4, %rax
	movq	%rax, %rdx
	movq	_seedStack@GOTPCREL(%rip), %rax
	addq	%rdx, %rax
L3:
	popq	%rbp
LCFI2:
	ret
LFE4:
	.comm	_readyQ,2048,5
	.globl _readyQFront
	.zerofill __DATA,__pu_bss2,_readyQFront,4,2
	.globl _readyQBack
	.zerofill __DATA,__pu_bss2,_readyQBack,4,2
	.text
	.globl _dequeue
_dequeue:
LFB5:
	pushq	%rbp
LCFI3:
	movq	%rsp, %rbp
LCFI4:
	movl	_readyQFront(%rip), %eax
	addl	$1, %eax
	movl	%eax, _readyQFront(%rip)
	movl	_readyQFront(%rip), %eax
	andl	$3, %eax
	movl	%eax, _readyQFront(%rip)
	popq	%rbp
LCFI5:
	ret
LFE5:
	.cstring
LC0:
	.ascii "Error: %s\12\0"
	.text
	.globl _die
_die:
LFB6:
	pushq	%rbp
LCFI6:
	movq	%rsp, %rbp
LCFI7:
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
LFE6:
	.zerofill __DATA,__bss2,_suspCtr,4,2
	.data
	.align 2
_suspHere:
	.long	10
	.text
	.globl _testHack
_testHack:
LFB7:
	pushq	%rbp
LCFI8:
	movq	%rsp, %rbp
LCFI9:
	addq	$-128, %rsp
	movl	_suspCtr(%rip), %eax
	leal	1(%rax), %edx
	movl	%edx, _suspCtr(%rip)
	cmpl	$9, %eax
	jle	L8
	movl	$0, _suspCtr(%rip)
# 58 "fib.c" 1
	movq rcx,-128(%rbp)  
movq rdx,-120(%rbp)  
movq rbx,-112(%rbp)  
movq rsp,-104(%rbp)  
movq rsi,-96(%rbp)  
movq rdi,-88(%rbp)  
movq r8,-80(%rbp)   
movq r9,-72(%rbp)   
movq r10,-64(%rbp) 
movq r11,-56(%rbp) 
movq r12,-48(%rbp) 
movq r13,-40(%rbp) 
movq r14,-32(%rbp) 
movq r15,-24(%rbp) 

# 0 "" 2
	movl	_readyQBack(%rip), %edx
	movl	_readyQBack(%rip), %ecx
	movq	_readyQ@GOTPCREL(%rip), %rax
	movslq	%edx, %rdx
	salq	$4, %rdx
	addq	%rax, %rdx
	movq	_readyQ@GOTPCREL(%rip), %rax
	movslq	%ecx, %rcx
	salq	$4, %rcx
	addq	%rax, %rcx
	leaq	L8(%rip), %rax
# 59 "fib.c" 1
	nop; nop; nop; movq %rax,(%rdx); movq %rsp,8(%rcx); addq $16,_readyQBack(%rip); andq $2047, _readyQBack(%rip)
# 0 "" 2
	movl	$0, %eax
	call	_suspend
	jmp	L6
L8:
# 64 "fib.c" 1
	movq -128(%rbp),rcx   
movq -120(%rbp),rdx   
movq -112(%rbp),rbx   
movq -104(%rbp),rsp   
movq -96(%rbp),rsi   
movq -88(%rbp),rdi   
movq -80(%rbp),r8    
movq -72(%rbp),r9    
movq -64(%rbp),r10  
movq -56(%rbp),r11  
movq -48(%rbp),r12  
movq -40(%rbp),r13  
movq -32(%rbp),r14  
movq -24(%rbp),r15  

# 0 "" 2
	nop
L6:
	leave
LCFI10:
	ret
LFE7:
	.globl _suspend
_suspend:
LFB8:
	pushq	%rbp
LCFI11:
	movq	%rsp, %rbp
LCFI12:
	subq	$16, %rsp
	movl	$0, %eax
	call	_popseed
	movq	%rax, -8(%rbp)
	cmpq	$0, -8(%rbp)
	je	L12
	movq	-8(%rbp), %rax
	movq	8(%rax), %rax
# 80 "fib.c" 1
	movq %rax,%rbp
# 0 "" 2
	movq	-8(%rbp), %rax
	movq	(%rax), %rax
	nop
L14:
	jmp	*%rax
L12:
	movl	_readyQFront(%rip), %edx
	movl	_readyQBack(%rip), %eax
	cmpl	%eax, %edx
	je	L11
	movl	_readyQFront(%rip), %eax
	movl	%eax, -12(%rbp)
	movl	$0, %eax
	call	_dequeue
	movq	_readyQ@GOTPCREL(%rip), %rax
	movl	-12(%rbp), %edx
	movslq	%edx, %rdx
	salq	$4, %rdx
	addq	%rdx, %rax
	movq	8(%rax), %rax
# 89 "fib.c" 1
	movq %rax,%rbp
# 0 "" 2
	movq	_readyQ@GOTPCREL(%rip), %rax
	movl	-12(%rbp), %edx
	movslq	%edx, %rdx
	salq	$4, %rdx
	addq	%rdx, %rax
	movq	(%rax), %rax
	jmp	L14
L11:
	leave
LCFI13:
	ret
LFE8:
	.cstring
LC1:
	.ascii "thread id = %x\12\0"
	.align 3
LC2:
	.ascii "Need at least one argument to run, n.  If 2, then second is # of threads\0"
	.align 3
LC3:
	.ascii "Will run fib(%d) on %d thread(s)\12\0"
LC4:
	.ascii "fib(%d) = %d\12\0"
