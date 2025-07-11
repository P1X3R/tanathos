#include "bitboard.h"
#include "board.h"
#include <string>

ChessBoard::ChessBoard(const std::string &fen) {
  zobrist = 0;
  halfmoveCounter = 0;
  enPassantSquare = 0;
  whiteToMove = true;
  castlingRights = {
      .whiteKingSide = false,
      .whiteQueenSide = false,
      .blackKingSide = false,
      .blackQueenSide = false,
  };

  // Split FEN into components
  std::size_t pos = 0;
  auto next = fen.find(' ');

  // Parse piece placement
  std::string placement = fen.substr(0, next);
  std::uint32_t square =
      BOARD_AREA - BOARD_LENGTH; // Start from a8 (rank 8, file a)

  for (char character : placement) {
    if (character == '/') {
      square -= BOARD_LENGTH * 2; // Move to next rank
      continue;
    }
    if (std::isdigit(character) != 0) {
      square += character - '0'; // Skip empty squares
      continue;
    }

    std::uint64_t bit = 1ULL << square;
    switch (character) {
    case 'P':
      whites[Piece::PAWN] |= bit;
      break;
    case 'N':
      whites[Piece::KNIGHT] |= bit;
      break;
    case 'B':
      whites[Piece::BISHOP] |= bit;
      break;
    case 'R':
      whites[Piece::ROOK] |= bit;
      break;
    case 'Q':
      whites[Piece::QUEEN] |= bit;
      break;
    case 'K':
      whites[Piece::KING] |= bit;
      break;
    case 'p':
      blacks[Piece::PAWN] |= bit;
      break;
    case 'n':
      blacks[Piece::KNIGHT] |= bit;
      break;
    case 'b':
      blacks[Piece::BISHOP] |= bit;
      break;
    case 'r':
      blacks[Piece::ROOK] |= bit;
      break;
    case 'q':
      blacks[Piece::QUEEN] |= bit;
      break;
    case 'k':
      blacks[Piece::KING] |= bit;
      break;
    default:
      break;
    }
    square++;
  }

  // Parse active color
  pos = next + 1;
  next = fen.find(' ', pos);
  whiteToMove = (fen[pos] == 'w');

  // Parse castling availability
  pos = next + 1;
  next = fen.find(' ', pos);
  std::string castling = fen.substr(pos, next - pos);
  for (char character : castling) {
    switch (character) {
    case 'K':
      castlingRights.whiteKingSide = true;
      break;
    case 'Q':
      castlingRights.whiteQueenSide = true;
      break;
    case 'k':
      castlingRights.blackKingSide = true;
      break;
    case 'q':
      castlingRights.blackQueenSide = true;
      break;
    default:
      break;
    }
  }

  // Parse en passant target square
  pos = next + 1;
  next = fen.find(' ', pos);
  std::string enPassant = fen.substr(pos, next - pos);
  if (enPassant != "-") {
    std::uint32_t file = enPassant[0] - 'a';
    std::uint32_t rank = enPassant[1] - '1';
    enPassantSquare = rank * BOARD_LENGTH + file;
  }

  // Parse halfmove clock
  pos = next + 1;
  next = fen.find(' ', pos);
  halfmoveCounter = std::stoi(fen.substr(pos, next - pos));
}
