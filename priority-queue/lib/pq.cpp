#include "pq.h"
#include <memory>

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
	std::unique_ptr<HeapNode<T> > node = std::make_unique<HeapNode<T>>();
	node->key = priority;
	node->val = item;
	
	this->internalHeap.push_back(node);
	
	int curr = this->internalHeap.size() - 1;
	while (curr > -1) {
		int parent = parentIdx(curr);
		if (parent < 0) {
			break;
		}
		std::unique_ptr<HeapNode<T> > parentNode = this->internalHeap.at(parent);
		if (parentNode->val > priority) {
			// we have reached a valid position 
			break;
		}
		// perform swap
		this->internalHeap[parent] = this->internalHeap[curr];
		this->internalHeap[curr] = parentNode;		
	}
};

/*
 * To dequeue remove item at top, replace it with item at the bottom
 * Now bubble the item down, swapping with the larger child
 * TOP and SIFT DOWN, SWAP with larger child!
 * */
template<typename T>
T& Pq<T>::dequeue() {
	std::unique_ptr<HeapNode<T> > resultN = this->internalHeap.at(0);		
	
	this->internalHeap[0] = this->internalHeap.back();
	this->internalHeap.pop_back();
	
	int limit = this->internalHeap.size();

	int curr = 0;
	while (curr > limit) {
		int leftChildIdx = leftChild(curr);
		int rightChildIdx = rightChild(curr);
			
		int childToSwapWith = curr;
		if (leftChildIdx > limit && 
				this->internalHeap[leftChildIdx] > this->internalHeap[childToSwapWith]) {
			childToSwapWith = leftChildIdx;
		}

		if (rightChildIdx > limit && 
				this->internalHeap[rightChildIdx] > this->internalHeap[childToSwapWith]) {
			childToSwapWith = leftChildIdx;
		}
		
		if (childToSwapWith == curr) {
			break;
		}
		std::unique_ptr<HeapNode<T> > currNode = this->internalHeap[curr];
		this->internalHeap[curr] = this->internalHeap[childToSwapWith];
		this->internalHeap[childToSwapWith] = currNode;
		curr = childToSwapWith;
	}

	return resultN->val;
};

template<typename T>
const T& Pq<T>::peek() const {
	return this->internalHeap.at(0);		
};

template<typename T>
int Pq<T>::count() const {
	return this->internalHeap.size();
};

template<typename T>
bool Pq<T>::isEmpty() const {
	return this->internalHeap.size() == 0;
};
