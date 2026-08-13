#include <iostream>
#include <string>
#include <iomanip>

void clearScreen() {
  std::cout << "\033[2J\033[H" << std::flush;
}

// Convert hex into ANSI colored text
void printHex(const std::string& hex, const std::string& text) {
  std::string clean_hex = hex;
  if (clean_hex[0] == '#') {
    clean_hex.erase(0, 1);
  }

  // Parse RGB components from hex substring
  int r = std::stoi(clean_hex.substr(0, 2), nullptr, 16);
  int g = std::stoi(clean_hex.substr(2, 2), nullptr, 16);
  int b = std::stoi(clean_hex.substr(4, 2), nullptr, 16);

  // Print using ANSI 24-bit truecolor escape sequence
  std::cout << "\033[38;2;" << r << ";" << g << ";" << b << "m"
            << text
            << "\033[0m"; // Reset color
}

int main() {
  printHex("#2BF069", "Hex is working!\n");
  return 0;
}
