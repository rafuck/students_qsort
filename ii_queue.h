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
		return std::move(ret);
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

//--------------------------------------------------------------------
#endif
