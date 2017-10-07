#include <monsoon/history/dir/tsdata.h>
#include <iostream>
#include <cstddef>
#include <numeric>

int main(int argc, char** argv) {
  if (argc <= 1) {
    std::cerr << "Usage: " << (argc > 0 ? argv[0] : "count") << " files...\n";
    return 1;
  }

  const auto count = std::accumulate(argv + 1, argv + argc, std::size_t(0),
      [](std::size_t v, char* fname) {
        std::cerr << "Counting number of scrapes in " << fname << "...\n";
        try {
          auto tsfile = monsoon::history::tsdata::open(fname);
          if (tsfile != nullptr)
            return v + tsfile->read_all().size();
        } catch (const std::exception& e) {
          std::cerr << "Exception " << e.what() << " while reading " << fname << std::endl;
        }
        return v;
      });
  std::cout << count << std::endl;
}
