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
#include <unifex/static_thread_pool.hpp>

#include <unifex/just.hpp>
#include <unifex/on.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/then.hpp>
#include <unifex/when_all.hpp>

#include <atomic>

#include <gtest/gtest.h>

using namespace unifex;

namespace {
template <typename Scheduler, typename F>
auto run_on(Scheduler&& s, F&& func) {
  return then(schedule((Scheduler &&) s), (F &&) func);
}
} // anonymous namespace

TEST(StaticThreadPool, Smoke) {
  static_thread_pool tpContext;
  auto tp = tpContext.get_scheduler();
  std::atomic<int> x = 0;

  sync_wait(when_all(
      run_on(
          tp,
          [&] {
            ++x;
            std::printf("task 1\n");
          }),
      run_on(
          tp,
          [&] {
            ++x;
            std::printf("task 2\n");
          }),
      run_on(tp, [&] {
        ++x;
        std::printf("task 3\n");
      })));

  sync_wait(on(tp, just()));

  EXPECT_EQ(x, 3);
}
