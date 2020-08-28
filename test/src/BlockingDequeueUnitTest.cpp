#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testutil.h"
#include "piblockingdequeue.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::Matcher;
using ::testing::Expectation;
using ::testing::Sequence;
using ::testing::NiceMock;

class MockConditionVar {
public:
    bool isWaitCalled = false;
    bool isWaitForCalled = false;
    bool isTrueCondition = false;
    int timeout = -1;

	MOCK_METHOD1(wait, void(std::mutex&));
	MOCK_METHOD2(wait, void(std::mutex&, const std::function<bool()>&));
	MOCK_METHOD2(wait_for, bool(std::mutex&, int));
	MOCK_METHOD3(wait_for, bool(std::mutex&, int, const std::function<bool()>&));
	MOCK_METHOD0(notify_one, void());
};

struct QueueElement {
	bool is_empty;
	int value;
	int copy_count;

	QueueElement(): is_empty(true), value(0), copy_count(0) { }
	explicit QueueElement(int value): is_empty(false), value(value), copy_count(0) { }

	QueueElement(const QueueElement& other) {
		this->is_empty = other.is_empty;
		this->value = other.value;
		this->copy_count = 0;
		const_cast<int&>(other.copy_count)++;
	}
	QueueElement(QueueElement&& other) noexcept : QueueElement() {
		std::swap(is_empty, other.is_empty);
		std::swap(value, other.value);
		std::swap(copy_count, other.copy_count);
	}

	bool operator==(const QueueElement &rhs) const {
		return is_empty == rhs.is_empty &&
			   value == rhs.value;
	}

	bool operator!=(const QueueElement &rhs) const {
		return !(rhs == *this);
	}

	friend std::ostream& operator<<(std::ostream& os, const QueueElement& el) {
		return os << "{ is_empty:" << el.is_empty << ", value:" << el.value << ", copy_count:" << el.copy_count << " }";
	}
};

template<typename T>
class MockDequeBase {
public:
	MOCK_METHOD1_T(push_back_rval, void(T));
	MOCK_METHOD1_T(push_back, void(const T&));
	MOCK_METHOD0(size, size_t());
	MOCK_METHOD0_T(front, T());
	MOCK_METHOD0(pop_front, void());

	void push_back(T&& t) {
		push_back_rval(t);
	}
};

template<typename T>
class MockDeque: public NiceMock<MockDequeBase<T>> {};

class PIBlockingDequeuePrepare: public PIBlockingDequeue<QueueElement, MockDeque, NiceMock<MockConditionVar>> {
public:
	typedef PIBlockingDequeue<QueueElement, MockDeque, NiceMock<MockConditionVar>> SuperClass;

	explicit PIBlockingDequeuePrepare(size_t capacity = SIZE_MAX): SuperClass(capacity) { }

	template<typename Iterable,
	         typename std::enable_if<!std::is_arithmetic<Iterable>::value, int>::type = 0>
	explicit PIBlockingDequeuePrepare(const Iterable& other): SuperClass(other) { }

	MockConditionVar* getCondVarAdd() { return this->cond_var_add; }
	MockConditionVar* getCondVarRem() { return this->cond_var_rem; }
	MockDeque<QueueElement>& getQueue() { return this->data_queue; }
	size_t getMaxSize() { return max_size; }
};

class BlockingDequeueUnitTest: public ::testing::Test {
public:
	int timeout = 100;
	size_t capacity;
	PIBlockingDequeuePrepare dequeue;
	QueueElement element;

	BlockingDequeueUnitTest(): capacity(1), dequeue(capacity), element(11) {}

	void offer2_is_wait_predicate(bool isCapacityReach);
	void put_is_wait_predicate(bool isCapacityReach);
	void take_is_wait_predicate(bool isEmpty);
};

TEST_F(BlockingDequeueUnitTest, construct_default_is_max_size_eq_size_max) {
	PIBlockingDequeuePrepare dequeue;
	ASSERT_EQ(dequeue.getMaxSize(), SIZE_MAX);
}

