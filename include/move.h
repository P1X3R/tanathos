#pragma once

#include "board.h"
#include "legalMoves.h"

void makeMove(ChessBoard &board, const MoveCTX &ctx);
void undoMove(ChessBoard &board, const MoveCTX &lastMove);
