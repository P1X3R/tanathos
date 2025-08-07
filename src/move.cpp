#include "move.h"
#include "bitboard.h"
#include "board.h"
#include "legalMoves.h"
#include "sysifus.h"
#include "zobrist.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

static void
moveRookIfCastling(std::array<std::uint64_t, Piece::KING + 1> &color,
                   ChessBoard &board, const MoveCTX &ctx) {
  const bool isKingSide =
      board.whiteToMove ? ctx.to == BoardSquare::G1 : ctx.to == BoardSquare::G8;
  std::uint32_t fromRook = 0;
  std::uint32_t toRook = 0;

  if (board.whiteToMove) {
    fromRook = isKingSide ? BoardSquare::H1 : BoardSquare::A1;
    toRook = isKingSide ? BoardSquare::F1 : BoardSquare::D1;
  } else {
    fromRook = isKingSide ? BoardSquare::H8 : BoardSquare::A8;
    toRook = isKingSide ? BoardSquare::F8 : BoardSquare::D8;
  }

  color[Piece::ROOK] ^= (1ULL << fromRook) | (1ULL << toRook);
  board.zobrist ^= ZOBRIST_PIECE[board.whiteToMove][Piece::ROOK][fromRook] ^
                   ZOBRIST_PIECE[board.whiteToMove][Piece::ROOK][toRook];
}

void movePieceToDestination(ChessBoard &board, const MoveCTX &ctx) {
  std::array<std::uint64_t, Piece::KING + 1> &color =
      board.whiteToMove ? board.whites : board.blacks;

  color[ctx.original] &= ~(1ULL << ctx.from);

  const Piece final =
      ctx.promotion != Piece::NOTHING ? ctx.promotion : ctx.original;

  color[final] |= 1ULL << ctx.to;

  board.zobrist ^= ZOBRIST_PIECE[board.whiteToMove][ctx.original][ctx.from] ^
                   ZOBRIST_PIECE[board.whiteToMove][final][ctx.to];

  const bool isCastling =
      ctx.original == KING && std::abs(ctx.to - ctx.from) == 2;
  if (isCastling) {
    moveRookIfCastling(color, board, ctx);
  }

  if (ctx.captured != Piece::NOTHING) {
    std::array<std::uint64_t, Piece::KING + 1> &enemyColor =
        board.whiteToMove ? board.blacks : board.whites;

    enemyColor[ctx.captured] &= ~(1ULL << ctx.capturedSquare);

    board.zobrist ^= ZOBRIST_PIECE[static_cast<std::size_t>(!board.whiteToMove)]
                                  [ctx.captured][ctx.capturedSquare];
  }
}

static void updateEnPassantSquare(ChessBoard &board, const MoveCTX &ctx) {
  // Clear old en passant zobrist
  if (board.enPassantSquare != 0) {
    board.zobrist ^=
        ZOBRIST_EN_PASSANT_FILE[board.enPassantSquare % BOARD_LENGTH];
  }

  board.enPassantSquare = 0;
  const bool isDoublePush = ctx.original == Piece::PAWN &&
                            std::abs(ctx.to - ctx.from) == BOARD_LENGTH * 2;
  if (isDoublePush) {
    board.enPassantSquare = (ctx.from + ctx.to) / 2;
    board.zobrist ^=
        ZOBRIST_EN_PASSANT_FILE[board.enPassantSquare % BOARD_LENGTH];
  }
}

static void updateCastlingByRook(ChessBoard &board,
                                 const std::uint32_t square) {
  switch (square) {
  case BoardSquare::H1:
    board.castlingRights.whiteKingSide = false;
    break;
  case BoardSquare::H8:
    board.castlingRights.blackKingSide = false;
    break;
  case BoardSquare::A1:
    board.castlingRights.whiteQueenSide = false;
    break;
  case BoardSquare::A8:
    board.castlingRights.blackQueenSide = false;
    break;
  default:
    break;
  }
}

static void updateCastlingRightsByMove(ChessBoard &board, const MoveCTX &ctx) {
  const std::uint64_t initialZobrist =
      ZOBRIST_CASTLING_RIGHTS[board.getCompressedCastlingRights()];

  if (ctx.original == Piece::KING) {
    if (board.whiteToMove) {
      board.castlingRights.whiteKingSide = false;
      board.castlingRights.whiteQueenSide = false;
    } else {
      board.castlingRights.blackKingSide = false;
      board.castlingRights.blackQueenSide = false;
    }
  }

  if (ctx.original == Piece::ROOK) {
    updateCastlingByRook(board, ctx.from);
  }
  if (ctx.captured == Piece::ROOK) {
    updateCastlingByRook(board, ctx.capturedSquare);
  }

  const std::uint64_t newZobrist =
      ZOBRIST_CASTLING_RIGHTS[board.getCompressedCastlingRights()];
  if (newZobrist != initialZobrist) {
    board.zobrist ^= initialZobrist ^ newZobrist;
  }
}

