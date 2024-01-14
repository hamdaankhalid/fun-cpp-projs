#ifndef PQ_H

#define PQ_H

#include <memory>
#include <vector>

template<typename T>
struct HeapNode {
	int key;
	T val;
};

// internally this is only a MaxHeap
template<typename T>
class Pq {
	public:
		// content of item is copied or moved
		void enqueue(const T& item, int priority);
		T& dequeue();
		// peek returns a readonly reference
		const T& peek() const;
		int count() const;
		bool isEmpty() const;
	private:
		std::vector<std::unique_ptr<HeapNode<T> > > internalHeap;
};

#endif
