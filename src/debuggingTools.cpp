#include "bitboard.h"
#include "board.h"
#include "legalMoves.h"
#include "sysifus.h"
#include <cstdint>
#include <iostream>

static auto getPieceSymbol(const ChessBoard &board, int squareIndex) -> char {
  std::uint64_t squareMask = 1ULL << squareIndex;

  // Check for white pieces
  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    if ((board.whites[type] & squareMask) != 0) {
      switch (type) {
      case Piece::PAWN:
        return 'P';
      case Piece::KNIGHT:
        return 'N';
      case Piece::BISHOP:
        return 'B';
      case Piece::ROOK:
        return 'R';
      case Piece::QUEEN:
        return 'Q';
      case Piece::KING:
        return 'K';
      default:
        break;
      }
    }
  }

  // Check for black pieces
  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    if ((board.blacks[type] & squareMask) != 0) {
      switch (type) {
      case Piece::PAWN:
        return 'p';
      case Piece::KNIGHT:
        return 'n';
      case Piece::BISHOP:
        return 'b';
      case Piece::ROOK:
        return 'r';
      case Piece::QUEEN:
        return 'q';
      case Piece::KING:
        return 'k';
      default:
        break;
      }
    }
  }

  return '.'; // Empty square
}

void printChessBoard(const ChessBoard &board) {
  // Print the header with file labels (a-h)
  std::cout << "  +---+---+---+---+---+---+---+---+" << '\n';
  for (int rank = BOARD_LENGTH - 1; rank >= 0; --rank) {
    // Print the rank number on the left
    std::cout << rank + 1 << " | ";
    for (int file = 0; file < BOARD_LENGTH; ++file) {
      // Convert rank and file to a 0-63 square index
      int squareIndex = (rank * BOARD_LENGTH) + file;
      char piece = getPieceSymbol(board, squareIndex);
      std::cout << piece << " | ";
    }
    std::cout << '\n' << "  +---+---+---+---+---+---+---+---+" << '\n';
  }
  // Print the footer with file labels (a-h)
  std::cout << "    a   b   c   d   e   f   g   h  " << '\n';
}

auto operator<<(std::ostream &ostrm, Piece piece) -> std::ostream & {
  switch (piece) {
  case Piece::PAWN:
    return ostrm << "PAWN";
  case Piece::KNIGHT:
    return ostrm << "KNIGHT";
  case Piece::BISHOP:
    return ostrm << "BISHOP";
  case Piece::ROOK:
    return ostrm << "ROOK";
  case Piece::QUEEN:
    return ostrm << "QUEEN";
  case Piece::KING:
    return ostrm << "KING";
  case Piece::NOTHING:
    return ostrm << "NOTHING";
  default:
    return ostrm << "UNKNOWN";
  }
}

void printMoveCTX(const MoveCTX &move) {
  std::cout << "MoveCTX from:" << move.from << " to:" << move.to
            << " capturedSquare:" << move.capturedSquare
            << " original:" << move.original << " captured:" << move.captured
            << " promotion:" << move.promotion << '\n';
}