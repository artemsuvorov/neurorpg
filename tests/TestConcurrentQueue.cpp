#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <chrono>
#include <random>

#include "ConcurrentQueue.h"


using namespace Neuro::Core;


namespace Neuro::Tests {

	// Helper: custom type to track constructions/destructions
	struct Tracker
	{
		static std::atomic<int> constructed;
		static std::atomic<int> destructed;
		int id;
		Tracker() : id(0)
		{
			constructed++;
		}
		Tracker(int i) : id(i)
		{
			constructed++;
		}
		Tracker(const Tracker& other) : id(other.id)
		{
			constructed++;
		}
		Tracker(Tracker&& other) noexcept : id(other.id)
		{
			other.id = -1;
			constructed++;
		}
		~Tracker()
		{
			destructed++;
		}
		Tracker& operator=(const Tracker&) = default;
		Tracker& operator=(Tracker&&) = default;
	};
	std::atomic<int> Tracker::constructed{0};
	std::atomic<int> Tracker::destructed{0};


	// Helper: type that counts moves and copies
	struct MoveCounter
	{
		static std::atomic<int> moves;
		static std::atomic<int> copies;
		int value;
		MoveCounter() : value(0) {}
		explicit MoveCounter(int v) : value(v) {}
		MoveCounter(const MoveCounter& other) : value(other.value)
		{
			copies++;
		}
		MoveCounter(MoveCounter&& other) noexcept : value(other.value)
		{
			other.value = -1;
			moves++;
		}
		MoveCounter& operator=(const MoveCounter&) = default;
		MoveCounter& operator=(MoveCounter&&) = default;
	};
	std::atomic<int> MoveCounter::moves{0};
	std::atomic<int> MoveCounter::copies{0};


	// Test 1: basic single item
	static void test_basic()
	{
		ConcurrentQueue<int> q;
		q.Enqueue(42);
		int val = q.Dequeue();
		assert(val == 42);
		(void)val;
		std::cout << "test_basic passed\n";
	}


	// Test 2: multiple producers, all items accounted
	static void test_multiple_producers()
	{
		const int N = 1000;
		ConcurrentQueue<int> q;
		std::vector<std::thread> producers;
		for (int i = 0; i < N; ++i)
		{
			producers.emplace_back([&q, i] {
				q.Enqueue(i);
			});
		}
		for (auto& t : producers)
			t.join();

		std::vector<int> results;
		for (int i = 0; i < N; ++i)
		{
			results.push_back(q.Dequeue());
		}
		std::vector<int> count(N, 0);
		for (int v : results)
		{
			assert(v >= 0 && v < N);
			count[v]++;
		}
		for (int i = 0; i < N; ++i)
			assert(count[i] == 1);
		std::cout << "test_multiple_producers passed\n";
	}


	// Test 3: blocking consumer
	static void test_blocking()
	{
		ConcurrentQueue<int> q;
		std::thread consumer([&q] {
			int val = q.Dequeue();
			assert(val == 123);
			(void)val;
		});
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		q.Enqueue(123);
		consumer.join();
		std::cout << "test_blocking passed\n";
	}


	// Test 4: high volume stress test with many producers and items
	static void test_stress_volume()
	{
		const int num_producers = 8;
		const int items_per_producer = 50000;
		const int total = num_producers * items_per_producer;
		ConcurrentQueue<int> q;
		std::vector<std::thread> producers;
		std::atomic<int> produced{0};

		for (int i = 0; i < num_producers; ++i)
		{
			producers.emplace_back([&q, &produced, i, items_per_producer] {
				for (int j = 0; j < items_per_producer; ++j)
				{
					q.Enqueue(i * items_per_producer + j);
					produced++;
				}
			});
		}

		std::vector<int> results;
		results.reserve(total);
		std::thread consumer([&q, &results, total] {
			for (int i = 0; i < total; ++i)
			{
				results.push_back(q.Dequeue());
			}
		});

		for (auto& t : producers)
			t.join();
		consumer.join();

		assert(produced == total);
		assert(results.size() == total);
		std::vector<int> count(total, 0);
		for (int v : results)
		{
			assert(v >= 0 && v < total);
			count[v]++;
		}
		for (int i = 0; i < total; ++i)
			assert(count[i] == 1);
		std::cout << "test_stress_volume passed\n";
	}


