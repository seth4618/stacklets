# Stacklets
# two lforks followed by a join
#
# This is handcrafted assembly code for the program:
#
# //test return 10
# typedef void *(*pfunc)(void *);
# void *sthread(pfunc, void*);
# void join();
#
# int foo_id;
# 
# void *foo(void *argv) {
#   foo_id = get_id();
#   suspend();
#   return (int)argv + 1;
# }
#
# void *bar(void *argv) {
#   int x = (int)argv + 2;
#   unsuspend(foo_id);
#   return x;
# }
#
# int main() {
#   x = lfork(foo, 3);
#   y = lfork(bar, 5);
#   join();
#   return x + y;
# }

  .comm   psp1,8,8      # points to the return address of foo
  .comm   psp,8,8       # simulation of temperary register psp
  .comm   run_queue,8,8
  .comm   foo_id,8,8
  .text
  .globl  main

foo:
  call  get_id
  movq  %rax, foo_id(%rip)
  call  suspend
  addq  $1, %rdi
  movq  %rdi, %rax
  ret

# The stub will have an prolog and an epilog.
# retAdr
# resumeAdr
# sync_counter
# x address, where to store the return value of bar
# y address
foo_stub:
  movq  %rsp, psp1(%rip)
  addq  $4, (%rsp)            # find "4" through objdump... probably there's a better way 
  pushq $0                    # resumeAdr
  pushq $0                    # sync_counter
  pushq %rbp  
  subq  $8, (%rsp)
  pushq %rbp
  subq  $16, (%rsp)
  call  foo
  addq  $32, %rsp
  ret

foo_susp:
  movq  psp(%rip), %rax
  movq  inlet_foo, %r11
  movq  %r11, (%rax)
  movq  checkcont, %r11
  movq  %r11, -8(%rax)
  movq  $2, -16(%rax)         # sync_counter = 2
  movq  bar, %rdi
  movq  $5, %rsi
  movq  inlet_bar, %rdx
  call  fork
  call  deschedule
  ret

inlet_foo:
  movq  -24(%rip), %rbx
  movq  %rax, (%rbx)
  sub   $1, psp1-16(%rip)
  jmp   *-8(%rip)

bar:
  leaq  2(%rdi), %rax
  movq  foo_id(%rip), %rdi
  call  unsuspend
  ret

inlet_bar:
  movq  psp1-32(%rip), %rbx
  movq  %rax, (%rbx)
  subq  $1, psp1-16(%rip)
  jmp   *psp1-8(%rip)

checkcont:
  cmp   $0, psp1-16(%rip)
  jz    checkcont_end
  call  suspend
checkcont_end:
  jmp   cont

main:
  pushq %rbp
  movq  %rsp, %rbp
  movq  %rbp, psp(%rip)       # psp will hold the base of stub
  subq  $24, %rsp             # stacklet stub for retAdr, resumeAdr, join_counter
  subq  $16, %rsp             # stack space for x, y
  movq  $3, %rdi              # load arguments for foo
  call  foo_stub
  jmp   foo_steal
  jmp   foo_susp
  movq  %rax, -8(%rbp)        # -8(%rbp) holds x
  movq  $5, %rdi              # load arugments for foo
  call  foo
  movq  %rax, -16(%rbp)       # -16(%rbp) holds y
cont:
  movq  -8(%rbp), %rax
  addq  -16(%rbp), %rax
  leave
  ret

// TODO
foo_steal:

# Library TODO in c
stub_handler:
  movq  psp-16(%rip), %rsp
  jmp   *psp-8(%rip)

suspend:
  movq  psp1(%rip), %rax
  movq  (%rax), %rbx
  subq  $2, %rbx
  jmp   *%rbx

get_id:
  movq  $1, %rax
  ret

fork:
  # allocate stacklet
  pushq %rdi
  pushq %rsi
  pushq %rdx
  movq  $1024, %rdi
  call  malloc
  add   $1024, %rax
  popq  %rdx
  popq  %rsi
  popq  %rdi

  # initialize stacklet
  # -8 inlet
  # -16 sp
  # -24 top
  # -32 pc
  # -40 where to store the return value
  # -48 stub_handler
  movq  %rdx, -8(%rax)
  movq  psp1(%rip), %r11
  movq  %r11, -16(%rax)
  movq  %rax, %rbx
  subq  $40, %rbx
  movq  %rbx, -24(%rax)  
  movq  %rdi, -32(%rax)
  movq  stub_handler, %r11
  movq  %r11, -48(%rax)
  movq  %rsi, -56(%rax)

  # push to run queue
  movq  %rax, run_queue(%rip)
  ret

deschedule:
  # move the current thread from run queue to suspend queue
  # pick a thread from run queue and run it
  popq  %rdx
  movq  %rdx, psp-32(%rip)
  movq  run_queue(%rip), %r11
  movq  %r11, psp(%rip)
  movq  -16(%r11), %rsp
  movq  -48(%r11), %rdi
  movq  -32(%r11), %rdx
  pushq %rdx
  ret

unsuspend:
  # move the thread from suspend queue to run queue
  ret
