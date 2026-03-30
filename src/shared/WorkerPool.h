#pragma once

#include "ConcurrentQueue.h"

#include <functional>
#include <thread>


namespace Neuro::Net {

	// Task type for a Working Pool.
	// Used to distinguish actual tasks from stop signals.
	//
	enum class ETaskType
	{
		kWork,
		kStop,
		kNum,
	};

	// Move only function for Task.
	using TTaskFunctor = std::move_only_function<void()>;


	// Primitive Task definition for a worker pool.
	// NOTE: Suppose that function cannot throw (exceptions are disabled).
	//
	struct Task
	{
		ETaskType Type = ETaskType::kNum;
		TTaskFunctor Functor = nullptr;
	};


	// Worker with a Concurrent Queue.
	//
	class Worker final
	{
	public:
		Worker() = default;
		~Worker();

		Worker(const Worker&) = delete;
		Worker& operator=(const Worker&) = delete;

		Worker(Worker&&) = delete;
		Worker& operator=(Worker&&) = delete;

		bool TryEnqueue(Task&& task);

		void Start();
		void Stop();

	private:
		void Run();

		enum class EWorkerState : uint8_t
		{
			kNone,
			kRunning,
			kStopping
		};

		Core::ConcurrentQueue<Task> m_Queue;
		std::atomic<EWorkerState> m_State{EWorkerState::kNone};
		std::thread m_Thread;
	};


	// Round-robin dispatcher pool for Workers.
	//
	class WorkerPool final
	{
	public:
		explicit WorkerPool(uint32_t count);
		~WorkerPool();

		void Enqueue(Task&& task);
		
		template <typename TFunctor>
		void Enqueue(TFunctor&& functor);

	private:
		std::vector<Worker> m_Workers;
		std::atomic<uint32_t> m_Index{0};
	};


	template <typename TFunctor>
	void WorkerPool::Enqueue(TFunctor&& functor)
	{
		Task task;
		task.Type = ETaskType::kWork;
		task.Functor = std::forward<TFunctor>(functor);
		Enqueue(std::forward<Task>(task));
	}

}  // namespace Neuro::Net
