#include "gtest/gtest.h"
#include "testutil.h"
#include "executor.h"

using namespace std;
using namespace chrono;

TEST(ExcutorIntegrationTest, execute_is_runnable_invoke) {
    std::mutex m;
    int invokedRunnables = 0;
    ThreadPoolExecutor executorService(1);
	executorService.execute([&]() {
        m.lock();
        invokedRunnables++;
        m.unlock();
    });
	this_thread::sleep_for(milliseconds(WAIT_THREAD_TIME_MS));
	m.lock();
    ASSERT_EQ(invokedRunnables, 1);
    m.unlock();
}

TEST(ExcutorIntegrationTest, execute_is_not_execute_after_shutdown) {
    volatile bool isRunnableInvoke = false;
    ThreadPoolExecutor executorService(1);
    executorService.shutdown();
    executorService.execute([&]() {
        isRunnableInvoke = true;
    });
	this_thread::sleep_for(milliseconds(WAIT_THREAD_TIME_MS));
    ASSERT_FALSE(isRunnableInvoke);
}

TEST(ExcutorIntegrationTest, execute_is_execute_before_shutdown) {
	volatile bool isRunnableInvoke = false;
    ThreadPoolExecutor executorService(1);
    executorService.execute([&]() {
		this_thread::sleep_for(milliseconds(WAIT_THREAD_TIME_MS));
        isRunnableInvoke = true;
    });
    executorService.shutdown();
	this_thread::sleep_for(milliseconds(2 * WAIT_THREAD_TIME_MS));
    ASSERT_TRUE(isRunnableInvoke);
}

// FIXME
TEST(DISABLED_ExcutorIntegrationTest, execute_is_awaitTermination_wait) {
    ThreadPoolExecutor executorService(1);
    executorService.execute([&]() {
		this_thread::sleep_for(milliseconds(2 * WAIT_THREAD_TIME_MS));
    });
    executorService.shutdown();
	auto start_time = high_resolution_clock::now();
    ASSERT_TRUE(executorService.awaitTermination(3 * WAIT_THREAD_TIME_MS));
    double wait_time = static_cast<double>(duration_cast<microseconds>(high_resolution_clock::now() - start_time).count()) / 1000.;
    ASSERT_GE(wait_time, WAIT_THREAD_TIME_MS);
    ASSERT_LE(wait_time, 4 * WAIT_THREAD_TIME_MS);
}
