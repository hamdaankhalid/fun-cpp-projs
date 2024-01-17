#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <sys/fcntl.h>   // for making stdin non-blocking
#include <sys/poll.h>    // IO multiplexing
#include <sys/termios.h> // interacting with terminal
#include <tuple>
#include <unistd.h> // For usleep function
#include <unordered_map>
#include <utility>
#include <vector>

const char PACMAN = 'O';
const char GHOST = 'X';
const char QUIT = 'q';
const char UP_CMD = 'w';
const char DOWN_CMD = 's';
const char LEFT_CMD = 'a';
const char RIGHT_CMD = 'd';

// Decides how many FPS to update
const int FRAME = 100000;
const int SECOND = 1000000;
const int NON_BLOCKING_EVENT_LOOP_INPUT_POLL = 0;

int getRandomInt(int upper) {
  // Create a random device to obtain seed
  std::random_device rd;

  // Create a random number generator using the seed from the random device
  std::mt19937 gen(rd());

  // Define the distribution for integers in the range [0, upper]
  std::uniform_int_distribution<int> distribution(0, upper);

  // Generate a random integer
  return distribution(gen);
}

void runCountdown(int i) {
  system("clear");
  std::cout << "Game starting in " << i << " seconds." << std::endl;
  usleep(SECOND);
  system("clear");
  for (int j = i; j >= 0; j--) {
    std::cout << j << " seconds to go!" << std::endl;
    usleep(SECOND);
    system("clear");
  }
}

enum Direction {
  UP,
  DOWN,
  RIGHT,
  LEFT,
  NOOP,
};

Direction dirFrom[4] = {UP, DOWN, LEFT, RIGHT};

struct Input {
  int moverId;
  Direction dir;
};

struct Position {
  int x;
  int y;
  Direction dir;
};

// forward decl
class Ghost;

class Gameboard {
public:
  Gameboard(int rows, int cols);
  void draw(std::vector<std::unique_ptr<Input>> &updates);
  int insertMovable();

private:
  std::vector<std::vector<char>> board;
  int rows;
  int cols;
  int pidCounter;
  std::unordered_map<int, std::unique_ptr<Position>> movables;

  std::pair<int, int> getCurrPos(int pid);

  // update and get prev position
  std::pair<int, int> updateMovable(const Input &input);

  std::pair<std::pair<int, int>, Direction>
  validateMove(std::pair<int, int> &currPos, Direction dir);

  friend class Ghost;
};

std::pair<int, int> Gameboard::getCurrPos(int pid) {
  const Position &pos = *this->movables[pid];
  return std::make_pair(pos.y, pos.x);
}

Gameboard::Gameboard(int rows, int cols)
    : rows(rows), cols(cols), pidCounter(0) {
  for (int i = 0; i < rows; i++) {
    std::vector<char> row(cols, '.');
    this->board.push_back(row);
  }
}

const char ghostDir[4] = {'v', '^', '<', '>'};

void Gameboard::draw(std::vector<std::unique_ptr<Input>> &updates) {
  // go over the updates, make the updates and mark the inverse as empty
  for (auto i = 0u; i < updates.size(); i++) {
    const Input &update = *updates[i];
    std::pair<int, int> prevPos = this->updateMovable(update);
    // when someone has passed over the cell is now empty
    this->board[prevPos.first][prevPos.second] = ' ';
  }

  for (int i = 0; i < pidCounter; i++) {
    const Position &pos = *this->movables[i];
    char repr;
    if (i == 0) {
      repr = PACMAN;
    } else {
      repr = ghostDir[pos.dir]; // based on the dir we should get the ghost char
    }
    this->board[pos.y][pos.x] = repr;
  }

  // print board to screen in one go
  int strBoardSize = this->rows * this->cols * 2;  // 2 represents the spaces and EOL
  strBoardSize--;
  std::string strBoard(strBoardSize, '\n');
  for (int i = 0; i < strBoardSize; i++)
  {
	  if (i != 0 && i % (this->rows*2) == 0)
	  {
		  continue;
	  }
	
	  if (i % 2 != 0) 
	  {
		  strBoard[i] = ' ';
	  }
	  else 
	  {
		  int j = i/2;

		  int normalizedRow = j / this->rows;
		  int normalizedCol = j % this->cols;

		  strBoard[i] = this->board[normalizedRow][normalizedCol];
	  }
  };
  
  std::cout << strBoard << std::endl;
}

int Gameboard::insertMovable() {
  int row = 0;
  int col = 0;

  if (this->pidCounter != 0) {
    row = getRandomInt(this->rows - 1);
    col = getRandomInt(this->cols - 1);
  }

  this->movables[this->pidCounter] =
      std::make_unique<Position>(Position{row, col});
  this->pidCounter++;

  // the movable Id for the newly inserted movable
  return this->pidCounter - 1;
}

