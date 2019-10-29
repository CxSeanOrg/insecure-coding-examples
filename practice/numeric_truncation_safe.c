#include <assert.h>
#include <limits.h>
#include <stdbool.h>

/**
 * Numeric Truncation Error
 * https://cwe.mitre.org/data/definitions/197.html
 */
bool isSafe(unsigned int length, signed char * buf_len) {
  if (length > SCHAR_MAX)
    return false;
  *buf_len = (signed char) length;
  assert(length == *buf_len);
  return true;
}

int main() {
  signed char buf_len = 0;
  assert(!isSafe(UINT_MAX, &buf_len));
}
