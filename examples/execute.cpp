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
#include <unifex/single_thread_context.hpp>
#include <unifex/execute.hpp>
#include <unifex/scheduler_concepts.hpp>
#include <unifex/just.hpp>
#include <unifex/then.hpp>
#include <unifex/sync_wait.hpp>
#include <unifex/via.hpp>

#include <cstdio>

using namespace unifex;

int main() {
    single_thread_context ctx;

    auto sch_sender = schedule(ctx.get_scheduler());
    auto res = sch_sender
        | then ([]() {
            std::printf("done\n");
            return 1;
        });

    std::printf("return val is %d\n",sync_wait(res).value());
    
    return 0;
}
