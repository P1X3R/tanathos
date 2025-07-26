#include "move.h"
#include "bitboard.h"
#include "sysifus.h"
#include "zobrist.h"
#include <cassert>
#include <cstddef>
#include <cstdlib>

static void movePieceToDestination(ChessBoard &board, const MoveCTX &ctx) {
  std::array<std::uint64_t, Piece::KING + 1> &color =
      board.whiteToMove ? board.whites : board.blacks;

  color[ctx.original] &= ~(1ULL << ctx.from);

  const Piece final =
      ctx.promotion != Piece::NOTHING ? ctx.promotion : ctx.original;

  color[final] |= 1ULL << ctx.to;

  board.zobrist ^= ZOBRIST_PIECE[board.whiteToMove][ctx.original][ctx.from] ^
                   ZOBRIST_PIECE[board.whiteToMove][final][ctx.to];

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
  const bool isDoublePush =
      ctx.original == Piece::PAWN && abs(ctx.to - ctx.from) == BOARD_LENGTH * 2;
  if (isDoublePush) {
    board.enPassantSquare = (ctx.from + ctx.to) / 2;
    board.zobrist ^=
        ZOBRIST_EN_PASSANT_FILE[board.enPassantSquare % BOARD_LENGTH];
  }
}

void makeMove(ChessBoard &board, const MoveCTX &ctx) {
#ifndef NDEBUG
  assert(ctx.original != Piece::NOTHING);
  assert(ctx.original == Piece::PAWN ? ctx.promotion == Piece::NOTHING : true);
#endif // !NDEBUG

  movePieceToDestination(board, ctx);

  if (ctx.captured != Piece::NOTHING || ctx.original == Piece::PAWN) {
    board.halfmoveCounter = 0;
  } else {
    board.halfmoveCounter++;
  }

  updateEnPassantSquare(board, ctx);

  board.zobrist ^= ZOBRIST_TURN;
  board.whiteToMove = !board.whiteToMove;
}
