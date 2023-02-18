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
#include <unifex/typed_via.hpp>
#include <iostream>

#include <cstdio>

using namespace unifex;

/*
    execution架构：
    sender代表一小段任务，任何一个异步任务，都可以分为多个同步任务，所以一个sender就是一个同步任务
    当sync_wait时，用一个sender_consumer调用connect(sender,reciever),把执行流程穿起来，
    穿成一个reciever，当start(op)时，开始set_value(reciever)，然后开始执行
*/



int main() {
    /*参照single_thread_context魔改，采用eventfd和队列来模拟任务调度*/
    event_thread_context ctx;

    auto sch_sender = schedule(ctx.get_scheduler());
    sch_sender
        | then ([]() {
            std::cout << std::this_thread::get_id();
            std::printf(" thread done\n");
            return 1;
        })
        | typed_via(ctx.get_scheduler())
        | then([&](auto x) {
            std::cout << std::this_thread::get_id();
            std::printf(" thread get %d\n",x);
        })
        | sync_wait();
    
    return 0;
}
