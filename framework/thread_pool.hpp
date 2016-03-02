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

#ifndef FRAMEWORK_THREAD_POOL_HPP
#define FRAMEWORK_THREAD_POOL_HPP

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace MaliSDK
{

/// @brief Implements a simple thread pool which can be used to submit rendering
/// work to multiple
/// threads. It does not aim to distribute chunks of work dynamically to
/// threads, but API users
/// must submit work to particular worker threads.
class ThreadPool
{
public:
	/// @brief Sets the number of worker threads to spawn.
	///
	/// This call is heavyweight and should not be called more than once during
	/// initialization.
	/// To get a platform specific optimal count to use, use
	/// `MaliSDK::OS::getNumberOfCpuThreads`.
	/// @param workerThreadCount The number of worker threads.
	void setWorkerThreadCount(unsigned workerThreadCount);

	/// @brief Gets the current number of worker threads.
	unsigned getWorkerThreadCount() const
	{
		return workerThreads.size();
	}

	/// @brief Pushes a bundle of work to a thread.
	/// @param threadIndex The worker thread to push work to.
	/// @param func A generic function object which will be executed by the
	/// thread.
	/// Using C++11 lambdas is the intended way to create these objects.
	void pushWorkToThread(unsigned threadIndex, std::function<void()> func);

	/// @brief Waits for all worker threads to complete all work they have been
	/// assigned.
	void waitIdle();

private:
	class Worker
	{
	public:
		Worker();
		~Worker();

		void pushWork(std::function<void()> func);
		void waitIdle();

	private:
		std::thread workerThread;
		std::mutex lock;
		std::condition_variable cond;

		std::queue<std::function<void()>> workQueue;
		bool threadIsAlive = true;

		void threadEntry();
	};

	std::vector<std::unique_ptr<Worker>> workerThreads;
};
}

#endif
