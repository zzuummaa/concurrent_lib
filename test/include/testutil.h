#ifndef AWRCANFLASHER_TESTUTIL_H
#define AWRCANFLASHER_TESTUTIL_H

#include <future>
#include <atomic>

template<typename T>
void print_type_info() {
	std::cout << typeid(T).name() << " is a "
			  << (std::is_const<typename std::remove_reference<T>::type>::value ? "const " : "")
			  << (std::is_lvalue_reference<T>::value ? "lvalue" : "rvalue")
			  << " reference" << std::endl;
}

/**
 * Minimum wait thread start, switch context or another interthread communication action time. Increase it if tests
 * write "Start thread timeout reach!" message. You can reduce it if you want increase test performance.
 */
const int WAIT_THREAD_TIME_MS = 30;

const int THREAD_COUNT = 2;

class TestUtil {
public:
    double threadStartTime;
    std::atomic_bool isRunning;
    std::function<void()> adapterFunctionDefault;

	TestUtil() : isRunning(false) {}

	bool createThread(const std::function<void()>&  fun = nullptr) {
    	std::function<void()> actualFun = fun == nullptr ? adapterFunctionDefault : fun;

		std::promise<void> start_promise;
		std::future<void> start_future = start_promise.get_future();
        std::thread thread([this, &start_promise, actualFun](){
			isRunning = true;
			start_promise.set_value();
        	actualFun();
        });
        thread.detach();

        auto status = start_future.wait_for(std::chrono::milliseconds(WAIT_THREAD_TIME_MS));
        if (status == std::future_status::timeout) {
			std::cout << "Start thread timeout reach!" << std::endl;
        }
        start_future.get();
        return status == std::future_status::ready;
    }

//    bool waitThread(PIThread* thread_, bool runningStatus = true) {
//        PITimeMeasurer measurer;
//        bool isTimeout = !thread_->waitForStart(WAIT_THREAD_TIME_MS);
//        while (!isRunning) {
//            isTimeout = WAIT_THREAD_TIME_MS <= measurer.elapsed_m();
//            if (isTimeout) break;
//            piUSleep(100);
//        }
//
//        threadStartTime = measurer.elapsed_m();
//
//        if (isTimeout) piCout << "Start thread timeout reach!";
//
//        if (threadStartTime > 1) {
//            piCout << "Start time" << threadStartTime << "ms";
//        } else if (threadStartTime > 0.001) {
//            piCout << "Start time" << threadStartTime * 1000 << "mcs";
//        } else {
//            piCout << "Start time" << threadStartTime * 1000 * 1000 << "ns";
//        }
//
//        return !isTimeout;
//    }
};

#endif //AWRCANFLASHER_TESTUTIL_H
