// int fib(int n) {
//   if (n <= 2) return 1;
//   int x = lfork(fib, n-1);
//   int y = lfork(fib, n-2);
//   join();
//   return (x+y);
// }
// 
// int main() {
//   return lfork(fib, 4);
// }

#include<stdlib.h>

void *seed_stack[1000];
int seed_top = -1;

void *pop_seed()
{
    if (seed_top == -1) return 0;
    return seed_stack[seed_top--];
}

void push_seed(void *seed)
{
    seed_stack[++seed_top] = seed;
}

void yield() {
    void **x = pop_seed();
    if(x == NULL) return;

    void *rbp = *x;
    void *ret = (void *)((char *)(x+1) + 2);
    *(x+1) = (void *)((char *)(x+1) + 6);
    asm volatile("movq %0, %%rbp        \n\
                  movq $2, -20(%%rbp)   \n\
                  call *%1"
                 :
                 :"r"(rbp), "r"(ret)
                 :);
    return;
}

// thread is the function a pthread will run. It will first check in a loop if
// the seed stack has something. When it detects a seed, the thread will pop the
// seed out and start executing that work.
void thread() {
    yield();
}

// In fib, we first push the seed to seed stack. The seed is the base stack
// pointer. The second thing we need to do is to set up a local jump table.
int fib(int n) {
    if (n <= 2) return 1;
    int sync_counter = 0;
    void *seed;
    asm volatile("movq %%rbp, %0 \n"
                 :"=r"(seed)
                 :
                 :);
    push_seed(seed);
    int x;
    asm volatile goto ("movq %1, %%rdi    \n\
                        call fib          \n\
                        jmp ret           \n\
                        jmp y_steal       \n\
                        jmp fib_susp      \n\
                        jmp fib_steal     \n\
                        ret:              \n\
                        movq %%rax, %0"
                        :"=r"(x)
                        :"r"(n-1)
                        :
                        :ret, y_steal, fib_susp, fib_steal);
    int y;
y_steal:
    y = fib(n-2);
cont:
    return (x+y);

// fib_steal will check if the sync_counter is zero. If it is not, then this
// function will call yield.
fib_steal:
    sync_counter--;
    if (sync_counter) yield();
}

int main() {
    return fib(4);
}
