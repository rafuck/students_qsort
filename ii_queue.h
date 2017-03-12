#ifndef _II_QUEUE_
#define _II_QUEUE_
//--------------------------------------------------------------------
#include <queue>
#include <mutex>
#include <condition_variable>
template <typename T>
class IIQueue{
private:
	std::queue<T> q;
	std::mutex lock;
	std::condition_variable cond;
public:
	T wait(){
		std::unique_lock<std::mutex> lk(lock);
		cond.wait(lk, [this]{
			return !q.empty();
		});

		T ret = std::move(q.front());
		q.pop();
		return ret;
	}

	bool try_pop(T &item){
		if (q.empty()) return false;

		std::lock_guard<std::mutex> lk(lock);
		if (!q.empty()){
			item = std::move(q.front());
			q.pop();
			return true;
		}
		return false;
	}

	void push(T item){
		std::lock_guard<std::mutex> lk(lock);
		q.push(std::move(item));
		cond.notify_one();
	}

	bool empty() const{
		return q.empty();
	}
};

class CQueueCond{
private:
	std::mutex _wait;
	std::atomic<int> _counter;
public:
	CQueueCond():_counter(0){
		_wait.lock();
	}

	void wait(){
		if ((_counter--) > 0){
			return;
		}
		else{
			_counter++;
		}

		_wait.lock();
		if ((_counter--) > 1){
			_wait.unlock();
		}
	}

	void notify(){
		if ((_counter++) < 1){
			_wait.unlock();
		}
	}
};

template <typename T>
class IIQueueMutex{
private:
	std::queue<T> _q;
	std::mutex _lock;
	CQueueCond _wait;
public:
	void push(T item){
		std::lock_guard<std::mutex> lk(_lock);
		_q.push(std::move(item));
		_wait.notify();
	}

	T wait(){
		_wait.wait();
		{
			std::lock_guard<std::mutex> lk(_lock);
			if (!_q.empty()){
				T ret = std::move(_q.front());
				_q.pop();
				return ret;
			}
		}

		return wait();
	}
};

//--------------------------------------------------------------------
#endif
