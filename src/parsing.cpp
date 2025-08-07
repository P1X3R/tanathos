#include "bitboard.h"
#include "board.h"
#include "legalMoves.h"
#include "sysifus.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

ChessBoard::ChessBoard(const std::string &fen) {
  whites = {};
  blacks = {};
  zobrist = 0;
  halfmoveClock = 0;
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
  halfmoveClock = std::stoi(fen.substr(pos, next - pos));
}

static auto getPieceAt(const std::uint32_t square, const ChessBoard &board)
    -> std::pair<Piece, bool> {
  const std::uint64_t bit = 1ULL << square;

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    if ((board.whites[type] & bit) != 0) {
      return std::make_pair(static_cast<Piece>(type), true);
    }
  }

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    if ((board.blacks[type] & bit) != 0) {
      return std::make_pair(static_cast<Piece>(type), false);
    }
  }

  return std::make_pair(Piece::NOTHING, false);
}

void insertMoveInfo(MoveCTX &partial, const ChessBoard &board,
                    const bool getOriginalType, bool isPieceWhite) {
  if (getOriginalType) {
    std::pair<Piece, bool> pieceInfo = getPieceAt(partial.from, board);
    partial.original = pieceInfo.first;
    isPieceWhite = pieceInfo.second;
  }

  // Get captured piece info depending on en passant
  const std::int32_t capturedPawnSquare =
      isPieceWhite ? board.enPassantSquare - BOARD_LENGTH
                   : board.enPassantSquare + BOARD_LENGTH;

  const bool isEnPassantCapture =
      partial.original == Piece::PAWN && board.enPassantSquare != 0 &&
      std::abs(partial.from - capturedPawnSquare) == 1 &&
      partial.to == board.enPassantSquare;

  if (isEnPassantCapture) {
    partial.capturedSquare = capturedPawnSquare;
    partial.captured = Piece::PAWN;
  } else {
    partial.capturedSquare = partial.to;
    partial.captured = getPieceAt(partial.to, board).first;
  }
}

auto fromAlgebraic(const std::string_view &algebraic, const ChessBoard &board)
    -> MoveCTX {
  MoveCTX ctx;

  // Get piece position and landing square
  const std::string_view fromCoordinates = algebraic.substr(0, 2);
  const std::string_view toCoordinates = algebraic.substr(2, algebraic.size());

  const std::uint32_t fromFile = fromCoordinates[0] - 'a';
  const std::uint32_t fromRank = fromCoordinates[1] - '1';

  const std::uint32_t toFile = toCoordinates[0] - 'a';
  const std::uint32_t toRank = toCoordinates[1] - '1';

  ctx.from = (fromRank * BOARD_LENGTH) + fromFile;
  ctx.to = (toRank * BOARD_LENGTH) + toFile;

  insertMoveInfo(ctx, board, true, false);

  // Get promotions
  static constexpr std::uint32_t algebraicLengthIfPromotion = 5;
  if (algebraic.size() == algebraicLengthIfPromotion) {
    switch (algebraic[4]) {
    case 'n':
      ctx.promotion = Piece::KNIGHT;
      break;
    case 'b':
      ctx.promotion = Piece::BISHOP;
      break;
    case 'r':
      ctx.promotion = Piece::ROOK;
      break;
    case 'q':
      ctx.promotion = Piece::QUEEN;
      break;
    default:
      ctx.promotion = Piece::NOTHING;
      break;
    }
  }

  return ctx;
}

auto moveToUCI(const MoveCTX &move) -> std::string {
  const Coordinate fromCoord = {
      .rank = static_cast<std::int8_t>(move.from / BOARD_LENGTH),
      .file = static_cast<std::int8_t>(move.from % BOARD_LENGTH),
  };

  const Coordinate toCoord = {
      .rank = static_cast<std::int8_t>(move.to / BOARD_LENGTH),
      .file = static_cast<std::int8_t>(move.to % BOARD_LENGTH),
  };

  std::string result = {static_cast<char>('a' + fromCoord.file),
                        static_cast<char>('1' + fromCoord.rank),
                        static_cast<char>('a' + toCoord.file),
                        static_cast<char>('1' + toCoord.rank)};

  if (move.promotion != Piece::NOTHING) {
    static const std::array<char, Piece::KING + 1> pieceTypeCharacters = {
        ' ', 'n', 'b', 'r', 'q', ' '};
    result += pieceTypeCharacters[move.promotion];
  }

  return result;
}

