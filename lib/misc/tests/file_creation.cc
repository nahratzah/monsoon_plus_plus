#include <monsoon/io/fd.h>
#include <string>
#include <iostream>
#include <fstream>

#if defined(WIN32)
# include <Windows.h>
#else
# include <unistd.h>
#endif

const char* NAME = "create_nonexisting.testfile";

void create_foobar() {
  auto foobar = monsoon::io::fd::create(NAME);
  foobar.write("foobar", 6);
}

void ensure_foobar_txt_is_absent() {
#if WIN32
  DeleteFile(NAME);
#else
  unlink(NAME);
#endif
}

int main() {
  ensure_foobar_txt_is_absent();
  create_foobar();

  std::string data;
  std::ifstream(NAME) >> data;
  std::cout << data;
}
