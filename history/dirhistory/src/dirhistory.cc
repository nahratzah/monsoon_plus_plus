#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/dir/tsdata.h>
#include <stdexcept>
#include <utility>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include "v2/tsdata.h"

namespace monsoon {
namespace history {


dirhistory::dirhistory(filesystem::path dir, bool open_for_write)
: dir_(std::move(dir)),
  writable_(open_for_write)
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
        if ((*fiter)->is_writable()
            && std::get<0>((*fiter)->time()) > std::get<0>(write_file_->time()))
          write_file_ = *fiter;
      }
    }
  }
}

dirhistory::~dirhistory() noexcept {}

void dirhistory::push_back(const time_series& ts) {
  maybe_start_new_file_(ts.get_time());
  write_file_->push_back(ts);
}

auto dirhistory::simple_groups(const time_range& tr) const
-> std::unordered_set<simple_group> {
  std::unordered_set<simple_group> result;

  for (const auto& file : files_) {
    time_point fbegin, fend;
    std::tie(fbegin, fend) = file->time();
    if (fbegin <= tr.end() && fend >= tr.begin()) {
      auto fsubset = file->simple_groups();
#if __cplusplus >= 201703
      if (result.get_allocator() == fsubset.get_allocator())
        result.merge(std::move(fsubset));
      else
#else
        result.insert(
            std::make_move_iterator(fsubset.begin()),
            std::make_move_iterator(fsubset.end()));
#endif
    }
  }

  return result;
}

auto dirhistory::group_names(const time_range& tr) const
-> std::unordered_set<group_name> {
  std::unordered_set<group_name> result;

  for (const auto& file : files_) {
    time_point fbegin, fend;
    std::tie(fbegin, fend) = file->time();
    if (fbegin <= tr.end() && fend >= tr.begin()) {
      auto fsubset = file->group_names();
#if __cplusplus >= 201703
      if (result.get_allocator() == fsubset.get_allocator())
        result.merge(std::move(fsubset));
      else
#else
        result.insert(
            std::make_move_iterator(fsubset.begin()),
            std::make_move_iterator(fsubset.end()));
#endif
    }
  }

  return result;
}

auto dirhistory::untagged_metrics(const monsoon::time_range& tr) const
-> std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> {
  std::unordered_set<std::tuple<simple_group, metric_name>, metrics_hash> result;

  for (const auto& file : files_) {
    time_point fbegin, fend;
    std::tie(fbegin, fend) = file->time();
    if (fbegin <= tr.end() && fend >= tr.begin()) {
      auto fsubset = file->untagged_metrics();
#if __cplusplus >= 201703
      if (result.get_allocator() == fsubset.get_allocator())
        result.merge(std::move(fsubset));
      else
#else
        result.insert(
            std::make_move_iterator(fsubset.begin()),
            std::make_move_iterator(fsubset.end()));
#endif
    }
  }

  return result;
}

auto dirhistory::tagged_metrics(const monsoon::time_range& tr) const
-> std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> {
  std::unordered_set<std::tuple<group_name, metric_name>, metrics_hash> result;

  for (const auto& file : files_) {
    time_point fbegin, fend;
    std::tie(fbegin, fend) = file->time();
    if (fbegin <= tr.end() && fend >= tr.begin()) {
      auto fsubset = file->tagged_metrics();
#if __cplusplus >= 201703
      if (result.get_allocator() == fsubset.get_allocator())
        result.merge(std::move(fsubset));
      else
#else
        result.insert(
            std::make_move_iterator(fsubset.begin()),
            std::make_move_iterator(fsubset.end()));
#endif
    }
  }

  return result;
}

void dirhistory::maybe_start_new_file_(time_point tp) {
  using std::to_string;

  if (!writable_) throw std::runtime_error("history is not writable");

  if (write_file_ == nullptr) {
    auto fname = dir_ / decide_fname_(tp);
    io::fd new_file = io::fd::create(fname.native());
    try {
      new_file = io::fd::create(fname.native());
    } catch (...) {
      for (int i = 0; i < 100; ++i) {
        try {
          new_file = io::fd::create(fname.native() + "-" + to_string(i));
        } catch (...) {
          continue;
        }
        break;
      }
    }

    if (!new_file.is_open())
      throw std::runtime_error("unable to create file");
    try {
      auto new_file_ptr = v2::tsdata_v2::new_list_file(std::move(new_file), tp); // write mime header
      files_.push_back(new_file_ptr);
      write_file_ = new_file_ptr; // Fill in write_file_ pointer
    } catch (...) {
      new_file.unlink();
      throw;
    }
  }
}

auto dirhistory::decide_fname_(time_point tp) -> filesystem::path {
  return (std::ostringstream()
      << std::setfill('0')
      << std::right
      << "monsoon-"
      << std::setw(4) << tp.year()
      << std::setw(2) << tp.month()
      << std::setw(2) << tp.day_of_month()
      << "-"
      << std::setw(2) << tp.hour()
      << std::setw(2) << tp.minute()
      << ".tsd")
      .str();
}


}} /* namespace monsoon::history */
