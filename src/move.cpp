#include "move.h"
#include "sysifus.h"
#include "zobrist.h"
#include <cstddef>

static void movePieceToDestination(ChessBoard &board, const MoveCTX &ctx) {
  std::array<std::uint64_t, Piece::KING + 1> &color =
      board.whiteToMove ? board.whites : board.blacks;

  color.at(ctx.original) &= ~(1ULL << ctx.from);

  const Piece final =
      ctx.promotion != Piece::NOTHING ? ctx.promotion : ctx.original;

  color.at(final) |= 1ULL << ctx.to;

  board.zobrist ^= ZOBRIST_PIECE[board.whiteToMove][ctx.original][ctx.from] ^
                   ZOBRIST_PIECE[board.whiteToMove][final][ctx.to];

  if (ctx.captured != Piece::NOTHING) {
    std::array<std::uint64_t, Piece::KING + 1> &enemyColor =
        board.whiteToMove ? board.blacks : board.whites;

    enemyColor.at(ctx.captured) &= ~(1ULL << ctx.capturedSquare);

    board.zobrist ^= ZOBRIST_PIECE[static_cast<std::size_t>(!board.whiteToMove)]
                                  [ctx.captured][ctx.capturedSquare];
  }
}

void makeMove(ChessBoard &board, const MoveCTX &ctx) {
  movePieceToDestination(board, ctx);

  if (ctx.captured != Piece::NOTHING || ctx.original == Piece::PAWN) {
    board.halfmoveCounter = 0;
  } else {
    board.halfmoveCounter++;
  }

  board.zobrist ^= ZOBRIST_TURN[board.whiteToMove] ^
                   ZOBRIST_TURN[static_cast<std::size_t>(!board.whiteToMove)];
  board.whiteToMove = !board.whiteToMove;
}
