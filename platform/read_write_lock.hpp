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

#ifndef PLATFORM_READ_WRITE_LOCK_HPP
#define PLATFORM_READ_WRITE_LOCK_HPP

#include <memory>

namespace MaliSDK
{

/// @brief Implements a read-write lock.
///
/// This is a mutex optimized for cases where we almost always have readers
/// locking a data structure,
/// but very rarely writers.
/// On Linux, this is implemented with pthread rw lock.
class ReadWriteLock
{
public:
	/// @brief Constructor
	ReadWriteLock();
	/// @brief Destructor
	~ReadWriteLock();

	/// Locks for readers. Multiple readers can lock without blocking each other.
	void lockRead();
	/// Unlocks for readers.
	void unlockRead();
	/// Locks for writes. Only one writer (and no readers) can be inside critical
	/// regions at the same time.
	void lockWrite();
	/// Unlocks for writes.
	void unlockWrite();

private:
	/// Pimpl idiom. Hides implementation details.
	struct Impl;
	/// The pimpl.
	std::unique_ptr<Impl> pImpl;
};
}

#endif