TEST_F(BlockingDequeueUnitTest, construct_from_constant_is_max_size_eq_capacity) {
	PIBlockingDequeuePrepare dequeue(2);
	ASSERT_EQ(dequeue.getMaxSize(), 2);
}

TEST_F(BlockingDequeueUnitTest, construct_from_capacity_is_max_size_eq_capacity) {
	ASSERT_EQ(dequeue.getMaxSize(), capacity);
}

TEST_F(BlockingDequeueUnitTest, construct_from_iterable) {
	std::vector<QueueElement> iterable;
	iterable.emplace_back(11);
	iterable.emplace_back(22);
	PIBlockingDequeuePrepare dequeue(iterable);
}

void BlockingDequeueUnitTest::put_is_wait_predicate(bool isCapacityReach) {
	std::function<bool()> conditionVarPredicate;
	EXPECT_CALL(*dequeue.getCondVarRem(), wait(_, _))
			.WillOnce([&](std::mutex& m, const std::function<bool()>& predicate){ conditionVarPredicate = predicate; });
	dequeue.put(element);

	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(isCapacityReach ? capacity : capacity - 1));
	ASSERT_EQ(conditionVarPredicate(), !isCapacityReach);
}

TEST_F(BlockingDequeueUnitTest, put_is_wait_predicate_true) {
	put_is_wait_predicate(false);
}

TEST_F(BlockingDequeueUnitTest, put_is_wait_predicate_false_when_capacity_reach) {
	put_is_wait_predicate(true);
}

TEST_F(BlockingDequeueUnitTest, put_is_insert_by_copy) {
	EXPECT_CALL(dequeue.getQueue(), push_back( Eq(element) ))
			.WillOnce(Return());
	dequeue.put(element);
}

TEST_F(BlockingDequeueUnitTest, put_is_insert_by_move) {
	QueueElement copyElement = element;
	EXPECT_CALL(dequeue.getQueue(), push_back_rval( Eq(element) ))
			.WillOnce(Return());
	dequeue.put(std::move(copyElement));
}

TEST_F(BlockingDequeueUnitTest, put_is_notify_about_insert) {
	EXPECT_CALL(*dequeue.getCondVarAdd(), notify_one)
			.WillOnce(Return());
	dequeue.put(element);
}

TEST_F(BlockingDequeueUnitTest, offer1_is_insert_by_copy) {
	EXPECT_CALL(dequeue.getQueue(), push_back( Eq(element) ))
			.WillOnce(Return());
	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(capacity - 1));
	dequeue.offer(element);
}

TEST_F(BlockingDequeueUnitTest, offer1_is_insert_by_move) {
	QueueElement copyElement = element;
	EXPECT_CALL(dequeue.getQueue(), push_back_rval( Eq(element) ))
			.WillOnce(Return());
	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(capacity - 1));
	dequeue.offer(std::move(copyElement));
}

TEST_F(BlockingDequeueUnitTest, offer1_is_not_insert_when_capacity_reach) {
	EXPECT_CALL(dequeue.getQueue(), push_back(_))
			.Times(0);
	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(capacity));
	dequeue.offer(element);
}

TEST_F(BlockingDequeueUnitTest, offer1_is_true_when_insert) {
	ON_CALL(dequeue.getQueue(), push_back(_))
			.WillByDefault(Return());
	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(capacity - 1));
	ASSERT_TRUE(dequeue.offer(element));
}

TEST_F(BlockingDequeueUnitTest, offer1_is_false_when_capacity_reach) {
	ON_CALL(dequeue.getQueue(), push_back(_))
			.WillByDefault(Return());
	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(capacity));
	ASSERT_FALSE(dequeue.offer(element));
}

TEST_F(BlockingDequeueUnitTest, offer1_is_notify_about_insert) {
	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(capacity - 1));
	EXPECT_CALL(*dequeue.getCondVarAdd(), notify_one)
			.WillOnce(Return());
	dequeue.offer(element);
}

