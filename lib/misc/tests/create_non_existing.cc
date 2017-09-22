#include <monsoon/io/fd.h>
#include <iostream>
#include <fstream>

const char* NAME = "create_nonexisting.testfile";

void ensure_file_exists() {
  std::ofstream(NAME) << "foobar";
}

int main() {
  ensure_file_exists();

  try {
    monsoon::io::fd::create(NAME);
    std::cout << "monsoon::fd::create succeeded when it should not\n";
  } catch (...) {
    std::cout << "monsoon::fd::create threw exception\n";
  }
}