std::pair<std::pair<int, int>, Direction>
Gameboard::validateMove(std::pair<int, int> &currPos, Direction dir) {
  switch (dir) {
  case UP:
    if (currPos.first > 0)
      return std::make_pair(std::make_pair(-1, 0), UP);
    break;
  case DOWN:
    if (currPos.first < rows - 1)
      return std::make_pair(std::make_pair(1, 0), DOWN);
    break;
  case LEFT:
    if (currPos.second > 0)
      return std::make_pair(std::make_pair(0, -1), LEFT);
    break;
  case RIGHT:
    if (currPos.second < cols - 1)
      return std::make_pair(std::make_pair(0, 1), RIGHT);
    break;
  default:
    break;
  }
  return std::make_pair(std::make_pair(0, 0), NOOP);
}

std::pair<int, int> Gameboard::updateMovable(const Input &input) {
  // Take reference of movable pos, without taking ownership of it
  Position &movablePos = *this->movables[input.moverId];
  std::pair<int, int> initPos = std::make_pair(movablePos.y, movablePos.x);

  std::pair<int, int> offset = this->validateMove(initPos, input.dir).first;

  movablePos.dir = input.dir;

  movablePos.y += offset.first;
  movablePos.x += offset.second;
  
  if (movablePos.y == initPos.first && movablePos.x == initPos.second) {
    return std::make_pair(-1, -1);
  }

  return initPos;
}

class Ghost {
public:
  Ghost(int id);
  std::unique_ptr<Input> getNextMove(Gameboard &gb);

private:
  int id;
  Direction lastMove;
};

// get random lastMove
Ghost::Ghost(int id) : id(id), lastMove(dirFrom[getRandomInt(3)]) {}

std::unique_ptr<Input> Ghost::getNextMove(Gameboard &gb) {
  while (true) {
    Direction moveToMake;
    switch (this->lastMove) {
    case NOOP:
      moveToMake = DOWN;
      break;
    default:
      moveToMake = lastMove;
    }
    // would making the move be a valid move on the board? if not give me a
    // random diff dir
    Input ghostInput;
    ghostInput.moverId = this->id;
    ghostInput.dir = moveToMake;

    std::pair<int, int> currPos = gb.getCurrPos(this->id);
    Direction dir = gb.validateMove(currPos, moveToMake).second;
    if (dir != NOOP) {
      this->lastMove = moveToMake;
      return std::make_unique<Input>(std::move(ghostInput));
    }
    this->lastMove = dirFrom[getRandomInt(3)];
  }
}

// get updates from user using eventloop style IO multiplexing
int handleFakeInterrupt(struct pollfd fds[],
                        std::vector<std::unique_ptr<Input>> &buf) {
  // "1" specifies size of fds
  int result = poll(fds, 1, NON_BLOCKING_EVENT_LOOP_INPUT_POLL);
  // some FD has an event for us
  if (result > 0) {
    // based on which FD it is, we map to the player and move the player
    if (fds[0].revents & POLLIN) {
      char buffer[256];
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
    if (tcgetattr(STDIN_FILENO, &t) < 0) {
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

  // monitor FD's for user input (later add FD for AI to write to), using IO
  // multiplexing
  struct pollfd fds[1];
  // monitor FD for standard in
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;

  // use the same vector to fill and drain
  std::vector<std::unique_ptr<Input>> gameplayInstructionBuffer;

  // setup gameboard
  Gameboard gb(10, 10);
  // add base player
  int userId = gb.insertMovable();
  int ghost1Id = gb.insertMovable();
  int ghost2Id = gb.insertMovable();

  std::cout << "User created with id: " << userId << std::endl;
  std::cout << "2 ghosts added with ids " << ghost1Id << ", and " << ghost2Id
            << std::endl;

  // add two ghosts
  Ghost ghosts[2] = {Ghost(ghost1Id) , Ghost(ghost2Id)};

  // runCountdown(3);

  bool moveGhost = false;
  while (true) {
	
	// slow the ghosts down?
	if (moveGhost) {
		for (int i = 0; i < 2; i++) {
		  gameplayInstructionBuffer.push_back(ghosts[i].getNextMove(gb));
		}
	}
	moveGhost = !moveGhost;

    if (handleFakeInterrupt(fds, gameplayInstructionBuffer) < 0) {
      // user wanted to exit the game
      break;
    }

    gb.draw(gameplayInstructionBuffer);
    gameplayInstructionBuffer.clear();

    usleep(FRAME);

    // clear screen	by handing command to shell
    system("clear");
  }

  std::cout << "Thanks for playing!" << std::endl;
  std::cout << "~ Hamdaan Khalid" << std::endl;
  return 0;
}
