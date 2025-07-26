#pragma once

#include "board.h"
#include "legalMoves.h"

struct UndoCTX {
  MoveCTX move;
  CastlingRights castlingRights;
  std::uint32_t halfmoveClock : 7;
  std::uint32_t enPassantSquare : 6; // 0 means no en passant
  std::uint64_t zobrist;
};

void makeMove(ChessBoard &board, const MoveCTX &ctx);
void undoMove(ChessBoard &board, const UndoCTX &ctx);
