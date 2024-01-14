#include <iostream>
#include <stdexcept>
#include <string>
#include <pq.hh>

bool tryParse(std::string& input, int& output) {
	try {
		output = std::stoi(input);
		return true;
	} catch (std::invalid_argument) {
		return false;
	}
}

int main() {
	Pq<int> pq;

	std::string input;
	int x;

	while (true)
	{
		std::cout << "Enter num: " << std::endl;

		std::getline(std::cin, input);

		if (input == "Q") {
			break;
		}

		while (!tryParse(input, x)) {
			std::cout << "Bad entry. Enter a NUMBER: ";
			getline(std::cin, input);
			if (input == "Q") {
				break;
			}
		}

		pq.enqueue(x, x);	
	}

	std::cout << "Printing Unsorted List from num inputs " << std::endl;
	
	pq.print();
	
	std::cout << "Printing Sorted List from num inputs " << pq.count() << std::endl;

	while (!pq.isEmpty()) {
		std::cout << pq.dequeue() << std::endl;
	}
	
	std::cout << "-- Done --" << std::endl;

	return 0;
}