	// Test 5: random delays and interleaving to catch race conditions
	static void test_random_delays()
	{
		const int num_producers = 10;
		const int items_per_producer = 200;
		const int total = num_producers * items_per_producer;

		ConcurrentQueue<int> q;
		std::vector<std::thread> producers;
		std::atomic<int> produced{0};

		std::random_device rd;
		for (int i = 0; i < num_producers; ++i)
		{
			producers.emplace_back([&q, &produced, i, items_per_producer, seed = rd()] {
				std::mt19937 rng(seed);
				std::uniform_int_distribution<int> delay_prob(0, 99);  // 1% chance to yield
				for (int j = 0; j < items_per_producer; ++j)
				{
					q.Enqueue(i * items_per_producer + j);
					produced++;
					if (delay_prob(rng) < 10)
					{  // 10% chance to yield
						std::this_thread::yield();
					}
				}
			});
		}

		std::vector<int> results;
		results.reserve(total);
		std::thread consumer([&q, &results, total, seed = rd()] {
			std::mt19937 rng(seed);
			std::uniform_int_distribution<int> delay_prob(0, 99);
			for (int i = 0; i < total; ++i)
			{
				results.push_back(q.Dequeue());
				if (delay_prob(rng) < 10)
				{
					std::this_thread::yield();
				}
			}
		});

		for (auto& t : producers)
			t.join();
		consumer.join();

		assert(produced == total);
		assert(results.size() == total);
		std::vector<int> count(total, 0);
		for (int v : results)
		{
			assert(v >= 0 && v < total);
			count[v]++;
		}
		for (int i = 0; i < total; ++i)
			assert(count[i] == 1);
		std::cout << "test_random_delays passed\n";
	}


	// Test 6: TryDequeue behavior
	static void test_try_dequeue()
	{
		ConcurrentQueue<int> q;
		int out = 0;
		assert(!q.TryDequeue(out));
		q.Enqueue(42);
		assert(q.TryDequeue(out));
		assert(out == 42);
		assert(!q.TryDequeue(out));
		(void)out;
		std::cout << "test_try_dequeue passed\n";
	}


	// Test 7: move-only type (with default constructor)
	static void test_move_only_type()
	{
		struct MoveOnly
		{
			int value;
			MoveOnly() : value(0) {}
			explicit MoveOnly(int v) : value(v) {}
			MoveOnly(const MoveOnly&) = delete;
			MoveOnly(MoveOnly&& other) noexcept : value(other.value)
			{
				other.value = -1;
			}
			MoveOnly& operator=(const MoveOnly&) = delete;
			MoveOnly& operator=(MoveOnly&& other) noexcept
			{
				value = other.value;
				other.value = -1;
				return *this;
			}
		};
		ConcurrentQueue<MoveOnly> q;
		q.Enqueue(MoveOnly(123));
		MoveOnly val = q.Dequeue();
		assert(val.value == 123);
		(void)val;
		std::cout << "test_move_only_type passed\n";
	}


	// Test 8: move-only type with tracking (ensure moves, not copies)
	static void test_move_only_tracking()
	{
		// Reset counters
		MoveCounter::moves = 0;
		MoveCounter::copies = 0;
		{
			ConcurrentQueue<MoveCounter> q;
			MoveCounter mc(42);
			q.Enqueue(std::move(mc));             // should move once
			MoveCounter retrieved = q.Dequeue();  // should move again (from slot to local)
		}
		// Enqueue moves the argument into the slot, Dequeue moves from slot to local.
		// There should be no copies.
		assert(MoveCounter::moves >= 2);  // at least two moves
		assert(MoveCounter::copies == 0);
		std::cout << "test_move_only_tracking passed (moves=" << MoveCounter::moves << ", copies=" << MoveCounter::copies
		          << ")\n";
	}


	// Test 9: destruction of queue with remaining items (should delete them)
	static void test_destruction_with_items()
	{
		Tracker::constructed = 0;
		Tracker::destructed = 0;
		{
			ConcurrentQueue<Tracker> q;
			for (int i = 0; i < 100; ++i)
			{
				q.Enqueue(Tracker(i));
			}
			// Dequeue only 50 items
			for (int i = 0; i < 50; ++i)
			{
				Tracker t = q.Dequeue();
				(void)t;
			}
			// The queue still holds 50 items plus the dummy. The destructor will delete them.
		}
		// All items (including the remaining 50 and the dummy) should be destroyed.
		// The total constructed count should equal the total destructed count.
		assert(Tracker::constructed == Tracker::destructed);
		std::cout << "test_destruction_with_items passed\n";
	}


	// Test 10: enqueue many items before any dequeue (semaphore count high)
	static void test_large_semaphore()
	{
		const int N = 100000;
		ConcurrentQueue<int> q;
		for (int i = 0; i < N; ++i)
		{
			q.Enqueue(i);
		}
		for (int i = 0; i < N; ++i)
		{
			int val = q.Dequeue();
			assert(val == i);
			(void)val;
		}
		std::cout << "test_large_semaphore passed\n";
	}