void makeMove(ChessBoard &board, const MoveCTX &ctx) {
#ifndef NDEBUG
  assert(ctx.from >= 0 && ctx.from < BOARD_AREA);
  assert(ctx.to >= 0 && ctx.to < BOARD_AREA);
  assert(ctx.capturedSquare >= 0 && ctx.capturedSquare < BOARD_AREA);
  assert(ctx.from != ctx.to);
  assert(ctx.original != Piece::NOTHING);
  assert(ctx.promotion == Piece::NOTHING ||
         (board.whiteToMove ? (ctx.to >= 56) : (ctx.to <= 7)));
  assert(ctx.promotion == Piece::NOTHING ||
         (ctx.promotion <= Piece::QUEEN && ctx.promotion >= Piece::KNIGHT));
#endif // !NDEBUG

  movePieceToDestination(board, ctx);

  if (ctx.captured != Piece::NOTHING || ctx.original == Piece::PAWN) {
    board.halfmoveClock = 0;
  } else {
    board.halfmoveClock++;
  }

  updateEnPassantSquare(board, ctx);
  updateCastlingRightsByMove(board, ctx);

  board.zobrist ^= ZOBRIST_TURN;
  board.whiteToMove = !board.whiteToMove;
}

static void restoreByUndoCTX(ChessBoard &board, const UndoCTX &ctx) {
  board.zobrist = ctx.zobrist;
  board.halfmoveClock = ctx.halfmoveClock;
  board.enPassantSquare = ctx.enPassantSquare;
  board.castlingRights = ctx.castlingRights;
  board.whiteToMove = !board.whiteToMove;
}

static void
restoreRookPositionIfCastling(std::array<std::uint64_t, Piece::KING + 1> &color,
                              ChessBoard &board, const UndoCTX &ctx) {
  const bool isKingSide = board.whiteToMove ? ctx.move.to == BoardSquare::G1
                                            : ctx.move.to == BoardSquare::G8;
  std::uint32_t fromRook = 0;
  std::uint32_t toRook = 0;

  if (board.whiteToMove) {
    fromRook = isKingSide ? BoardSquare::H1 : BoardSquare::A1;
    toRook = isKingSide ? BoardSquare::F1 : BoardSquare::D1;
  } else {
    fromRook = isKingSide ? BoardSquare::H8 : BoardSquare::A8;
    toRook = isKingSide ? BoardSquare::F8 : BoardSquare::D8;
  }

  color[Piece::ROOK] ^= (1ULL << fromRook) | (1ULL << toRook);
}

static void undoMoveDebugAsserts(ChessBoard &board, const UndoCTX &ctx) {
  assert(ctx.move.from < BOARD_AREA && "Invalid source square");
  assert(ctx.move.to < BOARD_AREA && "Invalid destination square");
  assert(ctx.move.capturedSquare < BOARD_AREA && "Invalid captured square");
  assert(ctx.move.from != ctx.move.to && "Source and destination must differ");
  assert(ctx.move.original != Piece::NOTHING &&
         "Original piece cannot be NOTHING");
  assert(ctx.move.promotion == Piece::NOTHING ||
         (ctx.move.promotion >= Piece::KNIGHT &&
          ctx.move.promotion <= Piece::QUEEN) &&
             "Invalid promotion piece");
  assert(ctx.move.promotion == Piece::NOTHING ||
         (board.whiteToMove ? ctx.move.to >= 56 : ctx.move.to <= 7) &&
             "Promotion must occur on opponent's back rank");
  assert(ctx.move.captured <= Piece::KING ||
         ctx.move.captured == Piece::NOTHING && "Invalid captured piece");
  assert(ctx.halfmoveClock <= 127 && "Invalid halfmove clock");
  assert(ctx.enPassantSquare < BOARD_AREA && "Invalid en passant square");
}

void undoMove(ChessBoard &board, const UndoCTX &ctx) {
  restoreByUndoCTX(board, ctx);

#ifndef NDEBUG
  undoMoveDebugAsserts(board, ctx);
#endif

  std::array<std::uint64_t, Piece::KING + 1> &color =
      board.whiteToMove ? board.whites : board.blacks;

  // Remove piece from final position
  const Piece final = ctx.move.promotion != Piece::NOTHING ? ctx.move.promotion
                                                           : ctx.move.original;

  color[final] &= ~(1ULL << ctx.move.to);
  color[ctx.move.original] |= 1ULL << ctx.move.from;

  const bool isCastling =
      ctx.move.original == KING && std::abs(ctx.move.to - ctx.move.from) == 2;
  if (isCastling) {
    restoreRookPositionIfCastling(color, board, ctx);
  }

  if (ctx.move.captured != Piece::NOTHING) {
    std::array<std::uint64_t, Piece::KING + 1> &enemyColor =
        board.whiteToMove ? board.blacks : board.whites;
    enemyColor[ctx.move.captured] |= 1ULL << ctx.move.capturedSquare;
  }
}
