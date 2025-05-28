#include "threadpool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t numThreads) {
	for (size_t i = 0; i < numThreads; ++i) {
		workers.emplace_back([this] { workerLoop(); });
	}
}

void ThreadPool::enqueue(std::function<void()> task) {
	{
		std::unique_lock<std::mutex> lock(queueMutex);
		tasks.emplace(std::move(task));
	}
	condition.notify_one();
}

void ThreadPool::stop() {
	stopping = true;
	condition.notify_all();

	for (std::thread &worker : workers) {
		if (worker.joinable())
			worker.join();
	}
}

ThreadPool::~ThreadPool() {
	stop();
}

void ThreadPool::workerLoop() {
	while (!stopping) {
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			condition.wait(lock, [this] { return stopping || !tasks.empty(); });
			if (stopping && tasks.empty()) return;
			task = std::move(tasks.front());
			tasks.pop();
		}
		std::cout << "Worker thread executing task\n";
		task();
	}
}