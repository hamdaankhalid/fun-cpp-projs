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

const char PACMAN    = 'O';
const char COLUMN    = 'I';
const char ADD_GHOST = ' ';
const char QUIT      = 'q';
const char UP_CMD    = 'w';
const char DOWN_CMD  = 's';
const char LEFT_CMD  = 'a';
const char RIGHT_CMD = 'd';

// Decides how many FPS to update
const int FRAME                              = 100000;
const int SECOND                             = 1000000;
const int NON_BLOCKING_EVENT_LOOP_INPUT_POLL = 0;

// making it a class to avoid reconstructing distribution on every call
class RandGen {
    public:
    RandGen (int lower, int upper);
    int getRandomInt ();

    private:
    std::unique_ptr< std::random_device > dev;
    std::unique_ptr< std::mt19937 > gen;
    std::unique_ptr< std::uniform_int_distribution< int > > distribution;
};

RandGen::RandGen (int lower, int upper) {
    // Create a random device to obtain seed
    this->dev = std::make_unique< std::random_device > ();

    // Create a random number generator using the seed from the random device
    this->gen = std::make_unique< std::mt19937 > ((*this->dev) ());

    // Define the distribution for integers in the range [lower, upper]
    this->distribution =
    std::make_unique< std::uniform_int_distribution< int > > (lower, upper);
}

int RandGen::getRandomInt () {
    // Generate a random integer
    return (*this->distribution) (*this->gen);
}

void runCountdown (int i) {
    system ("clear");
    for (int j = i; j >= 0; j--) {
        std::cout << j << " seconds to go!" << std::endl;
        usleep (SECOND);
        system ("clear");
    }
}

enum Direction {
    UP,
    DOWN,
    RIGHT,
    LEFT,
    NOOP,
};

struct Input {
    int moverId;
    Direction dir;
};

struct Position {
    int x;
    int y;
    Direction dir;
};

enum CollisionValidation {
    NOCOLLISION,
    COLUMNCOL,
    PACMANCOL,
    MOVABLECOL,
};

// forward decl
class Ghost;

class Gameboard {
    public:
    Gameboard (int rows, int cols);
	void drawWalls(int percentage);
    void draw (std::vector< std::unique_ptr< Input > >& updates);
    int insertMovable ();

    private:
    std::vector< std::vector< char > > board;
    int rows;
    int cols;
    int pidCounter;
    std::unordered_map< int, std::unique_ptr< Position > > movables;
    std::unique_ptr< RandGen > rowRandGen;
    std::unique_ptr< RandGen > colRandGen;

    std::pair< int, int > getCurrPos (int pid);

    // update and get prev position
    std::pair< int, int > updateMovable (const Input& input);

    std::pair< std::pair< int, int >, Direction >
    validateMoveBoundary (std::pair< int, int >& currPos, Direction dir);
    CollisionValidation validateCollision (std::pair< int, int >& currPos,
    std::pair< int, int >& offset);

    friend class Ghost;
};

std::pair< int, int > Gameboard::getCurrPos (int pid) {
    const Position& pos = *this->movables[pid];
    return std::make_pair (pos.y, pos.x);
}

Gameboard::Gameboard (int rows, int cols)
: rows (rows), cols (cols), pidCounter (0),
  rowRandGen (std::make_unique< RandGen > (0, rows - 1)),
  colRandGen (std::make_unique< RandGen > (0, cols - 1)) {
	// construct graph super simple
    for (int i = 0; i < rows; i++) {
        std::vector< char > row (cols, ' ');
        this->board.push_back (row);
    }
}

// This would be nice if we made it into mazes
void Gameboard::drawWalls(int percentage) {
	RandGen rg(1, 100);
	for (int i = 0; i < this->rows; i++) {
		for (int j = 0; j < this->cols; j++) {
			if (i != 0 && j != 0 && rg.getRandomInt() < percentage) {
				this->board[i][j] = COLUMN;			
			}
		}
	}
}

