/*
	PIP - Platform Independent Primitives
	
	Stephan Fomenko

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PIBLOCKINGDEQUEUE_H
#define PIBLOCKINGDEQUEUE_H

#include <queue>
#include <condition_variable>

/**
 * @brief A Queue that supports operations that wait for the queue to become non-empty when retrieving an element, and
 * wait for space to become available in the queue when storing an element.
 */
template <typename T, template<typename = T, typename...> class Queue_ = std::deque, typename ConditionVariable_ = std::condition_variable>
class PIBlockingDequeue {
public:
	typedef Queue_<T> QueueType;

	/**
	 * @brief Constructor
	 */
	explicit PIBlockingDequeue(size_t capacity = SIZE_MAX)
			: cond_var_add(new ConditionVariable_()), cond_var_rem(new ConditionVariable_()), max_size(capacity) { }

	/**
	 * @brief Copy constructor. Initialize queue with copy of other container elements. Not thread-safe for other queue.
	 */
	template<typename Iterable,
			 typename std::enable_if<!std::is_arithmetic<Iterable>::value, int>::type = 0>
	explicit PIBlockingDequeue(const Iterable& other): PIBlockingDequeue() {
		mutex.lock();
		for (const T& t : other) data_queue.push_back(t);
		mutex.unlock();
	}

	/**
	 * @brief Thread-safe copy constructor. Initialize queue with copy of other queue elements.
	 */
	explicit PIBlockingDequeue(PIBlockingDequeue<T>& other): PIBlockingDequeue() {
		other.mutex.lock();
		mutex.lock();
		max_size = other.max_size;
		data_queue = other.data_queue;
		mutex.unlock();
		other.mutex.unlock();
	}

	~PIBlockingDequeue() {
		delete cond_var_add;
		delete cond_var_rem;
	}

	/**
	 * @brief Inserts the specified element into this queue, waiting if necessary for space to become available.
	 *
	 * @param v the element to add
	 */
	template<typename Type>
	void put(Type && v) {
		mutex.lock();
		cond_var_rem->wait(mutex, [&]() { return data_queue.size() < max_size; });
		data_queue.push_back(std::forward<Type>(v));
		mutex.unlock();
		cond_var_add->notify_one();
	}

	/**
	 * @brief Inserts the specified element at the end of this queue if it is possible to do so immediately without
	 * exceeding the queue's capacity, returning true upon success and false if this queue is full.
	 *
	 * @param v the element to add
	 * @return true if the element was added to this queue, else false
	 */
	template<typename Type>
	bool offer(Type && v) {
		mutex.lock();
		if (data_queue.size() >= max_size) {
			mutex.unlock();
			return false;
		}
		data_queue.push_back(std::forward<Type>(v));
		mutex.unlock();
		cond_var_add->notify_one();
		return true;
	}

	/**
	 * @brief Inserts the specified element into this queue, waiting up to the specified wait time if necessary for
	 * space to become available.
	 *
	 * @param v the element to add
	 * @param timeoutMs how long to wait before giving up, in milliseconds
	 * @return true if successful, or false if the specified waiting time elapses before space is available
	 */
	template<typename Type>
	bool offer(Type && v, int timeoutMs) {
		mutex.lock();
		bool isOk = cond_var_rem->wait_for(mutex, timeoutMs, [&]() { return data_queue.size() < max_size; } );
		if (isOk) data_queue.push_back(std::forward<Type>(v));
		mutex.unlock();
		if (isOk) cond_var_add->notify_one();
		return isOk;
	}

	/**
	 * @brief Retrieves and removes the head of this queue, waiting if necessary until an element becomes available.
	 *
	 * @return the head of this queue
	 */
	T take() {
		mutex.lock();
		cond_var_add->wait(mutex, [&]() { return data_queue.size() != 0; });
		T t = std::move(data_queue.front());
		data_queue.pop_front();
		mutex.unlock();
		cond_var_rem->notify_one();
		return t;
	}