	// Test 11: TryDequeue after partial drain
	static void test_try_dequeue_after_drain()
	{
		ConcurrentQueue<int> q;
		for (int i = 0; i < 10; ++i)
			q.Enqueue(i);
		for (int i = 0; i < 5; ++i)
		{
			int out;
			assert(q.TryDequeue(out));
			(void)out;
		}
		// Still 5 items left
		int out2;
		assert(q.TryDequeue(out2));
		// Drain all
		for (int i = 0; i < 5; ++i)
			q.Dequeue();
		assert(!q.TryDequeue(out2));
		(void)out2;
		std::cout << "test_try_dequeue_after_drain passed\n";
	}


	// Test 12: multiple producers with heavy contention and one consumer that occasionally sleeps
	static void test_heavy_contention()
	{
		const int num_producers = 16;
		const int items_per_producer = 2000;  // 32,000 total
		const int total = num_producers * items_per_producer;

		ConcurrentQueue<int> q;
		std::vector<std::thread> producers;
		std::atomic<int> produced{0};

		std::random_device rd;
		for (int i = 0; i < num_producers; ++i)
		{
			producers.emplace_back([&q, &produced, i, items_per_producer, seed = rd()] {
				std::mt19937 rng(seed);
				std::uniform_int_distribution<int> delay_dist(0, 2);
				for (int j = 0; j < items_per_producer; ++j)
				{
					q.Enqueue(i * items_per_producer + j);
					produced++;
					if (delay_dist(rng) == 0)
					{
						std::this_thread::yield();  // give up CPU briefly
					}
				}
			});
		}

		std::atomic<int> consumed{0};
		std::thread consumer([&q, &consumed, total] {
			while (consumed.load() < total)
			{
				int val = q.Dequeue();  // blocks until an item is available
				consumed++;
				(void)val;
				if (consumed % 1000 == 0)
				{
					std::this_thread::yield();  // occasional yield to keep things fair
				}
			}
		});

		for (auto& t : producers)
			t.join();
		consumer.join();

		assert(produced == total);
		assert(consumed == total);
		std::cout << "test_heavy_contention passed\n";
	}


	// Test 13: Large number of items (pure performance stress)
	static void test_large_volume()
	{
		const int num_items = 500000;  // half a million
		ConcurrentQueue<int> q;
		for (int i = 0; i < num_items; ++i)
		{
			q.Enqueue(i);
		}
		for (int i = 0; i < num_items; ++i)
		{
			int val = q.Dequeue();
			assert(val == i);
			(void)val;
		}
		std::cout << "test_large_volume passed\n";
	}


	// Test 14: Non-trivial move type (std::vector)
	static void test_non_trivial_move_type()
	{
		using VectorType = std::vector<int>;
		ConcurrentQueue<VectorType> q;
		VectorType v1 = {1, 2, 3};
		q.Enqueue(std::move(v1));     // move
		VectorType v2 = q.Dequeue();  // move out
		assert(v2.size() == 3 && v2[0] == 1 && v2[1] == 2 && v2[2] == 3);
		assert(v1.empty());  // v1 should be moved-from
		std::cout << "test_non_trivial_move_type passed\n";
	}


	// Test 15: Semaphore count verification (by enqueuing N items, then dequeuing them all, ensuring no extra semaphore
	// signals)
	static void test_semaphore_count()
	{
		ConcurrentQueue<int> q;
		const int N = 10000;
		for (int i = 0; i < N; ++i)
			q.Enqueue(i);
		// Drain all items
		for (int i = 0; i < N; ++i)
		{
			int val = q.Dequeue();
			(void)val;
		}
		// Now the queue should be empty. We'll start a thread that tries to dequeue one more item.
		std::atomic<bool> started{false};
		std::thread consumer([&q, &started] {
			started = true;
			int val = q.Dequeue();  // should block because queue empty
			(void)val;
			started = false;
		});
		// Wait a bit to see if the thread blocked (i.e., started is still true)
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (!started)
		{
			// If started is false, the thread didn't start or already finished - meaning Dequeue returned immediately.
			// That would indicate the semaphore count was >0 when it should be 0.
			assert(false && "Semaphore count mismatch: Dequeue returned immediately when queue should be empty");
		}
		// Now enqueue an item to unblock the consumer
		q.Enqueue(42);
		consumer.join();
		assert(!started);  // should have finished after the item was enqueued
		std::cout << "test_semaphore_count passed\n";
	}

}  // namespace Neuro::Tests


int main()
{
	using namespace Neuro::Tests;

	test_basic();
	test_multiple_producers();
	test_blocking();
	test_stress_volume();
	test_random_delays();
	test_try_dequeue();
	test_move_only_type();
	test_move_only_tracking();
	test_destruction_with_items();
	test_large_semaphore();
	test_try_dequeue_after_drain();
	test_heavy_contention();
	test_large_volume();
	test_non_trivial_move_type();
	test_semaphore_count();

	std::cout << "All tests passed.\n";
}