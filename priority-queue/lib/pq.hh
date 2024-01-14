#ifndef PQ_H

#define PQ_H

#include <memory>
#include <vector>
#include <iostream>

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
		T dequeue();
		// peek returns a readonly reference
		const T& peek() const;
		int count() const;
		bool isEmpty() const;
		void print() const;
	private:
		std::vector<std::unique_ptr<HeapNode<T> > > internalHeap;
};

inline int parentIdx(int idx) {
	return (idx-1) / 2;
}

inline int leftChild(int idx) {
	return (2*idx) + 1;
}

inline int rightChild(int idx) {
	return (2*idx) + 2;
}

/*
 * We are basically implementing a max heap
 * the maximum element stays at the top of it.
 * */

/*
 * Insert item at bottom and bubble up,
 * back and SIFT UP
 */
template<typename T>
void Pq<T>::enqueue(const T& item, int priority) {	
	this->internalHeap.push_back(
		std::make_unique<HeapNode<T>>(HeapNode<T>{priority, item})
	);
	
	int curr = this->internalHeap.size() - 1;
	while (curr > 0) {
		int parent = parentIdx(curr);
		if (parent < 0) {
			break;
		}
		// get non-owning reference for readonly access
		const HeapNode<T>& parentNode = *this->internalHeap.at(parent);
		if (parentNode.key > priority) {
			// we have reached a valid position 
			break;
		}
		// perform swap
		std::swap(this->internalHeap[parent], this->internalHeap[curr]);
		curr = parent;
	}
};

/*
 * To dequeue remove item at top, replace it with item at the bottom
 * Now bubble the item down, swapping with the larger child
 * TOP and SIFT DOWN, SWAP with larger child!
 * */
template<typename T>
T Pq<T>::dequeue() {
	// move the ptr and it's ownership out of the index 0 on vec
    std::unique_ptr<HeapNode<T> > resultN = std::move(this->internalHeap.at(0));
	
	// replace the idx 0 of vec with the last element in vec
    this->internalHeap[0] = std::move(this->internalHeap.back());

	// get rid of last element in vec because it has null value
    this->internalHeap.pop_back();

    int limit = this->internalHeap.size();

    int curr = 0;
    while (curr < limit) {
        int leftChildIdx = leftChild(curr);
        int rightChildIdx = rightChild(curr);

        int childToSwapWith = curr;
        if (leftChildIdx < limit &&
            this->internalHeap[leftChildIdx]->key > this->internalHeap[childToSwapWith]->key) {
            childToSwapWith = leftChildIdx;
        }

        if (rightChildIdx < limit &&
            this->internalHeap[rightChildIdx]->key > this->internalHeap[childToSwapWith]->key) {
            childToSwapWith = rightChildIdx;
        }

        if (childToSwapWith == curr) {
            break;
        }

		std::swap(this->internalHeap[curr], this->internalHeap[childToSwapWith]);
        curr = childToSwapWith;
    }

    return resultN->val;
}

template<typename T>
const T& Pq<T>::peek() const {
	return this->internalHeap.at(0)->val;		
};

template<typename T>
int Pq<T>::count() const {
	return this->internalHeap.size();
};

template<typename T>
bool Pq<T>::isEmpty() const {
	return this->internalHeap.size() == 0;
};

template<typename T>
void Pq<T>::print() const {
	for (int i = 0; i < this->internalHeap.size(); i++) {
		std::cout << this->internalHeap[i]->key << std::endl;
	}
};

#endif
