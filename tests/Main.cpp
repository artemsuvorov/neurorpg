#include <iostream>


namespace Neuro::Tests {

	extern void TestConcurrentQueue();
	extern void TestWorkerPool();

}  // namespace Neuro::Tests


int main()
{
	using namespace Neuro::Tests;

	TestConcurrentQueue();
	TestWorkerPool();

	std::cout << "Full test suite has passed.\n";
}
