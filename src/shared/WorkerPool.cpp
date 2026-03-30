#include "Precompiled.h"
#include "WorkerPool.h"


namespace Neuro::Net {


	static bool IsTaskValid(const Task& task)
	{
		static_assert((uint32_t)ETaskType::kNum == 2, "Add task type support.");
		assert(task.Type < ETaskType::kNum);
		return task.Type == ETaskType::kStop || (task.Type == ETaskType::kWork && task.Functor);
	}


	Worker::~Worker()
	{
		Stop();
	}


	bool Worker::TryEnqueue(Task&& task)
	{
		assert(IsTaskValid(task));

		// Gaurd Enqueue`s after Stop.
		if (m_State.load(std::memory_order::acquire) != EWorkerState::kRunning)
			return false;

		m_Queue.Enqueue(std::move(task));
		return true;
	}


	void Worker::Start()
	{
		// Guard double start.
		EWorkerState expected = EWorkerState::kNone;
		if (!m_State.compare_exchange_strong(expected, EWorkerState::kRunning, std::memory_order::acq_rel))
			return;

		// NOTE: It's fine to capture `this` since the thread won't outlive the Worker.
		m_Thread = std::thread([this]() {
			Run();
		});
	}


	void Worker::Run()
	{
		// Run current tasks in the queue until a Stop signal.
		// All tasks that happen to go after Stop will be discarded without execution.
		while (true)
		{
			Task task;

			// TODO: Implement some backoff strategy later or a blocking primitive (semaphore/condvar).
			if (!m_Queue.TryDequeue(task))
			{
				std::this_thread::yield();
				continue;
			}

			assert(IsTaskValid(task));
			static_assert((uint32_t)ETaskType::kNum == 2, "Add task type support.");

			if (task.Type == ETaskType::kStop)
				break;

			if (task.Type == ETaskType::kWork)
			{
				assert(task.Functor);
				task.Functor();
			}
		}
	}


	void Worker::Stop()
	{
		// Guard double stop.
		EWorkerState expected = EWorkerState::kRunning;
		if (!m_State.compare_exchange_strong(expected, EWorkerState::kStopping, std::memory_order::acq_rel))
			return;

		// Wake the thread if it's blocked.
		m_Queue.Enqueue(Task{ETaskType::kStop});

		if (m_Thread.joinable())
			m_Thread.join();

		m_State.store(EWorkerState::kNone, std::memory_order::release);
	}

}  // namespace Neuro::Net


namespace Neuro::Net {

	WorkerPool::WorkerPool(uint32_t count) : m_Workers(count)
	{
		assert(count > 0 && "Worker pool cannot be empty.");

		for (Worker& worker : m_Workers)
			worker.Start();
	}


	WorkerPool::~WorkerPool()
	{
		for (Worker& worker : m_Workers)
			worker.Stop();
	}


	void WorkerPool::Enqueue(Task&& task)
	{
		assert(m_Workers.size() > 0 && "Worker pool is empty.");
		assert(IsTaskValid(task));
		uint32_t index = m_Index.fetch_add(1, std::memory_order::relaxed);
		Worker& worker = m_Workers[index % m_Workers.size()];
		bool enqueued = worker.TryEnqueue(std::move(task));
		(void)enqueued;
		assert(enqueued && "Task dropped during enqueue.");
	}

}  // namespace Neuro::Net
