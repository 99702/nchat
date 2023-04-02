//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2022
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/utils/port/config.h"

#ifdef TD_THREAD_STL

#include "td/utils/common.h"
#include "td/utils/invoke.h"
#include "td/utils/port/detail/ThreadIdGuard.h"
#include "td/utils/port/thread_local.h"
#include "td/utils/Slice.h"
#include "td/utils/Status.h"

#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

namespace td {
namespace detail {

class ThreadStl {
 public:
  ThreadStl() = default;
  ThreadStl(const ThreadStl &other) = delete;
  ThreadStl &operator=(const ThreadStl &other) = delete;
  ThreadStl(ThreadStl &&) = default;
  ThreadStl &operator=(ThreadStl &&) = default;
  ~ThreadStl() {
    join();
  }

  template <class Function, class... Args>
  explicit ThreadStl(Function &&f, Args &&...args) {
    thread_ = std::thread([args = std::make_tuple(decay_copy(std::forward<Function>(f)),
                                                  decay_copy(std::forward<Args>(args))...)]() mutable {
      ThreadIdGuard thread_id_guard;
      invoke_tuple(std::move(args));
      clear_thread_locals();
    });
  }

  void join() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void detach() {
    if (thread_.joinable()) {
      thread_.detach();
    }
  }

  void set_name(CSlice name) {
    // not supported
  }

  static unsigned hardware_concurrency() {
    return std::thread::hardware_concurrency();
  }

#if TD_WINDOWS
  using id = HANDLE;
#else
  using id = std::thread::id;
#endif

  static void send_real_time_signal(id thread_id, int real_time_signal_number) {
    // not supported
  }

  static Status set_affinity_mask(id thread_id, uint64 mask) {
#if TD_WINDOWS
    if (static_cast<DWORD_PTR>(mask) != mask) {
      return Status::Error("Invalid thread affinity mask specified");
    }
    if (SetThreadAffinityMask(thread_id, static_cast<DWORD_PTR>(mask))) {
      return Status::OK();
    }
    return OS_ERROR("Failed to set thread affinity mask");
#else
    return Status::Error("Unsupported");
#endif
  }

  static uint64 get_affinity_mask(id thread_id) {
#if TD_WINDOWS
    DWORD_PTR process_mask = 0;
    DWORD_PTR system_mask = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
      auto result = SetThreadAffinityMask(thread_id, process_mask);
      if (result != 0 && result != process_mask) {
        SetThreadAffinityMask(thread_id, result);
      }
      return result;
    }
#endif
    return 0;
  }

 private:
  std::thread thread_;

  template <class T>
  std::decay_t<T> decay_copy(T &&v) {
    return std::forward<T>(v);
  }
};

namespace this_thread_stl {
#if TD_WINDOWS
inline ThreadStl::id get_id() {
  return GetCurrentThread();
}
#else
using std::this_thread::get_id;
#endif
}  // namespace this_thread_stl

}  // namespace detail
}  // namespace td

#endif
