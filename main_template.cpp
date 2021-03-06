#ifdef _MSC_VER || _WIN32 || _WIN64
	#include "stdafx.h"
#endif

#include <cstdlib>
#include <cstdio>
#include <thread>
#include <algorithm>
#include <time.h>
#include <atomic>
#include <random>
#include <future>
#include <functional>
#include <chrono>
#include "ii_queue.h"

template<typename T>
inline int comparator(const T &a, const T &b){
	return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

template<typename F, typename T>
void qSort(const F &comparator, T *a, int left, int right){
	int l = left,
		r = right;
	T b = a[left + (right-left)/2];
	while(l<=r){
		while(comparator(a[l], b) < 0) l++;
		while(comparator(a[r], b) > 0) r--;

		if (l <= r){
			std::swap(a[l], a[r]);
			l++; r--;
		}
	}

	if (l < right) qSort(comparator, a, l, right);
	if (left < r ) qSort(comparator, a, left, r );
}

template<typename F, typename T>
void qSortSimpleParallel(const F comparator, T *a, int left, int right){
	int l = left,
		r = right;
	T b = a[left + (right-left)/2];
	while(l<=r){
		while(comparator(a[l], b) < 0) l++;
		while(comparator(a[r], b) > 0) r--;

		if (l <= r){
			std::swap(a[l], a[r]);
			l++; r--;
		}
	}

	static std::atomic<int> threadCounter(std::thread::hardware_concurrency()-1);
	std::thread thread;
	if (left < r){
		if (r - left > 256 && threadCounter > 0){
			threadCounter--;

			thread = std::thread(qSortSimpleParallel<F, T>, comparator, a, left, r);
		}
		else{
			qSortSimpleParallel(comparator, a, left, r);
		}
	}
	if (l < right) qSortSimpleParallel(comparator, a, l, right);

	if (thread.joinable()){
		thread.join();
		threadCounter++;
	}
}


template<typename F, typename T, typename Q>
class QSortThreadPool{
private:
	std::thread *threadPool;
	T *a;
	int N;
	const F &comparator;

	std::atomic<int> taskCounter;
	std::function<int(int, int)> sortMethod;

	std::condition_variable end;
	std::mutex endLock;

	Q taskQueue;

	void threadFunction(){
		while(true){
			auto task = taskQueue.wait();
			if (!task.valid()){
				break;
			}

			task();
			int old = taskCounter.fetch_add(-1, std::memory_order_relaxed);
			if (old == 1){
				std::lock_guard<std::mutex> lk(endLock);
				end.notify_one();
			}
		}
	}

	int _qSort(int left, int right){
		int l = left,
			r = right;
		T b = a[left + (right-left)/2];
		while(l<=r){
			while(comparator(a[l], b) < 0) l++;
			while(comparator(a[r], b) > 0) r--;

			if (l <= r){
				std::swap(a[l], a[r]);
				l++; r--;
			}
		}

		if (l < right){
			if (left < r && right - l > 256){
				taskCounter.fetch_add(1, std::memory_order_relaxed);
				auto task = std::packaged_task<int()>(
					std::bind(
						&QSortThreadPool::_qSort, this, l, right
					)
				);
				taskQueue.push(std::move(task));
			}
			else{
				_qSort(l, right);
			}
		}

		if (left < r ){
			_qSort(left, r );
		}

		return 0;
	}
public:
	QSortThreadPool(const F &_comparator):comparator(_comparator){
		sortMethod = std::bind(&QSortThreadPool<F,T,Q>::_qSort, this, std::placeholders::_1, std::placeholders::_2);
		const size_t nCores = std::thread::hardware_concurrency();
		threadPool = new std::thread[nCores];
		for(int i=0; i<nCores; ++i){
			threadPool[i] = std::thread(std::bind(&QSortThreadPool<F,T,Q>::threadFunction, this));
		}
	}

	void operator()(T *_a, int _N){
		a = _a;
		N = _N;
		taskCounter = 0;

		_qSort(0, N-1);

		
		if (taskCounter){
			endLock.lock();
			if (taskCounter){
				endLock.unlock();

				std::unique_lock<std::mutex> lk(endLock);
				end.wait(lk, [this]{
					return taskCounter.load(std::memory_order_relaxed) == 0;
				});
			}
			else{
				endLock.unlock();
			}
		}
	}

	~QSortThreadPool(){
		const size_t nCores = std::thread::hardware_concurrency();
		for(int i=0; i<nCores; ++i){
			std::packaged_task<int()> task;
			taskQueue.push(std::move(task));
		}

		for(int i=0; i<nCores; ++i){
			if (threadPool[i].joinable()){
				threadPool[i].join();
			}
		}

		delete[] threadPool;
	}
};

void print_a(const char *prefix, const double *a, int N){
	printf("\n%s:\n", prefix);
	int tail = N/2;
	if (tail > 10){
		tail = 10;
	}

	for(int i=0; i<tail; ++i){
		printf("a[%d] = %1.12lf\n", i, a[i]);
	}

	if (tail < N/2){
		printf("....\n");
	}

	for(int i=N-tail; i<N; ++i){
		printf("a[%d] = %1.12lf\n", i, a[i]);
	}
}

double timer(){
	static char is = 0;
	static std::chrono::time_point<std::chrono::high_resolution_clock> start;
	if (!is){
		is = 1;
		start = std::chrono::high_resolution_clock::now();
		return 0;
	}
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> diff = end-start;
	start = end;
	return diff.count();
}

void randomize(double *a, int N){
	std::random_device rd;
    std::srand(rd());

	for(int i=0; i<N; ++i){
		a[i] = (double)(std::rand())/RAND_MAX;
	}
}

int main(int argc, char *argv[]){
	const int N = 32*1024*1024;
	double *a = new double[N];
	
	/*randomize(a, N);	
	timer();
	qSort(comparator<double>, a, 0, N-1);
	printf("Sorted at %1.9lf seconds", timer());
	print_a("Sorted(A)", a, N);

	randomize(a, N);
	timer();
	qSortSimpleParallel(comparator<double>, a, 0, N-1);
	printf("\nSimple parallel sorted at %1.9lf seconds", timer());
	print_a("Sorted(A)", a, N);*/

	{
		randomize(a, N);
		QSortThreadPool<int(const double&, const double&), double, IIQueue<std::packaged_task<int()>>> sorter(comparator<double>);
		timer();
		sorter(a, N);
		printf("\nAsync std cond parallel sorted at %2.9lf seconds", timer());
		print_a("Sorted(A)", a, N);
	}

	{
		randomize(a, N);
		QSortThreadPool<int(const double&, const double&), double, IIQueueMutex<std::packaged_task<int()>>> sorter(comparator<double>);
		timer();
		sorter(a, N);
		printf("\nAsync cond mutex parallel sorted at %2.9lf seconds", timer());
		print_a("Sorted(A)", a, N);
	}

	delete[] a;
	return EXIT_SUCCESS;
}
