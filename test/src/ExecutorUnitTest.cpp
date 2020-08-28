#include <utility>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testutil.h"
#include "piexecutor.h"

using ::testing::_;
using ::testing::SetArgReferee;
using ::testing::DoAll;
using ::testing::DeleteArg;
using ::testing::Return;
using ::testing::ByMove;
using ::testing::AtLeast;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Pointee;
using ::testing::IsNull;
using ::testing::NiceMock;

typedef std::function<void()> VoidFunc;

namespace std {
    inline bool operator ==(const VoidFunc& s, const VoidFunc& v) {
        // TODO VoidFunc operator ==
        return true;
    }
}

class MockThread {
public:
	bool is_executed;
	VoidFunc runnnable;

	explicit MockThread(VoidFunc  runnnable) : is_executed(true), runnnable(std::move(runnnable)) { }

//    MOCK_METHOD0(stop, void());
//    MOCK_METHOD1(waitForStart, bool(int timeout_msecs));
    MOCK_METHOD0(join, void());
};

class MockDeque : public PIBlockingDequeue<FunctionWrapper> {
public:
    MOCK_METHOD1(offer, bool(const FunctionWrapper&));
    MOCK_METHOD0(take, FunctionWrapper());
    MOCK_METHOD1(poll, FunctionWrapper(int));
    MOCK_METHOD0(capacity, size_t());
    MOCK_METHOD0(remainingCapacity, size_t());
};

typedef PIThreadPoolExecutorTemplate<NiceMock<MockThread>, MockDeque> PIThreadPoolExecutorMoc_t;

class PIThreadPoolExecutorMoc : public PIThreadPoolExecutorMoc_t {
public:
	explicit PIThreadPoolExecutorMoc(size_t corePoolSize) : PIThreadPoolExecutorMoc_t(corePoolSize) { }

	template<typename Function>
	explicit PIThreadPoolExecutorMoc(size_t corePoolSize, Function onBeforeStart) : PIThreadPoolExecutorMoc_t(corePoolSize, onBeforeStart) { }

	std::vector<testing::NiceMock<MockThread>*>* getThreadPool() { return &threadPool; }
	bool isShutdown() { return thread_command_ != thread_command::run; }
	MockDeque* getTaskQueue() { return &taskQueue; }
};

TEST(ExecutorUnitTest, is_corePool_created) {
	PIThreadPoolExecutorMoc executor(THREAD_COUNT);
    ASSERT_EQ(THREAD_COUNT, executor.getThreadPool()->size());
}

TEST(ExecutorUnitTest, is_corePool_started) {
	PIThreadPoolExecutorMoc executor(THREAD_COUNT);
	for (auto* thread : *executor.getThreadPool()) ASSERT_TRUE(thread->is_executed);
}

TEST(ExecutorUnitTest, submit_is_added_to_taskQueue) {
	VoidFunc voidFunc = [](){};
	PIThreadPoolExecutorMoc executor(THREAD_COUNT);
	// TODO add check of offered
	EXPECT_CALL(*executor.getTaskQueue(), offer)
			.WillOnce(Return(true));
	executor.submit(voidFunc);
}

TEST(ExecutorUnitTest, submit_is_return_valid_future) {
	VoidFunc voidFunc = [](){};
	PIThreadPoolExecutorMoc executor(THREAD_COUNT);
	// TODO add check of offered
	EXPECT_CALL(*executor.getTaskQueue(), offer)
			.WillOnce(Return(true));
	auto future = executor.submit(voidFunc);
	EXPECT_TRUE(future.valid());
}

TEST(ExecutorUnitTest, execute_is_added_to_taskQueue) {
    VoidFunc voidFunc = [](){};
	PIThreadPoolExecutorMoc executor(THREAD_COUNT);
	// TODO add check of offered
	EXPECT_CALL(*executor.getTaskQueue(), offer)
			.WillOnce(Return(true));
    executor.execute(voidFunc);
}

// TODO fix
TEST(DISABLED_ExecutorUnitTest, is_corePool_execute_queue_elements) {
	bool is_executed = false;
	PIThreadPoolExecutorMoc executor(1);
	EXPECT_EQ(executor.getThreadPool()->size(), 1);
		EXPECT_CALL(*executor.getTaskQueue(), poll(Ge(0)))
			.WillOnce([&is_executed](int){
				return FunctionWrapper([&is_executed](){ is_executed = true; });
			});
	executor.getThreadPool()->at(0)->runnnable();
	ASSERT_TRUE(is_executed);
}

// FIXME
TEST(DISABLED_ExecutorUnitTest, shutdown_is_stop_threads) {
	// Exclude stop calls when executor deleting
	auto* executor = new PIThreadPoolExecutorMoc(THREAD_COUNT, [](MockThread* thread){
		testing::Mock::AllowLeak(thread);
		EXPECT_CALL(*thread, join())
				.WillOnce(Return());
	});
	testing::Mock::AllowLeak(executor);
	testing::Mock::AllowLeak(executor->getTaskQueue());

	EXPECT_CALL(*executor->getTaskQueue(), poll(Ge(0)))
		.WillRepeatedly([](int){ return FunctionWrapper(); });
    executor->shutdown();
    for (auto* thread : *executor->getThreadPool()) thread->runnnable();
}
