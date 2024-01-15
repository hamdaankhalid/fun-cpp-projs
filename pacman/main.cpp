#include <cstdlib>
#include <iostream>
#include <memory>
#include <sys/fcntl.h>
#include <sys/termios.h>
#include <termios.h>
#include <sys/poll.h>
#include <vector>
#include <unistd.h>	// For usleep function
#include <fcntl.h>	// for making stdin non-blocking
#include <poll.h>
#include <unordered_map>

const char PACMAN = 'O';
const char QUIT = 'q';
const char UP_CMD = 'w';
const char DOWN_CMD = 's';
const char LEFT_CMD = 'a';
const char RIGHT_CMD = 'd';

// Decides how many FPS to update
const int FRAME = 100000;
const int EVENT_LOOP_INPUT_POLL = 50;

enum Direction {
	UP,
	DOWN,
	RIGHT,
	LEFT,
};

struct Input {
	int moverId;
	Direction dir;
};

struct Position {
	int x;
	int y;
};

class Gameboard {
	public:
		Gameboard(int rows, int cols);	
		void draw();
		int insertMovable();
		void updateMovable(const std::unique_ptr<Input>& input);
	private:
		std::vector<std::vector<char> > board;
		int rows;
		int cols;
		int pidCounter;
		std::unordered_map<int, std::unique_ptr<Position> > movables;
};


Gameboard::Gameboard(int rows, int cols) : rows(rows), cols(cols), pidCounter(0) {
	for (int i = 0; i < rows; i++) {
		std::vector<char> row(cols, '.');
		this->board.push_back(row);
	}
}

void Gameboard::draw() {
	for (int i = 0; i < this->rows; i++) {
		for (int j = 0; j < this->cols; j++) {
			std::cout << this->board[i][j] << " ";
		}
		std::cout << "\n";
	}
	
	for (int i = 0; i < pidCounter; i++) {
		const Position& pos = *this->movables[i];
		this->board[pos.y][pos.x] = PACMAN;
	}
}

int Gameboard::insertMovable() {
	this->movables[this->pidCounter] = std::make_unique<Position>(Position{0, 0});
	this->pidCounter++;
	return this->pidCounter-1;
}

void Gameboard::updateMovable(const std::unique_ptr<Input>& input) {
	// refernce of movable pos, without taking ownership
	Position& movablePos = *this->movables[input->moverId];
	// TODO: bounds checking
	switch (input->dir) {
		case UP:
			if (movablePos.y > 0)
				movablePos.y--;
			break;
		case DOWN:
			if (movablePos.y < rows-1)
				movablePos.y++;
			break;
		case LEFT:
			if (movablePos.x > 0)
				movablePos.x--;
			break;
		case RIGHT:
			if (movablePos.x < cols-1)
				movablePos.x++;
			break;
		default:
			break;
	}
}

// get updates from user and AI players using eventloop style IO multiplexing
int handleFakeInterrupt(struct pollfd fds[], std::vector<std::unique_ptr<Input> >& buf) {
	// "1" specifies size of fds
	int result = poll(fds, 1, EVENT_LOOP_INPUT_POLL);
	// some FD has an event for us
	if (result > 0) {
		// based on which FD it is, we map to the player and move the player
		if (fds[0].revents & POLLIN) {
			char buffer[20];
			ssize_t bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer));

			if (bytesRead > 0) {
				for (int i = 0; i < bytesRead; i++) {
					std::unique_ptr<Input> userInput = nullptr;
					switch (buffer[i]) {
						case UP_CMD:
							userInput = std::make_unique<Input>(Input{0, UP});
							break;
						case DOWN_CMD:
							userInput = std::make_unique<Input>(Input{0, DOWN});
							break;
						case LEFT_CMD:
							userInput = std::make_unique<Input>(Input{0, LEFT});
							break;
						case RIGHT_CMD:
							userInput = std::make_unique<Input>(Input{0, RIGHT});
							break;
						case QUIT:
							return -1;
						default:
							break;
					}
					if (userInput != nullptr) {
						buf.push_back(std::move(userInput));
					}
				}
				return 0;
			}
		}
	}

	return 0;
}

// RAII wrapper to restore state of terminal
class TerminalInputConfigManager {
	public:
		TerminalInputConfigManager() {
			// save original terminal state for restoring later
			this->originalTerminalAttr = std::make_unique<struct termios>();
			tcgetattr(STDIN_FILENO, this->originalTerminalAttr.get());
		}
		
		int useRawInput() {
			struct termios t;
			if(tcgetattr(STDIN_FILENO, &t) < 0) {
				return -1;
			}

			t.c_lflag &= ~(ICANON | ECHO);
			t.c_cc[VMIN] = 0;
			t.c_cc[VTIME] = 0; // Timeout in tenths of a second

			if (tcsetattr(STDIN_FILENO, TCSANOW, &t) < 0) {
				return -1;
			}

			return 0;
		}

		~TerminalInputConfigManager() {
			tcsetattr(STDIN_FILENO, TCSANOW, this->originalTerminalAttr.get());
		}

	private:
		std::unique_ptr<struct termios> originalTerminalAttr;
};


int main() {
	TerminalInputConfigManager cm;
	if (cm.useRawInput() < 0) {
		return -1;
	}

	// monitor FD's for user input (later add FD for AI to write to), using IO multiplexing	
	struct pollfd fds[1];
	// monitor FD for standard in
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	
	// use the same vector to fill and drain
	std::vector<std::unique_ptr<Input> > gameplayInstructionBuffer;
	
	// setup gameboard
	Gameboard gb(10, 10);
	// add base player
	gb.insertMovable();

	while (true)
	{
		if (handleFakeInterrupt(fds, gameplayInstructionBuffer) < 0) {
			// user wanted to exit the game
			break;
		}
	
		for (auto i = 0u; i < gameplayInstructionBuffer.size(); i++) {
			std::unique_ptr<Input> instruction = std::move(gameplayInstructionBuffer[i]);
			// make changes in each player
			gb.updateMovable(instruction);
		}
		gameplayInstructionBuffer.clear();

		gb.draw();

		usleep(FRAME);

		// clear screen	by handing command to shell
		system("clear");
	}
	
	std::cout << "Thanks for playing!" << std::endl;
	return 0;
}
