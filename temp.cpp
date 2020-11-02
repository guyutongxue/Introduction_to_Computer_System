#include <cstddef>

union U {
  char a[7];
//   short b;
};

struct S {
  char d[3];
  U u;
  int i;
};

constexpr unsigned i = sizeof(S);
constexpr unsigned j = sizeof(U);