TEST_F(BlockingDequeueUnitTest, offer1_is_not_notify_about_insert_when_capacity_reach) {
	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(capacity));
	EXPECT_CALL(*dequeue.getCondVarAdd(), notify_one)
			.Times(0);
	dequeue.offer(element);
}

void BlockingDequeueUnitTest::offer2_is_wait_predicate(bool isCapacityReach) {
	std::function<bool()> conditionVarPredicate;
	EXPECT_CALL(*dequeue.getCondVarRem(), wait_for(_, Eq(timeout), _))
			.WillOnce([&](std::mutex& m, int timeout_, const std::function<bool()>& predicate) {
				conditionVarPredicate = predicate;
				return isCapacityReach;
			});
	dequeue.offer(element, timeout);

	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(isCapacityReach ? capacity : capacity - 1));
	ASSERT_EQ(conditionVarPredicate(), !isCapacityReach);
}

TEST_F(BlockingDequeueUnitTest, offer2_is_wait_predicate_true) {
	offer2_is_wait_predicate(false);
}

TEST_F(BlockingDequeueUnitTest, offer2_is_wait_predicate_false_when_capacity_reach) {
	offer2_is_wait_predicate(true);
}

TEST_F(BlockingDequeueUnitTest, offer2_is_insert_by_copy) {
	EXPECT_CALL(*dequeue.getCondVarRem(), wait_for(_, Eq(timeout), _))
			.WillOnce(Return(true));
	EXPECT_CALL(dequeue.getQueue(), push_back( Eq(element) ))
			.WillOnce(Return());
	dequeue.offer(element, timeout);
}

TEST_F(BlockingDequeueUnitTest, offer2_is_insert_by_move) {
	QueueElement copyElement = element;
	EXPECT_CALL(*dequeue.getCondVarRem(), wait_for(_, Eq(timeout), _))
			.WillOnce(Return(true));
	EXPECT_CALL(dequeue.getQueue(), push_back_rval( Eq(element) ))
			.WillOnce(Return());
	dequeue.offer(std::move(copyElement), timeout);
}

TEST_F(BlockingDequeueUnitTest, offer2_is_not_insert_when_timeout) {
	EXPECT_CALL(*dequeue.getCondVarRem(), wait_for(_, Eq(timeout), _))
			.WillOnce(Return(false));
	EXPECT_CALL(dequeue.getQueue(), push_back(_))
			.Times(0);
	dequeue.offer(element, timeout);
}

TEST_F(BlockingDequeueUnitTest, offer2_is_true_when_insert) {
	ON_CALL(*dequeue.getCondVarRem(), wait_for(_, _, _))
			.WillByDefault(Return(true));
	ASSERT_TRUE(dequeue.offer(element, timeout));
}

TEST_F(BlockingDequeueUnitTest, offer2_is_false_when_timeout) {
	ON_CALL(*dequeue.getCondVarRem(), wait_for(_, _, _))
			.WillByDefault(Return(false));
	ASSERT_FALSE(dequeue.offer(element, timeout));
}

TEST_F(BlockingDequeueUnitTest, offer2_is_notify_about_insert) {
	ON_CALL(*dequeue.getCondVarRem(), wait_for(_, _, _))
			.WillByDefault(Return(true));
	EXPECT_CALL(*dequeue.getCondVarAdd(), notify_one)
			.WillOnce(Return());
	dequeue.offer(element, timeout);
}

TEST_F(BlockingDequeueUnitTest, offer2_is_not_notify_about_insert_when_timeout) {
	ON_CALL(*dequeue.getCondVarRem(), wait_for(_, _, _))
			.WillByDefault(Return(false));
	EXPECT_CALL(*dequeue.getCondVarAdd(), notify_one)
			.Times(0);
	dequeue.offer(element, timeout);
}

void BlockingDequeueUnitTest::take_is_wait_predicate(bool isEmpty) {
	std::function<bool()> conditionVarPredicate;
	EXPECT_CALL(*dequeue.getCondVarAdd(), wait(_, _))
			.WillOnce([&](std::mutex& m, const std::function<bool()>& predicate) { conditionVarPredicate = predicate; });
	dequeue.take();

	ON_CALL(dequeue.getQueue(), size)
			.WillByDefault(Return(isEmpty ? 0 : 1));
	ASSERT_EQ(conditionVarPredicate(), !isEmpty);
}

