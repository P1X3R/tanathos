#pragma once

#include "board.h"
#include "legalMoves.h"

struct UndoCTX {
  const MoveCTX &move;
  CastlingRights castlingRights;
  std::uint32_t halfmoveClock : 7;
  std::uint32_t enPassantSquare : 6; // 0 means no en passant
  std::uint64_t zobrist;

  UndoCTX(const MoveCTX &_move, const ChessBoard &board)
      : move(_move), castlingRights(board.castlingRights),
        halfmoveClock(board.halfmoveClock),
        enPassantSquare(board.enPassantSquare), zobrist(board.zobrist) {}
};

void movePieceToDestination(ChessBoard &board, const MoveCTX &ctx);
void makeMove(ChessBoard &board, const MoveCTX &ctx);
void undoMove(ChessBoard &board, const UndoCTX &ctx);