const char ghostDir[4] = { 'v', '^', '<', '>' };

void Gameboard::draw (std::vector< std::unique_ptr< Input > >& updates) {
    // go over the updates, make the updates and mark the inverse as empty
    for (auto i = 0u; i < updates.size (); i++) {
        const Input& update           = *updates[i];
        std::pair< int, int > prevPos = this->updateMovable (update);
        // we know the pos on board will never go to -1 unless the move was invalid
        if (prevPos.first == -1) {
            continue;
        }
        // when someone has passed over the cell is now empty
        this->board[prevPos.first][prevPos.second] = ' ';
    }

    for (int i = 0; i < pidCounter; i++) {
        const Position& pos = *this->movables[i];
        char repr;
        if (i == 0) {
            repr = PACMAN;
        } else {
            repr = ghostDir[pos.dir]; // based on the dir we should get the ghost char
        }
        this->board[pos.y][pos.x] = repr;
    }

    /*
     * Reduce System calls to print the board by calling
     * cout on string repr only once
     * [.,O,.]
     * [^,.,.]
     * [.,.,<]
     *
     * Func (vec<vec<char>>) -> str
     *
     * str format
     *
     * ._O_.\n
     * ^_._.\n
     * ._._<\n
     * ._._.\n
	 *
     * ._O_.(\n)^_._.(\n)._._.(\n)
	 * 8 * 10 * 2 = 160
	 * 8 rows 10 columns
	 * 0..20..40..60..80..100..120..140
     * */

	// TODO: THIS IS STILL FUCKED UP
    int strBoardSize = this->rows * this->cols * 2; // 2 represents the spaces and EOL
    std::string strBoard (strBoardSize, ' ');

    for (int i = 0; i < strBoardSize; i++) {
		std::cout << i << std::endl;
        int row = (i / 2) / this->rows;
        int col = (i / 2) % this->cols;

        // newline
        if (i % 2 == 0 && col == this->cols-1) {
			std::cout << "newline" << std::endl;
            strBoard[i] = '\n';
            continue;
        }

        // space
        if (i % 2 != 0) {
			std::cout << "space" << std::endl;
            continue;
        }

		std::cout << row << ", " << col << std::endl;

        strBoard[i] = this->board[row][col];
    }

    std::cout << strBoard << std::endl;
}

int Gameboard::insertMovable () {
    int row = 0;
    int col = 0;

    if (this->pidCounter != 0) {
        row = this->rowRandGen->getRandomInt ();
        col = this->colRandGen->getRandomInt ();
    }

    this->movables[this->pidCounter] =
    std::make_unique< Position > (Position{ row, col, NOOP });
    this->pidCounter++;

    // the movable Id for the newly inserted movable
    return this->pidCounter - 1;
}

std::pair< std::pair< int, int >, Direction >
Gameboard::validateMoveBoundary (std::pair< int, int >& currPos, Direction dir) {
    Direction res                = NOOP;
    std::pair< int, int > offset = std::make_pair (0, 0);
    // Bounds checking
    switch (dir) {
    case UP:
        if (currPos.first <= 0)
            return std::make_pair (offset, res);
        offset.first--;
        res = UP;
        break;
    case DOWN:
        if (currPos.first >= rows - 1)
            return std::make_pair (offset, res);
        offset.first++;
        res = DOWN;
        break;
    case LEFT:
        if (currPos.second <= 0)
            return std::make_pair (offset, res);
        offset.second--;
        res = LEFT;
        break;
    case RIGHT:
        if (currPos.second >= cols - 1)
            return std::make_pair (offset, res);
        offset.second++;
        res = RIGHT;
        break;
    default: return std::make_pair (offset, res);
    }

    return std::make_pair (offset, res);
}