	/**
	 * @brief Retrieves and removes the head of this queue, waiting up to the specified wait time if necessary for an
	 * element to become available.
	 *
	 * @param timeoutMs how long to wait before giving up, in milliseconds
	 * @param defaultVal value, which returns if the specified waiting time elapses before an element is available
	 * @param isOk flag, which indicates result of method execution. It will be set to false if timeout, or true if
	 * return value is retrieved value
	 * @return the head of this queue, or defaultVal if the specified waiting time elapses before an element is available
	 */
	template<typename Type = T>
	T poll(int timeoutMs, Type && defaultVal = Type(), bool * isOk = nullptr) {
		bool isNotEmpty;
		T t;
		{
			std::unique_lock<std::mutex> lc(mutex);
			isNotEmpty = cond_var_add->wait_for(lc, std::chrono::milliseconds(timeoutMs), [&]() { return data_queue.size() != 0; });

			if (isNotEmpty) {
				t = std::move(data_queue.front());
				data_queue.pop_front();
			} else {
				t = std::forward<Type>(defaultVal);
			}
		}
		if (isNotEmpty) cond_var_rem->notify_one();
		if (isOk) *isOk = isNotEmpty;
		return t;
	}

	/**
	 * @brief Retrieves and removes the head of this queue and return it if queue not empty, otherwise return defaultVal.
	 * Do it immediately without waiting.
	 *
	 * @param defaultVal value, which returns if the specified waiting time elapses before an element is available
	 * @param isOk flag, which indicates result of method execution. It will be set to false if timeout, or true if
	 * return value is retrieved value
	 * @return the head of this queue, or defaultVal if the specified waiting time elapses before an element is available
	 */
	template<typename Type = T>
	T poll(Type && defaultVal = Type(), bool * isOk = nullptr) {
		T t;
		mutex.lock();
		bool isNotEmpty = data_queue.size() != 0;
		if (isNotEmpty) {
			t = std::move(data_queue.front());
			data_queue.pop_front();
		} else {
			t = std::forward<Type>(defaultVal);
		}
		mutex.unlock();
		if (isNotEmpty) cond_var_rem->notifyOne();
		if (isOk) *isOk = isNotEmpty;
		return t;
	}

	/**
	 * @brief Returns the number of elements that this queue can ideally (in the absence of memory or resource
	 * constraints) contains. This is always equal to the initial capacity of this queue less the current size of this queue.
	 *
	 * @return the capacity
	 */
	size_t capacity() {
		size_t c;
		mutex.lock();
		c = max_size;
		mutex.unlock();
		return c;
	}

	/**
	 * @brief Returns the number of additional elements that this queue can ideally (in the absence of memory or resource
	 * constraints) accept. This is always equal to the initial capacity of this queue less the current size of this queue.
	 *
	 * @return the remaining capacity
	 */
	size_t remainingCapacity() {
		mutex.lock();
		size_t c = max_size - data_queue.size();
		mutex.unlock();
		return c;
	}

	/**
	 * @brief Returns the number of elements in this collection.
	 */
	size_t size() {
		mutex.lock();
		size_t s = data_queue.size();
		mutex.unlock();
		return s;
	}

	/**
	 * @brief Removes all available elements from this queue and adds them to other given queue.
	 */
	template<typename Appendable>
	size_t drainTo(Appendable& other, size_t maxCount = SIZE_MAX) {
		mutex.lock();
		size_t count = maxCount > data_queue.size() ? data_queue.size() : maxCount;
		for (size_t i = 0; i < count; ++i) {
			other.push_back(std::move(data_queue.front()));
			data_queue.pop_front();
		}
		mutex.unlock();
		return count;
	}

	/**
	 * @brief Removes all available elements from this queue and adds them to other given queue.
	 */
	size_t drainTo(PIBlockingDequeue<T>& other, size_t maxCount = SIZE_MAX) {
		mutex.lock();
		other.mutex.lock();
		size_t count = maxCount > data_queue.size() ? data_queue.size() : maxCount;
		size_t otherRemainingCapacity = other.max_size - data_queue.size();
		if (count > otherRemainingCapacity) count = otherRemainingCapacity;
		for (size_t i = 0; i < count; ++i) {
			other.data_queue.push_back(std::move(data_queue.front()));
			data_queue.pop_front();
		}
		other.mutex.unlock();
		mutex.unlock();
		return count;
	}

protected:
	std::mutex mutex;
	// TODO change to type without point
	ConditionVariable_ *cond_var_add, *cond_var_rem;
	QueueType data_queue;
	size_t max_size;

};


#endif // PIBLOCKINGDEQUEUE_H
