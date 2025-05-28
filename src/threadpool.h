#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
	ThreadPool(size_t numThreads);
	~ThreadPool();

	void enqueue(std::function<void()> task);
	void stop();

private:
	std::vector<std::thread> workers;
	std::queue<std::function<void()>> tasks;

	std::mutex queueMutex;
	std::condition_variable condition;
	std::atomic<bool> stopping{false};

	void workerLoop();
};