CollisionValidation Gameboard::validateCollision (std::pair< int, int >& currPos,
std::pair< int, int >& offset) {
    std::pair< int, int > newPos =
    std::make_pair (currPos.first + offset.first, currPos.second + offset.second);
    char tile = this->board[newPos.first][newPos.second];
    if (tile == ' ' || tile == '.') {
        return NOCOLLISION;
    } else if (tile == PACMAN) {
        return PACMANCOL;
    } else if (tile == COLUMN) {
        return COLUMNCOL;
    }
    return MOVABLECOL;
}

std::pair< int, int > Gameboard::updateMovable (const Input& input) {
    // Take reference of movable pos, without taking ownership of it
    Position& movablePos          = *this->movables[input.moverId];
    std::pair< int, int > initPos = std::make_pair (movablePos.y, movablePos.x);

    std::pair< std::pair< int, int >, Direction > validationRes =
    this->validateMoveBoundary (initPos, input.dir);

    if (validationRes.second == NOOP) {
        return std::make_pair< int, int > (-1, -1);
    }

    std::pair< int, int > offset = validationRes.first;
    CollisionValidation collValidation = this->validateCollision (initPos, offset);
    if (collValidation == COLUMNCOL) {
        // cannot proceed
        return std::make_pair< int, int > (-1, -1);
    } else if (collValidation == PACMANCOL && input.moverId != 0) {
        // movable ghost collided into pacman!
        std::cout << "Ghost caught pacman!\n";
    } else if (collValidation == MOVABLECOL && input.moverId == 0) {
        // pacman ran into ghost!
        std::cout << "Pacman caught ghost!\n";
    }

    movablePos.dir = input.dir;

    movablePos.y += offset.first;
    movablePos.x += offset.second;

    return initPos;
}

class Ghost {
    public:
    Ghost (int id);
    std::unique_ptr< Input > getNextMove (Gameboard& gb);

    private:
    int id;
    Direction lastMove;
    std::unique_ptr< RandGen > rg;
    std::unique_ptr< RandGen > randomDirRg;
};

const Direction dirFrom[4] = { UP, DOWN, LEFT, RIGHT };

Ghost::Ghost (int id) : id (id) {
	this->rg = std::make_unique< RandGen > (0, 3);
	this->randomDirRg =std::make_unique< RandGen > (1, 100); 
    this->lastMove = dirFrom[this->rg->getRandomInt ()];
}

const int RANDOM_MOVE_PERCENTAGE = 30;
std::unique_ptr< Input > Ghost::getNextMove (Gameboard& gb) {
    while (true) {
        Direction moveToMake = this->lastMove;
		// Either this is the first move or this is the RANDOM MOVE PERFECNTAGE of the time situation where ghost turns randomly
        if (this->lastMove == NOOP || this->randomDirRg->getRandomInt() > (100 - RANDOM_MOVE_PERCENTAGE)) {
			moveToMake = dirFrom[this->rg->getRandomInt()];
        }		

        // would making the move be a valid move on the board? 
		// if it would not give me a random diff dir

        Input ghostInput {this->id, moveToMake};

        std::pair< int, int > currPos = gb.getCurrPos (this->id);

		std::pair< std::pair<int, int>, Direction> res = gb.validateMoveBoundary (currPos, moveToMake);

		if (res.second != NOOP) {
			// collision based checking for columns if the move 
			// doesn't lead us into the column we are okay to proceed
			CollisionValidation collValidation = gb.validateCollision (currPos, res.first);
			if (collValidation != COLUMNCOL) {
				this->lastMove = moveToMake;
				return std::make_unique< Input > (std::move (ghostInput));
			}
        }

	
        this->lastMove = dirFrom[this->rg->getRandomInt ()];
    }
}

