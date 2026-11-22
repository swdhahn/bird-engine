#ifndef BIRD_HOT_RELOAD_QUEUE_H
#define BIRD_HOT_RELOAD_QUEUE_H

#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

#include "HotReloadTypes.h"

namespace bird::hotreload {

/**
 * Thread-safe, deduplicating multi-producer / single-consumer queue for
 * AssetReloadRequest messages.
 *
 * Background build worker threads push completed AssetReloadRequest entries
 * onto this queue. The main thread calls drain() once per frame inside
 * ResourceManager::pollHotReloads() to swap GPU/CPU handles safely on the
 * rendering thread.
 *
 * If the same AssetID is pushed multiple times before the main thread drains
 * the queue, the older entry is updated in place with the latest compiledPath
 * and generation so the main thread never performs redundant reloads.
 */
class ReloadQueue {
   public:
	ReloadQueue() = default;
	~ReloadQueue() = default;

	ReloadQueue(const ReloadQueue&) = delete;
	ReloadQueue& operator=(const ReloadQueue&) = delete;

	/**
	 * Enqueues a reload request from any thread.
	 *
	 * Assigns a monotonically increasing generation number if request.generation
	 * is 0. Coalesces duplicate requests targeting the same AssetID.
	 *
	 * @param request The asset reload request to enqueue.
	 */
	void push(AssetReloadRequest request) {
		if (request.generation == 0) {
			request.generation =
				m_nextGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
		}
		std::lock_guard<std::mutex> lock(m_mutex);
		for (AssetReloadRequest& existing : m_queue) {
			if (existing.assetId == request.assetId) {
				existing.compiledPath = std::move(request.compiledPath);
				existing.generation = request.generation;
				return;
			}
		}
		m_queue.push_back(std::move(request));
	}

	/**
	 * Pops a single reload request if available.
	 *
	 * @param out Destination filled with the popped request when true is returned.
	 * @return True if a request was popped, false if the queue was empty.
	 */
	bool tryPop(AssetReloadRequest& out) {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_queue.empty()) return false;
		out = std::move(m_queue.front());
		m_queue.pop_front();
		return true;
	}

	/**
	 * Atomically drains all pending reload requests in FIFO order.
	 *
	 * Intended to be called once per frame on the main thread.
	 *
	 * @return Vector of all queued requests; empty if none were pending.
	 */
	std::vector<AssetReloadRequest> drain() {
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<AssetReloadRequest> batch;
		batch.reserve(m_queue.size());
		while (!m_queue.empty()) {
			batch.push_back(std::move(m_queue.front()));
			m_queue.pop_front();
		}
		return batch;
	}

	/** @return True when no reload requests are currently queued. */
	bool empty() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_queue.empty();
	}

	/** @return Number of pending reload requests currently in the queue. */
	size_t size() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_queue.size();
	}

	/** Discards all pending reload requests. */
	void clear() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queue.clear();
	}

   private:
	mutable std::mutex m_mutex;
	std::deque<AssetReloadRequest> m_queue;
	std::atomic<uint64_t> m_nextGeneration{0};
};

}  // namespace bird::hotreload

#endif	// BIRD_HOT_RELOAD_QUEUE_H
