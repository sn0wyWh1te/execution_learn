/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License Version 2.0 with LLVM Exceptions
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *   https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <unifex/manual_event_loop.hpp>
#include <unifex/linux/safe_file_descriptor.hpp>

#include <cstring>
#include <system_error>
#include <thread>

#include <fcntl.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

namespace unifex {
namespace _manual_event_loop {

void context::run() {
  std::unique_lock lock{mutex_};
  while (true) {
    while (head_ == nullptr) {
      if (stop_) return;
      cv_.wait(lock);
    }
    auto* task = head_;
    head_ = task->next_;
    if (head_ == nullptr) {
      tail_ = nullptr;
    }
    lock.unlock();
    task->execute();
    lock.lock();
  }
}

void context::stop() {
  std::unique_lock lock{mutex_};
  stop_ = true;
  cv_.notify_all();
}

void context::enqueue(task_base* task) {
  std::unique_lock lock{mutex_};
  if (head_ == nullptr) {
    head_ = task;
  } else {
    tail_->next_ = task;
  }
  tail_ = task;
  task->next_ = nullptr;
  cv_.notify_one();
}

} // _manual_event_loop
} // unifex


namespace unifex {
namespace _eventfd_event_loop {

context::context() {
  {
    int fd = epoll_create(1);
    if (fd < 0) {
      int errorCode = errno;
      throw(std::system_error{errorCode, std::system_category(), "epoll_create(1)"});
    }
    epollFd_ = fd;
  }

  {
    int fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (fd < 0) {
      int errorCode = errno;
      throw(std::system_error{errorCode, std::system_category(), "create remoteQueueEventFd_"});
    }

    remoteQueueEventFd_ = fd;
  }

  {
    epoll_event event = {};
    event.events = EPOLLIN;
    event.data.ptr = nullptr;
    int result = epoll_ctl(epollFd_, EPOLL_CTL_ADD, remoteQueueEventFd_, &event);
    if (result < 0) {
      int errorCode = errno;
      throw(std::system_error{errorCode, std::system_category(), "epoll_ctl EPOLL_CTL_ADD remoteQueueEventFd_"});
    }
  }
}

context::~context() {
  epoll_event event = {};
  (void)epoll_ctl(epollFd_, EPOLL_CTL_DEL, remoteQueueEventFd_, &event);
}

void context::run() {
  while (1) {
    epoll_event completions[10];
    int result = epoll_wait(epollFd_,completions, 10,-1);
    if (result < 0) {
      int errorCode = errno;
      throw(std::system_error{errorCode, std::system_category(), "epoll_wait"});
    }
    std::uint32_t count = result;

    std::printf("got %u completions\n", count);

    for (std::uint32_t i = 0; i < count; ++i) {
      auto& completed = completions[i];

      if (head_ != nullptr) {
        std::printf("got remote queue wakeup\n");

        // Read the eventfd to clear the signal.
        std::uint64_t buffer;
        ssize_t bytesRead =
            read(remoteQueueEventFd_, &buffer, sizeof(buffer));
        if (bytesRead < 0) {
          // read() failed
          [[maybe_unused]] int errorCode = errno;
          std::printf("read on eventfd failed with %i\n", errorCode);

          std::terminate();
        }

        UNIFEX_ASSERT(bytesRead == sizeof(buffer));

        std::unique_lock lock{mutex_};
        while (head_ != nullptr) {
          auto* task = head_;
          head_ = task->next_;
          if (head_ == nullptr) {
            tail_ = nullptr;
          }
          lock.unlock();
          task->execute();
          lock.lock();
        }
        continue;
      }
      std::printf("completion event %i\n", completed.events);
    }
    if (stop_)
    break;
  }
}

void context::signal_remote_queue() {
  std::printf("writing bytes to eventfd\n");

  // Notify eventfd() by writing a 64-bit integer to it.
  const std::uint64_t value = 1;
  ssize_t bytesWritten =
      write(remoteQueueEventFd_, &value, sizeof(value));
  if (bytesWritten < 0) {
    // What to do here? Terminate/abort/ignore?
    // Try to dequeue the item before returning?
    [[maybe_unused]] int errorCode = errno;
    std::printf("writing to remote queue eventfd failed with %i\n", errorCode);

    std::terminate();
  }

  UNIFEX_ASSERT(bytesWritten == sizeof(value));
}

void context::stop() {
  std::unique_lock lock{mutex_};
  stop_ = true;
  signal_remote_queue();
}

void context::enqueue(task_base* task) {
  std::unique_lock lock{mutex_};
  if (head_ == nullptr) {
    head_ = task;
  } else {
    tail_->next_ = task;
  }
  tail_ = task;
  task->next_ = nullptr;
  signal_remote_queue();
}

} // _eventfd_event_loop
} // unifex