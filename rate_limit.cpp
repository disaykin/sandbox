#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <deque>
#include <array>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <cassert>

using namespace std;
using namespace std::literals::chrono_literals;

class RateLimit {
private:
	// Временной слот, в который попал последний запрос
	uint64_t mLastUpdateSlice;
	// Максимальный разрешенный RPS
	int const mMaxRPS;

	// Счетчики количества запросов по слотам (циклический буфер)
	// Суммарное количество запросов во всех слотах не должно превышать максимальный RPS
	// Для учета временных погрешностей храним на один слот больше
	static const int N_TIME_SLICES = 1000;
	static const int HISTORY_LENGTH = N_TIME_SLICES + 1;
	std::array<int, HISTORY_LENGTH> mRequestCounters;
	// Текущее количество запросов во всех слотах
	int mCurrentRPS;

	std::mutex mLock;

private:
	static uint64_t get_time_slices_since_epoch() {
		auto now = chrono::steady_clock::now();
		auto seconds = chrono::duration<double>(now.time_since_epoch()).count();
		return static_cast<uint64_t>(seconds * N_TIME_SLICES);
	}

public:
	explicit RateLimit(int maxRPS) :
		mLastUpdateSlice(get_time_slices_since_epoch()),
		mMaxRPS(maxRPS),
		mRequestCounters(),
		mCurrentRPS(),
		mLock()
	{}

	// Возвращает true, если удалось получить разрешение на выполнение запроса.
	bool tryAquireRequestTicket() {
		std::lock_guard<std::mutex> guard(mLock);

		// Обновляем счетчики запросов на основе прошедшего с предыдущего вызова времени
		uint64_t current_slice = get_time_slices_since_epoch();
		int to_index = current_slice % HISTORY_LENGTH;
		if (current_slice == mLastUpdateSlice) {
			// Попали в тот же временной слот, что и предыдущий запрос
		} else if (current_slice - mLastUpdateSlice < HISTORY_LENGTH) {
			// Прошло менее секунды с предыдущего запроса
			// Обнуляем счетчики для пропущенных с последнего запроса слотов (не было запросов)
			int from_index = mLastUpdateSlice % HISTORY_LENGTH;
			for (int i = (from_index + 1) % HISTORY_LENGTH; i != (to_index + 1) % HISTORY_LENGTH; i = (i + 1) % HISTORY_LENGTH) {
				mCurrentRPS -= mRequestCounters[i];
				mRequestCounters[i] = 0;
			}
		} else {
			// С предыдущего запроса прошло времени секунда и более
			// Обнуляем все счетчики
			mRequestCounters.fill(0);
			mCurrentRPS = 0;
		}
		mLastUpdateSlice = current_slice;
		if (mCurrentRPS < mMaxRPS) {
			// Есть возможность выполнить запрос
			++mCurrentRPS;
			++mRequestCounters[to_index];
			return true;
		}
		return false;
	}
};

static void unittest1_zerolimit() {
	RateLimit rl{0};

	for (int i = 0; i < 10; ++i) {
		assert(!rl.tryAquireRequestTicket());
	}
}

static void unittest2_onelimit() {
	RateLimit rl{1};

	auto now = chrono::steady_clock::now();
	assert(rl.tryAquireRequestTicket());
	for (int i = 0; i < 10; ++i) {
		assert(!rl.tryAquireRequestTicket());
	}

	for (int i = 1; i < 10; ++i) {
		std::this_thread::sleep_for(100ms);
		assert(!rl.tryAquireRequestTicket());
	}

	std::this_thread::sleep_until(now + 1100ms);
	assert(rl.tryAquireRequestTicket());
}

static void unittest3_tenlimit() {
	RateLimit rl{10};

	auto now = chrono::steady_clock::now();
	for (int i = 0; i < 10; ++i) {
		assert(rl.tryAquireRequestTicket());
	}
	assert(!rl.tryAquireRequestTicket());

	for (int i = 1; i < 10; ++i) {
		std::this_thread::sleep_for(100ms);
		assert(!rl.tryAquireRequestTicket());
	}

	std::this_thread::sleep_until(now + 1100ms);
	for (int i = 0; i < 10; ++i) {
		assert(rl.tryAquireRequestTicket());
	}
	assert(!rl.tryAquireRequestTicket());
}

class API {
private:
	RateLimit& mRateLimit;

	chrono::steady_clock::time_point mStartDate;
	std::deque<int> mSuccessfullCalls;
	std::deque<int> mRateLimitedCalls;
	std::mutex mLock;

public:
	explicit API(RateLimit& rateLimit) :
		mRateLimit(rateLimit),
		mStartDate(chrono::steady_clock::now()),
		mSuccessfullCalls(),
		mRateLimitedCalls(),
		mLock()
	{}

	void call() {
		bool called = mRateLimit.tryAquireRequestTicket();

		auto now = chrono::steady_clock::now();
		size_t time_index = static_cast<size_t>(chrono::duration<double>(now - mStartDate).count() * 10);

		{
			std::lock_guard<std::mutex> guard(mLock);
			if (time_index >= mSuccessfullCalls.size()) mSuccessfullCalls.resize(time_index + 1);
			if (time_index >= mRateLimitedCalls.size()) mRateLimitedCalls.resize(time_index + 1);

			if (called) {
				++mSuccessfullCalls[time_index];
			} else {
				++mRateLimitedCalls[time_index];
			}
		}
	}

	void printStat() {
		std::lock_guard<std::mutex> guard(mLock);
		std::cout << "  TimeSlice   | Successfull calls | Rate limited calls | RPS for last second" << std::endl;
		for (size_t i = 0; i != mSuccessfullCalls.size(); ++i) {
			std::cout << setfill('0') << setw(5) << 100 * (i) << "-" << setw(5) << 100 * (i + 1) << "ms |     "
			          << setw(10) << mSuccessfullCalls[i] << "    |     "
			          << setw(10) << mRateLimitedCalls[i] << "     |     "
			          << std::accumulate(mSuccessfullCalls.begin() + std::max<size_t>(0, i - 9),
			                             mSuccessfullCalls.begin() + i + 1, 0) << std::endl;
		}
	}
};

static void thread_func(API& api) {
	for (int i = 0; i < 1000; ++i) {
		api.call();
		std::this_thread::sleep_for(chrono::milliseconds(std::rand() % 50));
	}
}

int main() {
	unittest1_zerolimit();
	unittest2_onelimit();
	unittest3_tenlimit();

	RateLimit rateLimit{5000};

	API api(rateLimit);

	std::vector<std::thread> workers;
	for (size_t n = 0; n < 500; ++n) {
		workers.push_back(std::thread(thread_func, std::ref(api)));
	}

	for (auto& w : workers) w.join();

	api.printStat();
}
