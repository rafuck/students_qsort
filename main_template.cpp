#include <cstdlib>
#include <cstdio>
#include <thread>
#include <algorithm>
#include <time.h>
#include <random>
#include <future>
#include <functional>
#include <chrono>

template<typename T>
inline int comparator(const T &a, const T &b) noexcept{
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
void qSortSimpleParallel(const F &comparator, T *a, int left, int right){
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

			thread = std::thread(qSortSimpleParallel<F, T>, std::ref(comparator), a, left, r);
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

void print_a(const char *prefix, const double *a, int N, int tail = 0){
	printf("\n%s:\n", prefix);
	if (tail < 1 || tail > N/2){
		tail = N/2;
	}
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
	
	randomize(a, N);	
	timer();
	qSort(comparator<double>, a, 0, N-1);
	printf("Sorted at %1.9lf seconds", timer());
	print_a("Sorted(A)", a, N);

	randomize(a, N);
	timer();
	qSortSimpleParallel(comparator<double>, a, 0, N-1);
	printf("\nSimple parallel sorted at %1.9lf seconds", timer());
	print_a("Sorted(A)", a, N);

	delete[] a;
	return EXIT_SUCCESS;
}
