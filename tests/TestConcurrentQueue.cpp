#pragma once

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <chrono>
#include <random>

#include "Precompiled.h"
#include "ConcurrentQueue.h"


namespace Neuro::Tests::Internal {

	using namespace Neuro::Core;


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
		NR_DEV_ASSERT(val == 42);
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
			NR_DEV_ASSERT(v >= 0 && v < N);
			count[v]++;
		}
		for (int i = 0; i < N; ++i)
			NR_DEV_ASSERT(count[i] == 1);
		std::cout << "test_multiple_producers passed\n";
	}


	// Test 3: blocking consumer
	static void test_blocking()
	{
		ConcurrentQueue<int> q;
		std::thread consumer([&q] {
			int val;
			while (!q.TryDequeue(val))
				std::this_thread::yield();
			NR_DEV_ASSERT(val == 123);
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

		NR_DEV_ASSERT(produced == total);
		NR_DEV_ASSERT(results.size() == total);
		std::vector<int> count(total, 0);
		for (int v : results)
		{
			NR_DEV_ASSERT(v >= 0 && v < total);
			count[v]++;
		}
		for (int i = 0; i < total; ++i)
			NR_DEV_ASSERT(count[i] == 1);
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

		NR_DEV_ASSERT(produced == total);
		NR_DEV_ASSERT(results.size() == total);
		std::vector<int> count(total, 0);
		for (int v : results)
		{
			NR_DEV_ASSERT(v >= 0 && v < total);
			count[v]++;
		}
		for (int i = 0; i < total; ++i)
			NR_DEV_ASSERT(count[i] == 1);
		std::cout << "test_random_delays passed\n";
	}


	// Test 6: TryDequeue behavior
	static void test_try_dequeue()
	{
		ConcurrentQueue<int> q;
		int out = 0;
		NR_DEV_ASSERT(!q.TryDequeue(out));
		q.Enqueue(42);
		NR_DEV_ASSERT(q.TryDequeue(out));
		NR_DEV_ASSERT(out == 42);
		NR_DEV_ASSERT(!q.TryDequeue(out));
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
		NR_DEV_ASSERT(val.value == 123);
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
		NR_DEV_ASSERT(MoveCounter::moves >= 2);  // at least two moves
		NR_DEV_ASSERT(MoveCounter::copies == 0);
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
		NR_DEV_ASSERT(Tracker::constructed == Tracker::destructed);
		std::cout << "test_destruction_with_items passed\n";
	}


	// Test 10: enqueue many items before any dequeue
	static void test_large_volume_ordering()
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
			NR_DEV_ASSERT(val == i);
			(void)val;
		}
		std::cout << "test_large_volume_ordering passed\n";
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
			NR_DEV_ASSERT(q.TryDequeue(out));
			(void)out;
		}
		// Still 5 items left
		int out2;
		NR_DEV_ASSERT(q.TryDequeue(out2));
		// Drain all
		int tmp;
		while (q.TryDequeue(tmp))
			;  // keep draining
		NR_DEV_ASSERT(!q.TryDequeue(out2));
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
				int val;
				if (!q.TryDequeue(val))
				{
					std::this_thread::yield();
					continue;
				}

				consumed++;
				(void)val;
				if (consumed % 1000 == 0)
					std::this_thread::yield();  // occasional yield to keep things fair
			}
		});

		for (auto& t : producers)
			t.join();
		consumer.join();

		NR_DEV_ASSERT(produced == total);
		NR_DEV_ASSERT(consumed == total);
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
			NR_DEV_ASSERT(val == i);
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
		NR_DEV_ASSERT(v2.size() == 3 && v2[0] == 1 && v2[1] == 2 && v2[2] == 3);
		NR_DEV_ASSERT(v1.empty());  // v1 should be moved-from
		std::cout << "test_non_trivial_move_type passed\n";
	}


	// Test 15: Test that empty queue behaves correctly
	static void test_empty_behavior()
	{
		ConcurrentQueue<int> q;

		int val;
		(void)val;
		NR_DEV_ASSERT(!q.TryDequeue(val));

		q.Enqueue(42);
		NR_DEV_ASSERT(q.TryDequeue(val));
		NR_DEV_ASSERT(val == 42);

		NR_DEV_ASSERT(!q.TryDequeue(val));

		std::cout << "test_empty_behavior passed\n";
	}


	// Test 16: Enqueue after heavy draining
	static void test_reuse_after_drain()
	{
		ConcurrentQueue<int> q;

		for (int round = 0; round < 5; ++round)
		{
			for (int i = 0; i < 1000; ++i)
				q.Enqueue(i);

			int val;
			int count = 0;
			while (q.TryDequeue(val))
				count++;

			NR_DEV_ASSERT(count == 1000);
		}

		std::cout << "test_reuse_after_drain passed\n";
	}


	// Test 17: Stress test specifically designed to expose the dummy node hazard
	// by creating many enqueue/dequeue operations with random delays.
	static void test_dummy_node_hazard()
	{
		const int num_producers = 8;
		const int items_per_producer = 2500;  // total 20,000 items

		std::random_device rd;
		for (int run = 0; run < 10; ++run)  // repeat to increase chance of hitting race
		{
			ConcurrentQueue<int> q;
			std::vector<std::thread> producers;
			std::atomic<int> produced{0};
			std::atomic<int> consumed{0};
			const int total = num_producers * items_per_producer;

			// Producers with random yields
			for (int i = 0; i < num_producers; ++i)
			{
				producers.emplace_back([&q, &produced, i, items_per_producer, seed = rd()] {
					std::mt19937 rng(seed);
					std::uniform_int_distribution<int> delay_prob(0, 99);
					for (int j = 0; j < items_per_producer; ++j)
					{
						q.Enqueue(i * items_per_producer + j);
						produced++;
						if (delay_prob(rng) < 5)  // 5% chance to yield
							std::this_thread::yield();
					}
				});
			}

			// Consumer with random delays and occasional yields
			std::thread consumer([&q, &consumed, total, seed = rd()] {
				std::mt19937 rng(seed);
				std::uniform_int_distribution<int> delay_prob(0, 99);
				std::vector<int> seen(total, 0);
				while (consumed.load() < total)
				{
					int val;
					if (q.TryDequeue(val))
					{
						NR_DEV_ASSERT(val >= 0 && val < total);
						seen[val]++;
						consumed++;
					}
					else
					{
						if (delay_prob(rng) < 2)  // occasional yield when empty
							std::this_thread::yield();
					}
				}
				// Verify all items seen exactly once
				for (int i = 0; i < total; ++i)
					NR_DEV_ASSERT(seen[i] == 1);
			});

			for (auto& t : producers)
				t.join();
			consumer.join();

			NR_DEV_ASSERT(produced == total);
			NR_DEV_ASSERT(consumed == total);
		}
		std::cout << "test_dummy_node_hazard passed\n";
	}


	// Test 18: Use a complex type that may expose memory corruption if dummy node is mishandled
	static void test_complex_type()
	{
		struct Complex
		{
			std::vector<int> data;
			std::string name;
			int id;

			Complex() : id(0) {}
			Complex(int i) : data({i, i + 1, i + 2}), name("Item" + std::to_string(i)), id(i) {}
			Complex(const Complex&) = delete;
			Complex(Complex&&) = default;
			Complex& operator=(const Complex&) = delete;
			Complex& operator=(Complex&&) = default;
		};

		const int num_items = 50000;
		ConcurrentQueue<Complex> q;

		// Enqueue many items
		for (int i = 0; i < num_items; ++i)
			q.Enqueue(Complex(i));

		// Dequeue and verify
		for (int i = 0; i < num_items; ++i)
		{
			Complex c = q.Dequeue();
			NR_DEV_ASSERT(c.id == i);
			NR_DEV_ASSERT(c.data.size() == 3);
			NR_DEV_ASSERT(c.data[0] == i);
			NR_DEV_ASSERT(c.data[1] == i + 1);
			NR_DEV_ASSERT(c.data[2] == i + 2);
			NR_DEV_ASSERT(c.name == "Item" + std::to_string(i));
		}

		// Ensure queue is empty
		Complex dummy;
		NR_DEV_ASSERT(!q.TryDequeue(dummy));

		std::cout << "test_complex_type passed\n";
	}

}  // namespace Neuro::Tests::Internal


namespace Neuro::Tests {

	void TestConcurrentQueue()
	{
		using namespace Internal;

		std::cout << "Running TestConcurrentQueue:\n\n";

		test_basic();
		test_multiple_producers();
		test_blocking();
		test_stress_volume();
		test_random_delays();
		test_try_dequeue();
		test_move_only_type();
		test_move_only_tracking();
		test_destruction_with_items();
		test_large_volume_ordering();
		test_try_dequeue_after_drain();
		test_heavy_contention();
		test_large_volume();
		test_non_trivial_move_type();
		test_empty_behavior();
		test_reuse_after_drain();
		test_dummy_node_hazard();
		test_complex_type();

		std::cout << "All tests passed.\n\n";
	}

}  // namespace Neuro::Tests
