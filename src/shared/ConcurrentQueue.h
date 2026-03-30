#pragma once

#include <atomic>


// NOTE: Queue was padded due to alignment specifier (intentional for cache-line alignment).
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif


namespace Neuro::Core {

	// Lock-free Multi-producer Single-consumer concurrent unbounded Queue.
	// Empty Slot means end of the Queue.
	//
	// This queue is intentionally "policy-free".
	// It is purely a data structure responsible for:
	// - safe concurrent Enqueue (multi-producer)
	// - safe single-threaded Dequeue (single-consumer)
	// - it does NOT: block, sleep, yield, etc.
	//
	// This queue provides only the mechanism.
	// The caller provides the policy.
	// Because otherwise mixing synchronization policy with the data structure leads to:
	// - hidden performance costs
	// - inflexible behavior across different workloads
	// - harder reasoning about threading behavior
	//
	//
	// Constraints:
	// 1. Only ONE thread may call TryDequeue / Dequeue.
	// 2. Producers may call Enqueue concurrently.
	// 3. No Enqueue after destruction begins.
	//
	// TODO: Need optimize allocations (lock-free freelist or per-thread pool).
	//
	template <typename TValue>
	class ConcurrentQueue final
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

		ConcurrentQueue(const ConcurrentQueue&) = delete;
		ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

		ConcurrentQueue(ConcurrentQueue&&) = delete;
		ConcurrentQueue& operator=(ConcurrentQueue&&) = delete;

		template <typename... Args>
		void Enqueue(Args&&... args);

		// Non-blocking dequeue attempt.
		// Returns true if an item was dequeued.
		// NOTE: If the queue is empty, this returns false immediately without waiting.
		bool TryDequeue(TValue& out);

		// Convenience blocking-like dequeue.
		// NOTE: This is NOT truly blocking, it spins until an item becomes available.
		// The caller should generally prefer TryDequeue and implement a proper waiting strategy externally.
		TValue Dequeue();

	private:
		// Atomic for Multiple producers.
		alignas(64) AtomicSlotPtr m_Head;
		// Non-atomic for Single consumer.
		alignas(64) SlotPtr m_Tail;
	};


	template <typename TValue>
	inline ConcurrentQueue<TValue>::ConcurrentQueue()
	{
		Slot* dummy = new Slot();
		m_Head.store(dummy, std::memory_order::relaxed);
		m_Tail = dummy;
	}


	template <typename TValue>
	inline ConcurrentQueue<TValue>::~ConcurrentQueue()
	{
		// NOTE: Caller must ensure no concurrent producers/consumer.
		while (m_Tail)
		{
			Slot* next = m_Tail->Next.load(std::memory_order::acquire);
			delete m_Tail;
			m_Tail = next;
		}
	}


	template <typename TValue>
	template <typename... Args>
	inline void ConcurrentQueue<TValue>::Enqueue(Args&&... args)
	{
		Slot* slot = new Slot(std::forward<Args>(args)...);
		Slot* prev = m_Head.exchange(slot, std::memory_order::acq_rel);
		prev->Next.store(slot, std::memory_order::release);
	}


	template <typename TValue>
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


	template <typename TValue>
	inline TValue ConcurrentQueue<TValue>::Dequeue()
	{
		while (true)
		{
			Slot* tail = m_Tail;
			Slot* next = tail->Next.load(std::memory_order::acquire);
			if (!next)
				continue;

			TValue out = std::move(next->Value);
			m_Tail = next;
			delete tail;
			return out;
		}
	}

}  // namespace Neuro::Core


#ifdef _MSC_VER
#pragma warning(pop)
#endif
