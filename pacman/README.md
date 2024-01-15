# Pacman on terminal

## Goals
- Build a graph, with valid paths
- Create ghosts that move on the graph
- Create pacman that moves based on your input

## Design
- Gameboard exists
- Player and ghosts exist on Gameboard
- Each player and ghost has positions
- player and ghosts are both just gaemobjs
- diff in player and ghost is that ghost moves by itself
- player moves by user input
- player input is taken on separate thread
- ghost AI is run on a separate thread
- gameboard is redrawn on screen on an inerval -> N/sec, where N is frame rate
