#pragma once

#include <atomic>
#include <semaphore>


// NOTE: Queue was padded due to alignment specifier (intentional for cache-line alignment).
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif


namespace Neuro::Core {

	// Lock-free Multi-producer Single-consumer concurrent unbounded Queue.
	// Empty Slot means end of the Queue.
	//
	// Constraints:
	// 1. Only one thread may call Dequeue.
	// 2. No calls to Enqueue may happen after the destructor starts.
	//    (i.e., the queue must be empty or all producers must have stopped before destruction)
	//
	// TODO: Need optimize allocations (lock-free freelist or per-thread pool).
	//
	template <typename TValue>
	    requires std::is_default_constructible_v<TValue>
	class ConcurrentQueue
	{
	private:
		struct Slot;

		using SlotVal = TValue;
		using SlotRef = Slot&;
		using SlotPtr = Slot*;
		using AtomicSlotPtr = std::atomic<Slot*>;

		struct Slot final
		{
			SlotVal Value;
			AtomicSlotPtr Next{nullptr};

			Slot() = default;
			template <typename... Args>
			Slot(Args&&... args) : Value(std::forward<Args>(args)...), Next(nullptr)
			{
			}
		};

	public:
		ConcurrentQueue();
		~ConcurrentQueue();

		template <typename... Args>
		void Enqueue(Args&&... args);

		// Block until an item is available, then return it.
		TValue Dequeue();
		// Non-blocking peek, returns true if an item was dequeued.
		bool TryDequeue(TValue& out);

	private:
		// Atomic for Multiple producers.
		alignas(64) AtomicSlotPtr m_Head;
		// Non-atomic for Single consumer.
		alignas(64) SlotPtr m_Tail;

		static const uint32_t kSemaphoreSize = UINT_MAX;
		std::counting_semaphore<kSemaphoreSize> m_Semaphore{0};
	};


	template <typename TValue>
	    requires std::is_default_constructible_v<TValue>
	inline ConcurrentQueue<TValue>::ConcurrentQueue()
	{
		Slot* dummy = new Slot();
		m_Head.store(dummy, std::memory_order::relaxed);
		m_Tail = dummy;
	}


	template <typename TValue>
	    requires std::is_default_constructible_v<TValue>
	inline ConcurrentQueue<TValue>::~ConcurrentQueue()
	{
		while (true)
		{
			Slot* tail = m_Tail;
			Slot* next = tail->Next.load(std::memory_order::acquire);
			if (!next)
				break;

			m_Tail = next;
			delete tail;
		}
		delete m_Tail;
	}


	template <typename TValue>
	    requires std::is_default_constructible_v<TValue>
	template <typename... Args>
	inline void ConcurrentQueue<TValue>::Enqueue(Args&&... args)
	{
		Slot* slot = new Slot(std::forward<Args>(args)...);
		Slot* prev = m_Head.exchange(slot, std::memory_order::acq_rel);
		prev->Next.store(slot, std::memory_order::release);
		m_Semaphore.release();
	}


	template <typename TValue>
	    requires std::is_default_constructible_v<TValue>
	inline TValue ConcurrentQueue<TValue>::Dequeue()
	{
		m_Semaphore.acquire();
		TValue out;
		while (!TryDequeue(out))
			;
		return out;
	}


	template <typename TValue>
	    requires std::is_default_constructible_v<TValue>
	inline bool ConcurrentQueue<TValue>::TryDequeue(TValue& out)
	{
		Slot* tail = m_Tail;
		Slot* next = tail->Next.load(std::memory_order::acquire);
		if (!next)
			return false;

		out = std::move(next->Value);
		m_Tail = next;
		delete tail;
		return true;
	}

}  // namespace Neuro::Core


#ifdef _MSC_VER
#pragma warning(pop)
#endif
