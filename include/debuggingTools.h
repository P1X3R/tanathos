#include "board.h"
#include <iostream>

inline void printBitboard(const std::uint64_t bitboard) {
  std::cout << "  A B C D E F G H\n";
  for (int8_t rank = BOARD_LENGTH - 1; rank >= 0; rank--) {
    std::cout << rank + 1 << ' ';
    for (int8_t file = 0; file < BOARD_LENGTH; file++) {
      const bool isBitSet =
          (bitboard & (1ULL << ((rank * BOARD_LENGTH) + file))) != 0;
      std::cout << (isBitSet ? '#' : '.') << ' ';
    }
    std::cout << '\n';
  }
}

/**
 * @brief Prints a representation of the chessboard to the console.
 *
 * This function iterates through all 64 squares of the board, from rank 8 down
 * to 1 and file A to H. For each square, it determines if a piece occupies it
 * by checking the piece bitboards in the `ChessBoard` struct. It prints a
 * character representation of the piece (uppercase for white, lowercase for
 * black) or a period for an empty square.
 *
 * @param board The ChessBoard struct containing the piece bitboards.
 */
void printChessBoard(const ChessBoard &board);
