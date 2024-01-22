#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <queue>
#include <random>
#include <sys/fcntl.h>   // for making stdin non-blocking
#include <sys/poll.h>    // IO multiplexing
#include <sys/termios.h> // interacting with terminal
#include <tuple>
#include <unistd.h> // For usleep function
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

const char PACMAN    = 'O';
const char COLUMN    = 'I';
const char ADD_GHOST = ' '; // spacebar
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

    // delete copy and move
    RandGen (const RandGen&)            = delete;
    RandGen& operator= (const RandGen&) = delete;
    RandGen (RandGen&&)                 = delete;
    RandGen& operator= (RandGen&&)      = delete;

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
    UP    = 0,
    DOWN  = 1,
    RIGHT = 2,
    LEFT  = 3,
    NOOP  = 4,
};

struct Input {
    int moverId;
    Direction dir;
    Input (int id, Direction iDir) : moverId (id), dir (iDir){};
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

enum GameNotification {
    CAUGHT,
    GHOSTADDED,
};

class ScoreKeeper {
    public:
    ScoreKeeper () : numGhosts (0), timesCaught (0) {
    }

    void notify (GameNotification notif) {
        switch (notif) {
        case CAUGHT: timesCaught++; break;
        case GHOSTADDED: numGhosts++; break;
        default: break;
        }
    }

    void displayScore () {
        std::cout << "Ghosts On Screen: " << numGhosts << "\n"
                  << "Times Caught: " << timesCaught << "\n";
    }

    private:
    int numGhosts;
    int timesCaught;
};

// forward decl
class Ghost;

class Gameboard {
    public:
    Gameboard (int rows, int cols, ScoreKeeper& scoreKeeper);
    void drawWalls (int percentage);
    void draw (std::vector< std::unique_ptr< Input > >& updates);
    int insertMovable ();

