#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/dir/tsdata.h>
#include <stdexcept>
#include <utility>

namespace monsoon {
namespace history {


dirhistory::dirhistory(filesystem::path dir, bool open_for_write)
: dir_(std::move(dir))
{
  using filesystem::perms;

  if (!filesystem::is_directory(dir_))
    throw std::invalid_argument("dirhistory requires a directory path");
  if (!dir_.is_absolute())
    throw std::invalid_argument("dirhistory requires an absolute path");
  if (open_for_write &&
      !(filesystem::status(dir_).permissions() & perms::owner_write))
    throw std::invalid_argument("dirhistory path is not writable");

  // Scan directory for files to manage.
  std::for_each(
      filesystem::directory_iterator(dir_), filesystem::directory_iterator(),
      [this, open_for_write](const filesystem::path& fname) {
        const auto fstat = filesystem::status(fname);
        if (filesystem::is_regular_file(fstat)) {
          io::fd::open_mode mode = io::fd::READ_WRITE;
          if (!open_for_write ||
              !(fstat.permissions() & perms::owner_write))
            mode = io::fd::READ_ONLY;

          auto fd = io::fd(fname.native(), mode);
          if (tsdata::is_tsdata(fd))
            files_.push_back(tsdata::open(std::move(fd)));
        }
      });

  if (open_for_write) { // Find a write candidate.
    auto fiter = std::find_if(files_.begin(), files_.end(),
        [](const auto& tsdata_ptr) { return tsdata_ptr->is_writable(); });
    if (fiter != files_.end()) {
      write_file_ = *fiter;

      while (++fiter != files_.end()) {
        if ((*fiter)->is_writable() && std::get<0>((*fiter)->time()) > std::get<0>(write_file_->time()))
          write_file_ = *fiter;
      }
    }
  }
}

dirhistory::~dirhistory() noexcept {}


}} /* namespace monsoon::history */
