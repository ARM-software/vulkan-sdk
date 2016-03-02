/* Copyright (c) 2016-2017, ARM Limited and Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "thread_pool.hpp"
#include <utility>

using namespace std;

namespace MaliSDK
{
void ThreadPool::setWorkerThreadCount(unsigned workerThreadCount)
{
	workerThreads.clear();
	for (unsigned i = 0; i < workerThreadCount; i++)
		workerThreads.emplace_back(new Worker);
}

void ThreadPool::pushWorkToThread(unsigned threadIndex, std::function<void()> func)
{
	workerThreads[threadIndex]->pushWork(move(func));
}

void ThreadPool::waitIdle()
{
	for (auto &worker : workerThreads)
		worker->waitIdle();
}

ThreadPool::Worker::Worker()
{
	workerThread = thread(&ThreadPool::Worker::threadEntry, this);
}

ThreadPool::Worker::~Worker()
{
	if (workerThread.joinable())
	{
		waitIdle();

		lock.lock();
		threadIsAlive = false;
		cond.notify_one();
		lock.unlock();

		workerThread.join();
	}
}

void ThreadPool::Worker::pushWork(std::function<void()> func)
{
	lock_guard<mutex> holder{ lock };
	workQueue.push(move(func));
	cond.notify_one();
}

void ThreadPool::Worker::waitIdle()
{
	unique_lock<mutex> holder{ lock };
	cond.wait(holder, [this] { return workQueue.empty(); });
}

void ThreadPool::Worker::threadEntry()
{
	for (;;)
	{
		function<void()> *pWork = nullptr;
		{
			unique_lock<mutex> holder{ lock };
			cond.wait(holder, [this] { return !workQueue.empty() || !threadIsAlive; });
			if (!threadIsAlive)
				break;

			pWork = &workQueue.front();
		}

		(*pWork)();

		{
			lock_guard<mutex> holder{ lock };
			workQueue.pop();
			cond.notify_one();
		}
	}
}
}