    private:
    std::vector< std::vector< char > > board;
    int rows;
    int cols;
    int pidCounter;
    std::vector< std::unique_ptr< Position > > movables;
    std::unique_ptr< RandGen > rowRandGen;
    std::unique_ptr< RandGen > colRandGen;
    // just hold a reference because this was allocated on stack and DI'd
    ScoreKeeper& keeper;

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

Gameboard::Gameboard (int rows, int cols, ScoreKeeper& scoreKeeper)
: rows (rows), cols (cols), pidCounter (0),
  rowRandGen (std::make_unique< RandGen > (0, rows - 1)),
  colRandGen (std::make_unique< RandGen > (0, cols - 1)), keeper (scoreKeeper) {
    // construct graph super simple
    for (int i = 0; i < rows; i++) {
        std::vector< char > row (cols, ' ');
        this->board.push_back (row);
    }
}

// This would be nice if we made it into mazes
void Gameboard::drawWalls (int percentage) {
    RandGen decisionRg (1, 100);

    for (int i = 0; i < this->rows; i++) {
        for (int j = 0; j < this->cols; j++) {
            if (i != 0 && j != 0 && decisionRg.getRandomInt () < percentage) {
                this->board[i][j] = COLUMN;
            }
        }
    }
}

const char ghostDir[5] = { 'v', '^', '<', '>', '<' };

void Gameboard::draw (std::vector< std::unique_ptr< Input > >& updates) {
    // go over the updates, make the updates and mark the inverse as empty
    for (auto i = 0u; i < updates.size (); i++) {
        const Input& update           = *updates[i];
        std::pair< int, int > prevPos = this->updateMovable (update);
        // we know the pos on board will never go to -1 unless the move was invalid
        if (prevPos.first == -1 || prevPos.second == -1) {
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
            // based on the dir we should get the ghost char
            repr = ghostDir[pos.dir];
        }

        this->board[pos.y][pos.x] = repr;
    }

    int strBoardSize = this->rows * this->cols;
    std::string strBoard;
    strBoard.reserve (strBoardSize);
    for (int i = 0; i < this->rows; i++) {
        for (int j = 0; j < this->cols; j++) {
            strBoard.push_back (this->board[i][j]);
        }
        strBoard.push_back ('\n');
    }

    std::cout << strBoard;
}

int Gameboard::insertMovable () {
    int row = 0;
    int col = 0;

    if (this->pidCounter != 0) {
        row = this->rowRandGen->getRandomInt ();
        col = this->colRandGen->getRandomInt ();
    }

    this->movables.push_back (std::make_unique< Position > (Position{ col, row, NOOP }));
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
    if (tile == ' ') {
        return NOCOLLISION;
    } else if (tile == PACMAN) {
        return PACMANCOL;
    } else if (tile == COLUMN) {
        return COLUMNCOL;
    }

    return MOVABLECOL;
}

std::pair< int, int > Gameboard::updateMovable (const Input& input) {
    // Take mutable reference of movable pos, without taking ownership of it
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
        this->keeper.notify (CAUGHT);
    } else if (collValidation == MOVABLECOL && input.moverId == 0) {
        // pacman ran into ghost!
        this->keeper.notify (CAUGHT);
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
    this->rg          = std::make_unique< RandGen > (0, 3);
    this->randomDirRg = std::make_unique< RandGen > (1, 100);
    this->lastMove    = dirFrom[this->rg->getRandomInt ()];
}

struct BfsNode {
    std::pair< int, int > currPos;
    int level;
    Direction initDirection;
};

const int RANDOM_MOVE_PERCENTAGE = 15;

// I have plagiarized this hasher from chatgpt
// Hash function for std::pair<int, int>
struct pair_hash {
    template < class T1, class T2 >
    std::size_t operator() (const std::pair< T1, T2 >& p) const {
        auto hash1 = std::hash< T1 >{}(p.first);
        auto hash2 = std::hash< T2 >{}(p.second);

        // Simple hash combining algorithm
        // You can replace this with a more sophisticated hash function if needed
        return hash1 ^ hash2;
    }
};

// From 5 moves away the ghost will start chasing you!
const int BFS_DEPTH_GHOST = 5;

std::unique_ptr< Input > Ghost::getNextMove (Gameboard& gb) {
    // check if Pacman is nearby and if we can move towards the fucker
    std::pair< int, int > currPos = gb.getCurrPos (this->id);
    // see if pacman is present anywhere close to us using size limited BFS
    // hydrate the queue
    std::unordered_set< std::pair< int, int >, pair_hash > visited;
    std::queue< BfsNode > q;
    q.push (BfsNode{ currPos, 0, NOOP });
    // mark starting point as visited
    visited.insert (currPos);

    while (!q.empty ()) {
        BfsNode curr = q.front ();
        q.pop ();

        // apply the offsets, see which one's are valid, see which one's result in us finding pacman
        // iterate over enums UP, DOWN, LEFT, RIGHT using their int repr
        for (int i = 0; i < 4; i++) {
            std::pair< std::pair< int, int >, Direction > boundaryValidation =
            gb.validateMoveBoundary (curr.currPos, (Direction)i);
            if (boundaryValidation.second == NOOP) {
                continue;
            }

            CollisionValidation collValidation =
            gb.validateCollision (curr.currPos, boundaryValidation.first);
            // either we cant move or cannot explore further in depth
            if (collValidation == COLUMNCOL) {
                continue;
            }

            // init direction is used to keep a track of the very first move we made from pacman for this exploration
            if (curr.level == 0) {
                curr.initDirection = (Direction)i;
            }

            if (collValidation == PACMANCOL) {
				this->lastMove = curr.initDirection;
                return std::make_unique< Input > (this->id, curr.initDirection);
            }

            std::pair< int, int > offset  = boundaryValidation.first;
            std::pair< int, int > nextPos = { curr.currPos.first + offset.first,
                curr.currPos.second + offset.second };
            // only proceed if we haven't visited nextPos before
            if (visited.find (nextPos) != visited.end () || curr.level == BFS_DEPTH_GHOST) {
                continue;
            }
            q.push (BfsNode{ nextPos, curr.level + 1, curr.initDirection });
        }
    }

    // Randome Exploration
    while (true) {
        // Either this is the first move or this is the RANDOM MOVE PERFECNTAGE of the time situation where ghost turns randomly
        Direction moveToMake = this->lastMove;
        if (this->lastMove == NOOP ||
        this->randomDirRg->getRandomInt () > (100 - RANDOM_MOVE_PERCENTAGE)) {
            moveToMake = dirFrom[this->rg->getRandomInt ()];
        }

        // would making the move be a valid move on the board?
        // if it would not give me a random diff dir

        Input ghostInput{ this->id, moveToMake };

        std::pair< std::pair< int, int >, Direction > res =
        gb.validateMoveBoundary (currPos, moveToMake);

        if (res.second != NOOP) {
            // collision based checking for columns if the move
            // doesn't lead us into the column we are okay to proceed
            CollisionValidation collValidation =
            gb.validateCollision (currPos, res.first);
            if (collValidation != COLUMNCOL) {
                this->lastMove = moveToMake;

                assert (moveToMake != NOOP);

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
                    case ADD_GHOST: ghostsAdded++; break;
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

void displayInstructions () {
    std::cout << "---- Game Instructions ---- \n"
              << "spacebar -> add a ghost \n"
              << "q        -> EXIT        \n"
              << "w        -> UP          \n"
              << "a        -> LEFT        \n"
              << "s        -> DOWN        \n"
              << "d        -> RIGHT       \n"
              << "PRESS ENTER TO START!" << std::endl;
    std::cin.ignore (std::numeric_limits< std::streamsize >::max (), '\n');
}

int main () {
    displayInstructions ();

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
    int rows = 20;
    int cols = 40;

    // because score lives for the lifetime	of the program we can keep it on the stack
    ScoreKeeper score;

    Gameboard gb (rows, cols, score);

    // add base player
    gb.insertMovable ();
    gb.drawWalls (5);

    // Make this a vector so we can add ghosts at runtime?
    std::vector< Ghost > ghosts;
    ghosts.push_back (Ghost (gb.insertMovable ()));
    score.notify (GHOSTADDED);

    runCountdown (3);

    bool moveGhost = true;
    // main Gameloop
    while (true) {
        // shitty hack to slow the ghosts down?
        if (moveGhost) {
            for (int i = 0; i < (int)ghosts.size (); i++) {
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

        gb.draw (gameplayInstructionBuffer);

        gameplayInstructionBuffer.clear ();

        score.displayScore ();

        for (int i = 0; i < ghostsAdded; i++) {
            ghosts.push_back (Ghost (gb.insertMovable ()));
            score.notify (GHOSTADDED);
        }

        usleep (FRAME);

        // clear screen	by handing command to shell
        system ("clear");
    }

    std::cout << "Thanks for playing!" << std::endl;
    std::cout << "~ Hamdaan Khalid" << std::endl;

    return 0;
}
