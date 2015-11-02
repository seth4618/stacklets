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
# void *foo(void *argv) {
#   return (int)argv + 1;
# }
#
# int main() {
#   x = lfork(foo, 3);
#   y = lfork(foo, 5);
#   join();
#   return x + y;
# }
  .comm   stub_base,8,8   # simulation of temperary register psp
  .comm   run_queue,8,8
  .text
  .globl  main

foo:
  addq $1, %rdi
  movq %rdi, %rax
  ret

main:
  pushq %rbp
  movq  %rsp, %rbp
  movq  %rbp, stub_base(%rip)   # stub_base will hold the base of stub
  subq  $24, %rsp         # stacklet stub for retAdr, resumeAdr, join_counter
  subq  $16, %rsp         # stack space for x, y
  movq  $3, %rdi          # load arguments for foo
  call  foo_stub
  jmp   foo_steal
  jmp   foo_susp
  movq  %rax, -8(%rbp)    # -8(%rbp) holds x
  movq  $5, %rdi          # load arugments for foo
  call  foo
  movq  %rax, -16(%rbp)   # -16(%rbp) holds y
cont:
  movq  -8(%rbp), %rax
  addq  -16(%rbp), %rax
  leave
  ret

foo_susp:
  movq  post_dis_ret, %rax
  movq  %rax, stub_base-8(%rip)    # retAdr = post_dis_ret
  movq  checkcont, %rax
  movq  %rax, stub_base-16(%rip)   # resumeAdr = checkcont
  movq  $2, stub_base-24(%rip)     # join_counter = 2
  # fork foo
  #suspend

foo_steal:

post_dis_ret:
  movq  %rax, -8(%rbp)
  subq  $1, stub_base-24(%rip)     # join_count--
  jmp   *stub_base-16(%rip)

checkcont:
  cmp   $0, stub_base-24(%rip)
  jz    checkcont_end
  #suspend
checkcont_end:
  jmp   cont

# The stub will have an prolog and an epilog.
foo_stub:
  popq  stub_base-8(%rip)
  addq  $4, stub_base-8(%rip)      # find "4" through objdump... probably there's a better way
  call  foo
  jmp   *stub_base-8(%rip)         # load the return address
