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

#ifndef PIEXECUTOR_H
#define PIEXECUTOR_H

#include "piblockingdequeue.h"
#include <atomic>
#include <future>

/**
 * @brief Wrapper for custom invoke operator available function types.
 * @note Source from: "Энтони Уильямс, Параллельное программирование на С++ в действии. Практика разработки многопоточных
 * программ. Пер. с англ. Слинкин А. А. - M.: ДМК Пресс, 2012 - 672c.: ил." (page 387)
 */
class FunctionWrapper {
	struct ImplBase {
		virtual void call() = 0;
		virtual ~ImplBase() = default;
	};

	std::unique_ptr<ImplBase> impl;

	template<typename F>
	struct ImplType: ImplBase {
		F f;
		explicit ImplType(F&& f): f(std::forward<F>(f)) {}
		void call() final { f(); }
	};
public:
	template<typename F, typename = std::enable_if<!std::is_same<F, FunctionWrapper>::value> >
	explicit FunctionWrapper(F&& f): impl(new ImplType<F>(std::forward<F>(f))) {}

	void operator()() { impl->call(); }

	explicit operator bool() const noexcept { return static_cast<bool>(impl); }

	FunctionWrapper() = default;
	FunctionWrapper(FunctionWrapper&& other) noexcept : impl(std::move(other.impl)) {}
	FunctionWrapper& operator=(FunctionWrapper&& other) noexcept {
		impl = std::move(other.impl);
		return *this;
	}

	FunctionWrapper(const FunctionWrapper& other) = delete;
	FunctionWrapper& operator=(const FunctionWrapper&) = delete;
};

template <typename Thread_ = std::thread, typename Dequeue_ = PIBlockingDequeue<FunctionWrapper>>
class PIThreadPoolExecutorTemplate {
protected:
	enum thread_command {
		run,
		shutdown_c,
		shutdown_now
	};

public:
	explicit PIThreadPoolExecutorTemplate(size_t corePoolSize = 1) : thread_command_(thread_command::run) { makePool(corePoolSize); }

	virtual ~PIThreadPoolExecutorTemplate() {
		shutdownNow();
		awaitTermination(1000);
		while (threadPool.size() > 0) {
			auto thread = threadPool.back();
			threadPool.pop_back();
			delete thread;
		}
	}

	template<typename FunctionType>
	std::future<typename std::result_of<FunctionType()>::type> submit(FunctionType&& callable) {
		typedef typename std::result_of<FunctionType()>::type ResultType;

		if (thread_command_ == thread_command::run) {
			std::packaged_task<ResultType()> callable_task(std::forward<FunctionType>(callable));
			auto future = callable_task.get_future();
			FunctionWrapper functionWrapper(callable_task);
			taskQueue.offer(std::move(functionWrapper));
			return future;
		} else {
			return std::future<ResultType>();
		}
	}

	template<typename FunctionType>
	void execute(FunctionType&& runnable) {
		if (thread_command_ == thread_command::run) {
			FunctionWrapper function_wrapper(std::forward<FunctionType>(runnable));
			taskQueue.offer(std::move(function_wrapper));
		}
	}

	void shutdown() {
		thread_command_ = thread_command::shutdown_c;
	}

	void shutdownNow() {
		thread_command_ = thread_command::shutdown_now;
	}

	bool isShutdown() const {
		return thread_command_;
	}

	bool awaitTermination(int timeoutMs) {
		using namespace std::chrono;

		auto start_time = high_resolution_clock::now();
		for (size_t i = 0; i < threadPool.size(); ++i) {
			int dif = timeoutMs - static_cast<int>(duration_cast<milliseconds>(high_resolution_clock::now() - start_time).count());
			if (dif < 0) return false;
			// TODO add wait with timeout
			threadPool[i]->join();
//			if (!threadPool[i]->waitFinish(dif)) return false;
		}
		return true;
	}

protected:
	std::atomic<thread_command> thread_command_;
	Dequeue_ taskQueue;
	std::vector<Thread_*> threadPool;

	template<typename Function>
	PIThreadPoolExecutorTemplate(size_t corePoolSize, Function&& onBeforeStart) : thread_command_(thread_command::run) {
		makePool(corePoolSize, std::forward<Function>(onBeforeStart));
	}

	void makePool(size_t corePoolSize, std::function<void(Thread_*)>&& onBeforeStart = [](Thread_*){}) {
		for (size_t i = 0; i < corePoolSize; ++i) {
			auto* thread = new Thread_([&, i](){
				do {
					auto runnable = taskQueue.poll(100);
					if (runnable) {
						runnable();
					}
				} while (!thread_command_ || taskQueue.size() != 0);
			});
			threadPool.push_back(thread);
			onBeforeStart(thread);
		}
	}
};

typedef PIThreadPoolExecutorTemplate<> PIThreadPoolExecutor;

#ifdef DOXYGEN
/**
 * @brief Thread pools address two different problems: they usually provide improved performance when executing large
 * numbers of asynchronous tasks, due to reduced per-task invocation overhead, and they provide a means of bounding and
 * managing the resources, including threads, consumed when executing a collection of tasks.
 */
class PIThreadPoolExecutor {
public:
	explicit PIThreadPoolExecutor(size_t corePoolSize);

	virtual ~PIThreadPoolExecutor();

	/**
	 * @brief Submits a Runnable task for execution and returns a Future representing that task. The Future's get method
	 * will return null upon successful completion.
	 *
	 * @tparam FunctionType - custom type of function with operator() and return type
	 * @tparam R - derived from FunctionType return type
	 *
	 * @param callable - the task to submit
	 * @return a future representing pending completion of the task
	 */
	std::future<R> submit(FunctionType&& callable);

	/**
	 * @brief Executes the given task sometime in the future. The task execute in an existing pooled thread. If the task
	 * cannot be submitted for execution, either because this executor has been shutdown or because its capacity has been
	 * reached.
	 *
	 * @tparam FunctionType - custom type of function with operator() and return type
	 *
	 * @param runnable not empty function for thread pool execution
	 */
	void execute(FunctionType&& runnable);

	/**
	 * @brief Initiates an orderly shutdown in which previously submitted tasks are executed, but no new tasks will be
	 * accepted. Invocation has no additional effect if already shut down. This method does not wait for previously
	 * submitted tasks to complete execution. Use awaitTermination to do that.
	 */
	void shutdown();

	void shutdownNow();

	bool isShutdown() const;

	bool awaitTermination(int timeoutMs);
};
#endif //DOXYGEN

#endif //PIEXECUTOR_H
