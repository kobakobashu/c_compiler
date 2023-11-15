#include "test.h"

int g1, g2[4];

int main() {
  ASSERT(3, ({ int a; a=3; a; }));
  ASSERT(8, ({ int a; int z; a=3; z=5; a+z; }));

  ASSERT(6, ({ int a; int b; a=b=3; a+b; }));
  ASSERT(3, ({ int foo; foo=3; foo; }));
  ASSERT(8, ({ int foo123; int bar; foo123=3; bar=5; foo123+bar; }));

  ASSERT(4, ({ int x; sizeof(x); }));
  ASSERT(4, ({ int x; sizeof x; }));
  ASSERT(8, ({ int *x; sizeof(x); }));
  ASSERT(16, ({ int x[4]; sizeof(x); }));
  ASSERT(48, ({ int x[3][4]; sizeof(x); }));
  ASSERT(16, ({ int x[3][4]; sizeof(*x); }));
  ASSERT(4, ({ int x[3][4]; sizeof(**x); }));
  ASSERT(5, ({ int x[3][4]; sizeof(**x) + 1; }));
  ASSERT(5, ({ int x[3][4]; sizeof **x + 1; }));
  ASSERT(4, ({ int x[3][4]; sizeof(**x + 1); }));
  ASSERT(4, ({ int x=1; sizeof(x=2); }));
  ASSERT(1, ({ int x; x=1; sizeof(x=2); x; }));

  ASSERT(0, g1);
  ASSERT(3, ({ g1=3; g1; }));
  ASSERT(0, ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[0]; }));
  ASSERT(1, ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[1]; }));
  ASSERT(2, ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[2]; }));
  ASSERT(3, ({ g2[0]=0; g2[1]=1; g2[2]=2; g2[3]=3; g2[3]; }));

  ASSERT(4, sizeof(g1));
  ASSERT(16, sizeof(g2));

  ASSERT(1, ({ char x; x=1; x; }));
  ASSERT(1, ({ char x; x=1; char y; y=2; x; }));
  ASSERT(2, ({ char x; x=1; char y; y=2; y; }));

  ASSERT(1, ({ char x; sizeof(x); }));
  ASSERT(10, ({ char x[10]; sizeof(x); }));

  ASSERT(2, ({ int x; x=2; { int x; x=3; } x; }));
  ASSERT(2, ({ int x; x=2; { int x; x=3; } int y; y=4; x; }));
  ASSERT(3, ({ int x; x=2; { x=3; } x; }));
  ASSERT(7, ({ int x; int y; char z; char *a=&y; char *b=&z; b-a; }));
  ASSERT(1, ({ int x; char y; int z; char *a=&y; char *b=&z; b-a; }));

  ASSERT(8, ({ long x; sizeof(x); }));
  ASSERT(2, ({ short x; sizeof(x); }));

  printf("OK\n");
  return 0;
}