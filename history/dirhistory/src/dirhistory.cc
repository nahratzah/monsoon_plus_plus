#include <monsoon/history/dir/dirhistory.h>
#include <stdexcept>
#include <utility>

namespace monsoon {
namespace history {


dirhistory::dirhistory(filesystem::path dir)
: dir_(std::move(dir))
{
  if (!filesystem::is_directory(dir_))
    throw std::invalid_argument("dirhistory requires a directory path");
  if (!dir_.is_absolute())
    throw std::invalid_argument("dirhistory requires an absolute path");
}

dirhistory::~dirhistory() noexcept {}


}} /* namespace monsoon::history */
