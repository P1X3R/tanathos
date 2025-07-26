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

// NOTE: The Zobrist hash is updated after the pseudo-legal moves are generated
// This happens in another function
static void updateCastlingRightsByMove(ChessBoard &board, const MoveCTX &ctx) {
  // Clear previous castling rights
  board.zobrist ^= ZOBRIST_CASTLING_RIGHTS[board.getCompressedCastlingRights()];

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
  updateCastlingRightsByMove(board, ctx);

  board.zobrist ^= ZOBRIST_TURN;
  board.whiteToMove = !board.whiteToMove;
}
