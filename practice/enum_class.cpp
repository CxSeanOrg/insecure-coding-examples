#include <type_traits>
#include <ostream>
#include <iostream>

enum class Direction : char
{ NORTH = 'N', EAST  = 'E', WEST  = 'W', SOUTH = 'S' };

std::ostream& operator << (std::ostream& os, const Direction& obj) {
  os << static_cast<std::underlying_type<Direction>::type>(obj);
  return os;
}

int main() {
  std::cout << "\t" << Direction::NORTH << "\n"
            << "\t" << Direction::EAST  << "\n"
            << "\t" << Direction::WEST  << "\n"
            << "\t" << Direction::SOUTH << "\n";
}