// get updates from user using eventloop style IO multiplexing, return the number of ghosts wanted to be updated
int handleFakeInterrupt (struct pollfd fds[], std::vector< std::unique_ptr< Input > >& buf) {
    // "1" specifies size of fds
    int result = poll (fds, 1, NON_BLOCKING_EVENT_LOOP_INPUT_POLL);
    // some FD has an event for us
    if (result > 0) {
        // based on which FD it is, we map to the player and move the player
        if (fds[0].revents & POLLIN) {
            char buffer[256];
            ssize_t bytesRead = read (STDIN_FILENO, buffer, sizeof (buffer));

            if (bytesRead > 0) {
				int ghostsAdded = 0;
                for (int i = 0; i < bytesRead; i++) {
                    std::unique_ptr< Input > userInput = nullptr;
                    switch (buffer[i]) {
                    case UP_CMD:
                        userInput = std::make_unique< Input > (Input{ 0, UP });
                        break;
                    case DOWN_CMD:
                        userInput = std::make_unique< Input > (Input{ 0, DOWN });
                        break;
                    case LEFT_CMD:
                        userInput = std::make_unique< Input > (Input{ 0, LEFT });
                        break;
                    case RIGHT_CMD:
                        userInput = std::make_unique< Input > (Input{ 0, RIGHT });
                        break;
					case ADD_GHOST:
						ghostsAdded++;
						break;
                    case QUIT: return -1;
                    default: break;
                    }
                    if (userInput != nullptr) {
                        buf.push_back (std::move (userInput));
                    }
                }
                return ghostsAdded;
            }
        }
    }

    return 0;
}

// RAII wrapper to restore state of terminal
class TerminalInputConfigManager {
    public:
    TerminalInputConfigManager () {
        // save original terminal state for restoring later
        this->originalTerminalAttr = std::make_unique< struct termios > ();
        tcgetattr (STDIN_FILENO, this->originalTerminalAttr.get ());
    }

    int useRawInput () {
        struct termios t;
        if (tcgetattr (STDIN_FILENO, &t) < 0) {
            return -1;
        }

        t.c_lflag &= ~(ICANON | ECHO);
        t.c_cc[VMIN]  = 0;
        t.c_cc[VTIME] = 0;

        if (tcsetattr (STDIN_FILENO, TCSANOW, &t) < 0) {
            return -1;
        }

        return 0;
    }

    ~TerminalInputConfigManager () {
        tcsetattr (STDIN_FILENO, TCSANOW, this->originalTerminalAttr.get ());
    }

    private:
    std::unique_ptr< struct termios > originalTerminalAttr;
};

int main () {
    TerminalInputConfigManager cm;
    if (cm.useRawInput () < 0) {
        return -1;
    }

    // monitor FD's for user using IO multiplexing
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    // monitor FD for standard in
    fds[0].events = POLLIN;

    // use the same vector to fill and drain
    std::vector< std::unique_ptr< Input > > gameplayInstructionBuffer;

    // setup gameboard
    Gameboard gb (2, 3);

    // add base player
    gb.insertMovable ();
	gb.drawWalls(10);

    // add two ghosts
    int ghost1Id = gb.insertMovable ();
	
    // Make this a vector so we can add ghosts at runtime?
	std::vector<Ghost> ghosts;
	ghosts.push_back(Ghost (ghost1Id));

    // TODO: runCountdown (2);
    bool moveGhost = false;
    // main Gameloop
    while (true) {
        // shitty hack to slow the ghosts down?
        if (moveGhost) {
            for (auto i = 0u; i < ghosts.size(); i++) {
                gameplayInstructionBuffer.push_back (ghosts[i].getNextMove (gb));
            }
        }
        moveGhost = !moveGhost;

		int ghostsAdded = handleFakeInterrupt (fds, gameplayInstructionBuffer);
		// less than 0 means quit was pressed
        if (ghostsAdded < 0) {
            // user wanted to exit the game
            break;
        }

		for (int i = 0; i < ghostsAdded; i++) {
			ghosts.push_back(Ghost (gb.insertMovable ()));
		}

        gb.draw (gameplayInstructionBuffer);

        gameplayInstructionBuffer.clear ();

        usleep (FRAME);

        // clear screen	by handing command to shell
        system ("clear");
    }

    std::cout << "Thanks for playing!" << std::endl;
    std::cout << "~ Hamdaan Khalid" << std::endl;

    return 0;
}
