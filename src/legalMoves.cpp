#include "legalMoves.h"
#include "bitboard.h"
#include "board.h"
#include "sysifus.h" // for getPseudoLegal
#include <bit>
#include <cstdint>
#include <cstdlib>

void appendContext(MoveCTX &ctx, const bool forWhites,
                   const std::array<uint64_t, Piece::KING + 1> &enemyColor,
                   const uint64_t enemyFlat, std::vector<MoveCTX> &pseudoLegal,
                   const int32_t enPassantSquare) {
  // Adjust capturedSquare for en passant capture
  const int32_t enPassantCapture = forWhites ? enPassantSquare - BOARD_LENGTH
                                             : enPassantSquare + BOARD_LENGTH;

  const bool isEnPassantCapture =
      ctx.original == Piece::PAWN && enPassantSquare != 0 &&
      std::abs(ctx.from - enPassantCapture) == 1 && enPassantCapture >= 0 &&
      enPassantCapture < BOARD_AREA &&
      (enemyColor.at(Piece::PAWN) & (1ULL << enPassantCapture)) != 0;

  const uint64_t toBit = 1ULL << ctx.to;

  ctx.capturedSquare = ctx.to;
  if (isEnPassantCapture) {
    ctx.capturedSquare = enPassantCapture;
    ctx.to = enPassantSquare;
    ctx.captured = Piece::PAWN;
  } else if ((enemyFlat & toBit) != 0) {
    // Handle regular captures
    for (uint32_t enemyType = Piece::PAWN; enemyType <= Piece::KING;
         enemyType++) {
      if ((enemyColor.at(enemyType) & toBit) != 0) {
        ctx.captured = static_cast<Piece>(enemyType);
        break;
      }
    }
  } else {
    ctx.captured = Piece::NOTHING;
  }

  const auto toRank = static_cast<int8_t>(ctx.to / BOARD_LENGTH);
  const int8_t promotionRank = forWhites ? 7 : 0;
  if (ctx.original == Piece::PAWN && toRank == promotionRank) {
    // Generate promotion moves for Knight, Bishop, Rook, Queen
    for (uint32_t promotion = Piece::KNIGHT; promotion <= Piece::QUEEN;
         promotion++) {
      ctx.promotion = static_cast<Piece>(promotion);
      pseudoLegal.push_back(ctx);
    }
  } else {
    pseudoLegal.push_back(ctx);
  }
}

void MoveGenerator::generatePseudoLegal(const ChessBoard &board,
                                        const bool forWhites) {
  const std::array<uint64_t, Piece::KING + 1> &color =
      forWhites ? board.whites : board.blacks;
  const std::array<uint64_t, Piece::KING + 1> &enemyColor =
      forWhites ? board.blacks : board.whites;

  const uint64_t flat = board.getFlat(forWhites);
  const uint64_t enemyFlat = board.getFlat(!forWhites);

  for (uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    uint64_t typeBitboard = color.at(type);

    while (typeBitboard != 0) {
      const auto fromSquare =
          static_cast<int8_t>(std::countr_zero(typeBitboard));
      const Move pseudoLegalMoves = getPseudoLegal(
          static_cast<Piece>(type), fromSquare, flat, forWhites, enemyFlat);
      uint64_t pseudoLegalBits =
          pseudoLegalMoves.quiet | pseudoLegalMoves.kills;

      kills |= pseudoLegalMoves.kills;

      while (pseudoLegalBits != 0) {
        // Captured info and promotion are updated on pushContext
        MoveCTX ctx = {
            .from = static_cast<uint32_t>(fromSquare),
            .to = static_cast<uint32_t>(std::countr_zero(pseudoLegalBits)),
            .capturedSquare = 0,
            .original = static_cast<Piece>(type),
            .captured = Piece::NOTHING,
            .promotion = Piece::NOTHING,
        };

        appendContext(ctx, forWhites, enemyColor, enemyFlat, pseudoLegal,
                      static_cast<int8_t>(board.enPassantSquare));

        pseudoLegalBits &= pseudoLegalBits - 1;
      }

      typeBitboard &= typeBitboard - 1;
    }
  }
}

void MoveGenerator::updateCastlingRights(ChessBoard &board,
                                         const uint64_t enemyFlat,
                                         const bool forWhites,
                                         const uint64_t enemyAttacks) {
  const uint64_t blockedSquares = enemyAttacks | enemyFlat;

  struct {
    uint64_t kingSide;
    uint64_t queenSide;
  } kingsPath = {
      .kingSide = (1ULL << BoardSquare::F1) | (1ULL << BoardSquare::G1) |
                  (1ULL << BoardSquare::E1),

      .queenSide = (1ULL << BoardSquare::D1) | (1ULL << BoardSquare::C1) |
                   (1ULL << BoardSquare::E1),
  };

  const uint32_t rankShifting =
      (BOARD_AREA - BOARD_LENGTH) * static_cast<uint32_t>(!forWhites);
  kingsPath.kingSide <<= rankShifting;
  kingsPath.queenSide <<= rankShifting;

  if ((kingsPath.kingSide & blockedSquares) != 0) {
    if (forWhites) {
      board.castlingRights.whiteKingSide = false;
    } else {
      board.castlingRights.blackKingSide = false;
    }
  }

  if ((kingsPath.queenSide & blockedSquares) != 0) {
    if (forWhites) {
      board.castlingRights.whiteQueenSide = false;
    } else {
      board.castlingRights.blackQueenSide = false;
    }
  }
}

void MoveGenerator::appendCastling(const ChessBoard &board,
                                   const bool forWhites) {
  uint64_t castleMask = 0;
  if (forWhites) {
    castleMask = (1ULL << BoardSquare::G1) | (1ULL << BoardSquare::C1);
  } else {
    castleMask = (1ULL << BoardSquare::G8) | (1ULL << BoardSquare::C8);
  }

  MoveCTX ctx = {
      .from = forWhites ? BoardSquare::E1 : BoardSquare::E8,
      .to = 0,
      .capturedSquare = 0,
      .original = Piece::KING,
      .captured = Piece::NOTHING,
      .promotion = Piece::NOTHING,
  };

  while (castleMask != 0) {
    ctx.to = std::countr_zero(castleMask);

    pseudoLegal.push_back(ctx);

    castleMask &= castleMask - 1;
  }
}
