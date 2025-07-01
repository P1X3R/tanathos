#include "legalMoves.h"
#include "bitboard.h"
#include "board.h"
#include "sysifus.h" // for getPseudoLegal
#include <bit>
#include <cstdint>

static void appendContext(MoveCTX &ctx, const bool forWhites,
                          const std::array<uint64_t, KING + 1> &enemyColor,
                          const uint64_t enemyFlat,
                          std::vector<MoveCTX> &pseudoLegal,
                          const uint32_t enPassantSquare) {
  const uint64_t toBit = 1ULL << ctx.to;
  ctx.capturedSquare = ctx.to;

  if ((enemyFlat & toBit) != 0) {
    for (int enemyType = PAWN; enemyType <= KING; enemyType++) {
      if ((enemyColor[enemyType] & toBit) != 0) {
        ctx.captured = static_cast<Piece>(enemyType);
        break;
      }
    }
  } else {
    ctx.captured = NOTHING;
  }

  // Adjust capturedSquare for en passant capture
  const int32_t enPassantCapture =
      forWhites ? ctx.to - BOARD_LENGTH : ctx.to + BOARD_LENGTH;
  if (ctx.original == PAWN && ctx.to == enPassantSquare &&
      (enemyColor[PAWN] & (1ULL << enPassantCapture)) != 0) {
    ctx.capturedSquare = enPassantCapture;
  }

  const auto toRank = static_cast<int8_t>(ctx.to / BOARD_LENGTH);
  const int8_t promotionRank = forWhites ? 7 : 0;
  if (ctx.original == PAWN && toRank == promotionRank) {
    // Generate promotion moves for Knight, Bishop, Rook, Queen
    for (int promotion = KNIGHT; promotion <= QUEEN; promotion++) {
      ctx.promotion = static_cast<Piece>(promotion);
      pseudoLegal.push_back(ctx);
    }
  } else {
    pseudoLegal.push_back(ctx);
  }
}

void MoveGenerator::generatePseudoLegal(const ChessBoard &board,
                                        bool forWhites) {
  const std::array<uint64_t, KING + 1> &color =
      forWhites ? board.whites : board.blacks;
  const std::array<uint64_t, KING + 1> &enemyColor =
      forWhites ? board.blacks : board.whites;

  const uint64_t flat = board.getFlat(forWhites);
  const uint64_t enemyFlat = board.getFlat(!forWhites);

  for (int type = PAWN; type <= KING; type++) {
    uint64_t typeBitboard = color[type];

    while (typeBitboard != 0) {
      const auto fromSquare =
          static_cast<int8_t>(std::countr_zero(color.at(typeBitboard)));
      const Move pseudoLegalMoves = getPseudoLegal(
          static_cast<Piece>(type), fromSquare, flat, forWhites, enemyFlat);
      uint64_t pseudoLegalBits =
          pseudoLegalMoves.quiet | pseudoLegalMoves.kills;

      while (pseudoLegalBits != 0) {
        // Captured info and promotion are updated on pushContext
        MoveCTX ctx = {
            .from = static_cast<uint32_t>(fromSquare),
            .to = static_cast<uint32_t>(std::countr_zero(pseudoLegalBits)),
            .capturedSquare = 0,
            .original = static_cast<Piece>(type),
            .captured = NOTHING,
            .promotion = NOTHING,
        };

        appendContext(ctx, forWhites, enemyColor, enemyFlat, pseudoLegal,
                      board.enPassantSquare);

        pseudoLegalBits &= pseudoLegalBits - 1;
      }

      typeBitboard &= typeBitboard - 1;
    }
  }
}