TEST_F(BlockingDequeueUnitTest, take_is_wait_predicate_true) {
	take_is_wait_predicate(false);
}

TEST_F(BlockingDequeueUnitTest, take_is_wait_predicate_false_when_queue_empty) {
	take_is_wait_predicate(true);
}

TEST_F(BlockingDequeueUnitTest, take_is_get_and_remove) {
	Expectation front = EXPECT_CALL(dequeue.getQueue(), front())
			.WillOnce(Return(element));
	EXPECT_CALL(dequeue.getQueue(), pop_front())
			.After(front)
			.WillOnce(Return());

	QueueElement takenElement = dequeue.take();
	ASSERT_EQ(element, takenElement);
}

TEST_F(BlockingDequeueUnitTest, take_is_notify_about_remove) {
	EXPECT_CALL(*dequeue.getCondVarRem(), notify_one)
			.WillOnce(Return());
	dequeue.take();
}

/*
// TODO change take_is_block_when_empty to prevent segfault
TEST(DISABLED_BlockingDequeueUnitTest, take_is_block_when_empty) {
    size_t capacity = 1;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    // May cause segfault because take front of empty queue
    dequeue.take();
    EXPECT_TRUE(dequeue.getCondVarAdd()->isWaitCalled);
    ASSERT_FALSE(dequeue.getCondVarAdd()->isTrueCondition);
}

TEST(BlockingDequeueUnitTest, take_is_not_block_when_not_empty) {
    size_t capacity = 1;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    dequeue.offer(111);
    dequeue.take();

    EXPECT_TRUE(dequeue.getCondVarAdd()->isWaitCalled);
    ASSERT_TRUE(dequeue.getCondVarAdd()->isTrueCondition);
}

TEST(BlockingDequeueUnitTest, take_is_value_eq_to_offer_value) {
    size_t capacity = 1;
	PIBlockingDequeuePrepare<int> dequeue(capacity);

    dequeue.offer(111);
    ASSERT_EQ(dequeue.take(), 111);
}

TEST(BlockingDequeueUnitTest, take_is_last) {
    size_t capacity = 10;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    EXPECT_TRUE(dequeue.offer(111));
    EXPECT_TRUE(dequeue.offer(222));
    ASSERT_EQ(dequeue.take(), 111);
    ASSERT_EQ(dequeue.take(), 222);
}

TEST(BlockingDequeueUnitTest, poll_is_not_block_when_empty) {
	size_t capacity = 1;
	bool isOk;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
	dequeue.poll(111, &isOk);
	EXPECT_FALSE(dequeue.getCondVarAdd()->isWaitForCalled);
}

TEST(BlockingDequeueUnitTest, poll_is_default_value_when_empty) {
	size_t capacity = 1;
	bool isOk;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
	ASSERT_EQ(dequeue.poll(111, &isOk), 111);
}

TEST(BlockingDequeueUnitTest, poll_is_offer_value_when_not_empty) {
	size_t capacity = 1;
	bool isOk;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
	dequeue.offer(111);
	ASSERT_EQ(dequeue.poll(-1, &isOk), 111);
}

TEST(BlockingDequeueUnitTest, poll_timeouted_is_block_when_empty) {
    size_t capacity = 1;
    int timeout = 11;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    dequeue.poll(timeout, 111);
    EXPECT_TRUE(dequeue.getCondVarAdd()->isWaitForCalled);
    EXPECT_EQ(timeout, dequeue.getCondVarAdd()->timeout);
    ASSERT_FALSE(dequeue.getCondVarAdd()->isTrueCondition);
}

TEST(BlockingDequeueUnitTest, poll_timeouted_is_default_value_when_empty) {
    size_t capacity = 1;
    int timeout = 11;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    ASSERT_EQ(dequeue.poll(timeout, 111), 111);
}

TEST(BlockingDequeueUnitTest, poll_timeouted_is_not_block_when_not_empty) {
    size_t capacity = 1;
    int timeout = 11;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    dequeue.offer(111);
    dequeue.poll(timeout, -1);

    EXPECT_TRUE(dequeue.getCondVarAdd()->isWaitForCalled);
    ASSERT_TRUE(dequeue.getCondVarAdd()->isTrueCondition);
}

TEST(BlockingDequeueUnitTest, poll_timeouted_is_offer_value_when_not_empty) {
    size_t capacity = 1;
    int timeout = 11;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    dequeue.offer(111);
    ASSERT_EQ(dequeue.poll(timeout, -1), 111);
}

TEST(BlockingDequeueUnitTest, poll_timeouted_is_last) {
    size_t capacity = 10;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    dequeue.offer(111);
    dequeue.offer(222);
    ASSERT_EQ(dequeue.poll(10, -1), 111);
    ASSERT_EQ(dequeue.poll(10, -1), 222);
}

TEST(BlockingDequeueUnitTest, capacity_is_eq_constructor_capacity) {
    size_t capacity = 10;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    ASSERT_EQ(dequeue.capacity(), capacity);
}

TEST(BlockingDequeueUnitTest, remainingCapacity_is_dif_of_capacity_and_size) {
    size_t capacity = 2;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    ASSERT_EQ(dequeue.remainingCapacity(), capacity);
    dequeue.offer(111);
    ASSERT_EQ(dequeue.remainingCapacity(), capacity - 1);
}

TEST(BlockingDequeueUnitTest, remainingCapacity_is_zero_when_capacity_reach) {
    size_t capacity = 1;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    dequeue.offer(111);
    dequeue.offer(111);
    ASSERT_EQ(dequeue.remainingCapacity(), 0);
}

TEST(BlockingDequeueUnitTest, size_is_eq_to_num_of_elements) {
    size_t capacity = 1;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    ASSERT_EQ(dequeue.size(), 0);
    dequeue.offer(111);
    ASSERT_EQ(dequeue.size(), 1);
}

TEST(BlockingDequeueUnitTest, size_is_eq_to_capacity_when_capacity_reach) {
    size_t capacity = 1;
	PIBlockingDequeuePrepare<int> dequeue(capacity);
    dequeue.offer(111);
    dequeue.offer(111);
    ASSERT_EQ(dequeue.size(), capacity);
}

TEST(BlockingDequeueUnitTest, drainTo_is_elements_moved) {
    size_t capacity = 10;
	std::deque<int> refDeque;
    for (size_t i = 0; i < capacity / 2; ++i) refDeque.push_back(i * 10);
	PIBlockingDequeuePrepare<int> blockingDequeue(refDeque);
	PIBlockingDequeuePrepare<int>::QueueType deque;
    blockingDequeue.drainTo(deque);
    ASSERT_EQ(blockingDequeue.size(), 0);
    // FIXME
//    ASSERT_TRUE(deque == refDeque);
}

TEST(BlockingDequeueUnitTest, drainTo_is_ret_eq_to_size_when_all_moved) {
    size_t capacity = 10;
	std::deque<int> refDeque;
    for (size_t i = 0; i < capacity / 2; ++i) refDeque.push_back(i * 10);
	PIBlockingDequeuePrepare<int> blockingDequeue(refDeque);
	PIBlockingDequeuePrepare<int>::QueueType deque;
    ASSERT_EQ(blockingDequeue.drainTo(deque), refDeque.size());
}

TEST(BlockingDequeueUnitTest, drainTo_is_ret_eq_to_maxCount) {
    size_t capacity = 10;
	std::deque<int> refDeque;
    for (size_t i = 0; i < capacity / 2; ++i) refDeque.push_back(i * 10);
	PIBlockingDequeuePrepare<int> blockingDequeue(refDeque);
	PIBlockingDequeuePrepare<int>::QueueType deque;
    ASSERT_EQ(blockingDequeue.drainTo(deque, refDeque.size() - 1), refDeque.size() - 1);
}
*/
