#include <cstring>
#include <cassert>
#include <cstdio>

/**
 * I.7: State postconditions
 * https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#i7-state-postconditions
 */
int main() {
  char buffer[64];
  explicit_bzero(buffer, sizeof(buffer));
}