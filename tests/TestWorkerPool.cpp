#pragma once

#include <iostream>
#include <cassert>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <mutex>
#include <random>
#include <functional>

#include "Precompiled.h"
#include "WorkerPool.h"


namespace Neuro::Tests::Internal {

	using namespace Neuro::Net;

	// Helper to sleep for a random duration between min_ms and max_ms (inclusive)
	static void random_sleep(int min_ms, int max_ms)
	{
		thread_local std::mt19937 rng((uint32_t)std::random_device{}() +
		                              (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id()));
		std::uniform_int_distribution<int> dist(min_ms, max_ms);
		std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng)));
	}


	// Helper to run a test with different pool sizes
	template <typename TestFunc>
	void run_with_sizes(const std::vector<uint32_t>& sizes, TestFunc test)
	{
		for (uint32_t size : sizes)
			test(size);
	}


	static void test_basic_execution(uint32_t pool_size)
	{
		WorkerPool pool(pool_size);
		std::atomic<int> counter{0};

		const int num_tasks = 1000;
		for (int i = 0; i < num_tasks; ++i)
		{
			pool.Enqueue([&counter]() {
				counter.fetch_add(1, std::memory_order_relaxed);
			});
		}

		while (counter.load(std::memory_order_acquire) < num_tasks)
			std::this_thread::yield();
		NR_DEV_ASSERT(counter == num_tasks);
		std::cout << "test_basic_execution (size=" << pool_size << ") passed\n";
	}


	static void test_each_task_executes_once(uint32_t pool_size)
	{
		WorkerPool pool(pool_size);
		const int num_tasks = 1000;
		std::vector<std::atomic<int>> hits(num_tasks);
		std::atomic<int> completed{0};

		for (int i = 0; i < num_tasks; ++i)
		{
			pool.Enqueue([i, &hits, &completed]() {
				hits[i].fetch_add(1, std::memory_order_relaxed);
				completed.fetch_add(1, std::memory_order_relaxed);
			});
		}

		while (completed.load(std::memory_order_acquire) < num_tasks)
			std::this_thread::yield();
		for (int i = 0; i < num_tasks; ++i)
			NR_DEV_ASSERT(hits[i] == 1);
		std::cout << "test_each_task_executes_once (size=" << pool_size << ") passed\n";
	}


	static void test_concurrent_execution(uint32_t pool_size)
	{
		WorkerPool pool(pool_size);

		std::atomic<int> active{0};
		std::atomic<int> completed{0};
		std::atomic<int> max_active{0};


		const int num_tasks = 50;
		for (int i = 0; i < num_tasks; ++i)
		{
			pool.Enqueue([&]() {
				int now = active.fetch_add(1, std::memory_order_relaxed) + 1;
				// Update max concurrency
				int cur_max = max_active.load(std::memory_order_relaxed);
				while (now > cur_max && !max_active.compare_exchange_weak(cur_max, now, std::memory_order_relaxed))
					;
				random_sleep(15, 35);
				active.fetch_sub(1, std::memory_order_relaxed);
				completed.fetch_add(1, std::memory_order_relaxed);
			});
		}

		while (completed.load(std::memory_order_acquire) < num_tasks)
			std::this_thread::yield();
		if (pool_size > 1)
			NR_DEV_ASSERT(max_active > 1);
		else
			NR_DEV_ASSERT(max_active == 1);
		std::cout << "test_concurrent_execution (size=" << pool_size << ") passed\n";
	}


	static void test_single_worker_execution()
	{
		// Explicit test for pool size 1
		test_basic_execution(1);
		test_each_task_executes_once(1);
		test_concurrent_execution(1);
		std::cout << "test_single_worker_execution passed\n";
	}


	static void test_stress(uint32_t pool_size)
	{
		WorkerPool pool(pool_size);
		const int rounds = 50;
		const int tasks_per_round = 300;

		for (int round = 0; round < rounds; ++round)
		{
			std::atomic<int> counter{0};
			for (int i = 0; i < tasks_per_round; ++i)
			{
				pool.Enqueue([&counter]() {
					counter.fetch_add(1, std::memory_order_relaxed);
				});
			}
			while (counter.load(std::memory_order_acquire) < tasks_per_round)
				std::this_thread::yield();
			if (counter != tasks_per_round)
			{
				printf("FAILED at round %d: %d\n", round, counter.load());
				NR_DEV_ASSERT(false);
			}
		}
		std::cout << "test_stress (size=" << pool_size << ") passed\n";
	}


	static void test_concurrent_enqueue(uint32_t pool_size)
	{
		WorkerPool pool(pool_size);
		const int num_threads = 10;
		const int tasks_per_thread = 1000;
		std::atomic<int> total_tasks{0};

		std::vector<std::thread> enqueuers;
		for (int t = 0; t < num_threads; ++t)
		{
			enqueuers.emplace_back([&pool, tasks_per_thread, &total_tasks]() {
				for (int i = 0; i < tasks_per_thread; ++i)
				{
					pool.Enqueue([&total_tasks]() {
						total_tasks.fetch_add(1, std::memory_order_relaxed);
					});
				}
			});
		}

		for (auto& t : enqueuers)
		{
			t.join();
		}

		while (total_tasks.load(std::memory_order_acquire) < num_threads * tasks_per_thread)
			std::this_thread::yield();
		NR_DEV_ASSERT(total_tasks == num_threads * tasks_per_thread);
		std::cout << "test_concurrent_enqueue (size=" << pool_size << ") passed\n";
	}


	static void test_recursive_enqueue(uint32_t pool_size)
	{
		WorkerPool pool(pool_size);
		std::atomic<int> counter{0};
		const int depth = 4;
		const int branches = 3;

		// A task that enqueues more tasks
		std::function<void(int)> recursive_task = [&](int level) {
			if (level >= depth)
				return;
			for (int i = 0; i < branches; ++i)
			{
				pool.Enqueue([&, level]() {
					counter.fetch_add(1, std::memory_order_relaxed);
					recursive_task(level + 1);
				});
			}
		};

		// Start with a single task
		pool.Enqueue([&]() {
			counter.fetch_add(1, std::memory_order_relaxed);
			recursive_task(1);
		});

		int expected = 1;
		int pow = 1;
		for (int l = 1; l < depth; ++l)
		{
			pow *= branches;
			expected += pow;
		}

		while (counter.load(std::memory_order_acquire) < expected)
			std::this_thread::yield();
		NR_DEV_ASSERT(counter == expected);
		std::cout << "test_recursive_enqueue (size=" << pool_size << ") passed\n";
	}


	static void test_destruction_completes_tasks(uint32_t pool_size)
	{
		std::atomic<int> executed{0};
		const int num_tasks = 200;

		{
			WorkerPool pool(pool_size);
			for (int i = 0; i < num_tasks; ++i)
			{
				pool.Enqueue([&]() {
					executed.fetch_add(1, std::memory_order_relaxed);
				});
			}
		}

		NR_DEV_ASSERT(executed == num_tasks);
		std::cout << "test_destruction_completes_tasks (size=" << pool_size << ") passed\n";
	}


	static void test_eventual_completion_under_load(uint32_t pool_size)
	{
		WorkerPool pool(pool_size);
		std::atomic<int> counter{0};

		const int num_tasks = 10000;
		for (int i = 0; i < num_tasks; ++i)
		{
			pool.Enqueue([&]() {
				counter.fetch_add(1, std::memory_order_relaxed);
			});
		}

		while (counter.load(std::memory_order_acquire) < num_tasks)
			std::this_thread::yield();

		NR_DEV_ASSERT(counter == num_tasks);
		std::cout << "test_eventual_completion_under_load (size=" << pool_size << ") passed\n";
	}


	void TestWorkerPool()
	{
		std::cout << "Running WorkerPool tests:\n\n";

		// Test with various pool sizes
		std::vector<uint32_t> sizes = {1, 4, 10, 50};

		// Basic tests
		run_with_sizes(sizes, test_basic_execution);
		run_with_sizes(sizes, test_each_task_executes_once);
		run_with_sizes(sizes, test_concurrent_execution);
		run_with_sizes(sizes, test_eventual_completion_under_load);
		test_single_worker_execution();

		// Stress tests
		run_with_sizes(sizes, test_stress);
		run_with_sizes(sizes, test_concurrent_enqueue);
		run_with_sizes(sizes, test_recursive_enqueue);

		// Destruction tests
		run_with_sizes(sizes, test_destruction_completes_tasks);

		std::cout << "\nAll tests passed.\n";
	}

}  // namespace Neuro::Tests::Internal


namespace Neuro::Tests {

	void TestWorkerPool()
	{
		Internal::TestWorkerPool();
	}

}  // namespace Neuro::Tests
