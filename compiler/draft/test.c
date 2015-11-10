//#include<inttypes.h>
//
//#define getFramePointer(x) do { \
//    asm volatile("movq %%rbp,%[output]" : [output] "=r" (x)); } while (0)
//
//#define restoreFramePointer(x) do { \
//    asm volatile("movq %[sp],%%rbp" : : [sp] "r" (x) : "rbp");} while (0)
//
//#define getStackPointer(x) do { \
//    asm volatile("movq %%rsp,%[output]" : [output] "=r" (x)); } while (0)
//
//#define restoreStackPointer(x) do { \
//    asm volatile("movq %[sp],%%rsp" : : [sp] "r" (x) : "rsp");} while (0)
//
//
//int foo(uint64_t rbp, uint64_t rsp)
//{
//    restoreStackPointer(rsp);
//    restoreFramePointer(rbp);
//    asm volatile ("jmp main_next");
//    return 0;
//}
//
//int main()
//{
//    int x;
//    uint64_t rbp, rsp;
//    getStackPointer(rsp);
//    getFramePointer(rbp);
//    foo(rbp, rsp);
//    asm volatile ("main_next:");
//    x = 1;
//    return x;
//}
//

//#define save() do { \
//    asm volatile("movq %%rcx, %[indexCX]  \n"\
//                 "movq %%rdx, %[indexDX]  \n"\
//                 : [indexCX] "=m" (array[0]),\
//                   [indexDX] "=m" (array[1]));} while (0)
//
//int foo(int i)
//{
//    int array[2];
//    save();
//    return i;
//}
//
//int main()
//{
//    int array[2];
//    save();
//
//
//    return foo(2);
//}
#include <setjmp.h>
#include <stdio.h>

jmp_buf buf;
int global;

void foo()
{
    global = 1;
    printf("foo\n");
    longjmp(buf, 1);
}

int main()
{
    volatile int local;
    local = 0 + 3;
//    int r = setjmp(buf);
//    if (r == 0)
//    {
//        local = 1;
//        printf("r == 0\n");
//        foo();
//    }
//    printf("jmp success global:%d, local:%d \n", global, local);
//    int x = 1;
    return local;
}
