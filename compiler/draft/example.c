#include <stddef.h>

typedef void *(*pfunc)(void *);
int lfork(pfunc, void *);
void join();

pfunc foo;
int t;

int main()
{
  int x = lfork(foo, NULL);
  int y = lfork(foo, NULL);
  t = 1;
  join();
  return 0;
}
