#include "board.h"
#include "legalMoves.h"
#include "luts.h"
#include "move.h"
#include "sysifus.h"
#include <bit>
#include <cstdint>

auto ChessBoard::isSquareUnderAttack(const std::int32_t square,
                                     const bool byWhites) const -> bool {
  const std::array<std::uint64_t, Piece::KING + 1> &attackingSidePieces =
      byWhites ? this->whites : this->blacks;

  const std::uint64_t flat = getFlat(true) | getFlat(false);

  const Coordinate coord = {.rank = static_cast<int8_t>(square / BOARD_LENGTH),
                            .file = static_cast<int8_t>(square % BOARD_LENGTH)};

  std::uint64_t pawnAttacksFromSquare =
      generatePawnCaptures(coord, attackingSidePieces[Piece::PAWN], !byWhites);
  if (pawnAttacksFromSquare != 0) {
    return true;
  }

  if ((KNIGHT_ATTACK_MAP[square] & attackingSidePieces[Piece::KNIGHT]) != 0) {
    return true;
  }

  // --- Check for Bishop/Queen attacks (Diagonal Sliders) ---
  // getBishopAttackByOccupancy expects the piece's square, friendly pieces,
  // and enemy pieces. Here, 'friendly' is *all* pieces (occupancy) and
  // 'enemy' is 0, because we are getting the attack lines *from* square,
  // and then checking if actual enemy pieces are on those lines. The friendly
  // parameter to getBishopAttackByOccupancy is used to clear squares the
  // *hypothetical* bishop would occupy itself, which isn't relevant for
  // attacks, so we combine friendly/enemy.
  std::uint64_t bishopAttacksFromSquare = getBishopAttackByOccupancy(
      static_cast<std::int8_t>(square),
      0ULL, // This is a dummy 'friendly' because the piece is hypothetical on
            // square
      flat  // All occupied squares are blockers for sliders
  );
  // Check if any actual enemy Bishop or Queen is on these attack lines
  if ((bishopAttacksFromSquare & (attackingSidePieces[Piece::BISHOP] |
                                  attackingSidePieces[Piece::QUEEN])) != 0) {
    return true; // King is attacked by a bishop or queen
  }

  // --- Check for Rook/Queen attacks (Orthogonal Sliders) ---
  std::uint64_t rookAttacksFromSquare =
      getRookAttackByOccupancy(static_cast<std::int8_t>(square),
                               0ULL, // Dummy 'friendly'
                               flat  // All occupied squares are blockers
      );
  // Check if any actual enemy Rook or Queen is on these attack lines
  if ((rookAttacksFromSquare & (attackingSidePieces[Piece::ROOK] |
                                attackingSidePieces[Piece::QUEEN])) != 0) {
    return true; // King is attacked by a rook or queen
  }

  // --- King vs King check ---
  // To avoid illegal moves:
  if ((KING_ATTACK_MAP[square] & attackingSidePieces[Piece::KING]) != 0) {
    // This means the opponent's king is adjacent to our king. This is an
    // illegal position.
    return true;
  }

  return false; // No attacking pieces found
}

static auto hasLegalMoves(std::uint64_t pseudoLegalBits,
                          const std::int8_t square, const std::uint32_t type,
                          ChessBoard &board, const bool forWhites) -> bool {
  auto testMoveLegality = [&](const MoveCTX &ctx) -> bool {
    const UndoCTX undo = {
        .move = ctx,
        .castlingRights = board.castlingRights,
        .halfmoveClock = board.halfmoveClock,
        .enPassantSquare = board.enPassantSquare,
        .zobrist = board.zobrist,
    };

    movePieceToDestination(board, ctx);
    const bool isMoveLegal = !board.isKingInCheck(forWhites);
    undoMove(board, undo);

    return isMoveLegal;
  };

  auto checkForPromotions = [&](MoveCTX &ctx) -> bool {
    const auto toRank = static_cast<std::int8_t>(ctx.to / BOARD_LENGTH);
    const std::int8_t promotionRank = forWhites ? 7 : 0;
    if (ctx.original == Piece::PAWN && toRank == promotionRank) {
      for (std::uint32_t promotion = Piece::KNIGHT; promotion <= Piece::QUEEN;
           promotion++) {
        ctx.promotion = static_cast<Piece>(promotion);

        if (testMoveLegality(ctx)) {
          return true;
        }
      }

      return false;
    }

    return testMoveLegality(ctx);
  };

  while (pseudoLegalBits != 0) {
    MoveCTX ctx = {
        .from = static_cast<std::uint32_t>(square),
        .to = static_cast<std::uint32_t>(std::countr_zero(pseudoLegalBits)),
        .capturedSquare = 0,
        .original = static_cast<Piece>(type),
        .captured = Piece::NOTHING,
        .promotion = Piece::NOTHING,
    };

    // This inserts the capturedSquare and captured type
    insertMoveInfo(ctx, board, false, forWhites);

    if (checkForPromotions(ctx)) {
      return true;
    }

    pseudoLegalBits &= pseudoLegalBits - 1;
  }

  return false;
}

auto ChessBoard::checkStalemate(const bool generateForWhites) -> bool {
  const std::uint64_t friendlyFlat = getFlat(generateForWhites);
  const std::uint64_t enemyFlat = getFlat(!generateForWhites);

  const auto &color = generateForWhites ? whites : blacks;
  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    std::uint64_t bitboard = color[type];

    while (bitboard != 0) {
      const auto square = static_cast<std::int8_t>(std::countr_zero(bitboard));
      const Move pseudoLegalMoves =
          getPseudoLegal(static_cast<Piece>(type), square, friendlyFlat,
                         generateForWhites, enemyFlat);
      std::uint64_t pseudoLegalBits =
          pseudoLegalMoves.quiet | pseudoLegalMoves.kills;

      if (hasLegalMoves(pseudoLegalBits, square, type, *this,
                        generateForWhites)) {
        return true;
      }

      bitboard &= bitboard - 1;
    }
  }

  return false;
}

auto ChessBoard::isDraw(const std::array<std::uint64_t, 3> &zobristHistory,
                        const bool isEnemyColorWhite) -> bool {
  static constexpr std::uint8_t fiftyMoveCounterThreshold = 100;
  if (halfmoveClock >= fiftyMoveCounterThreshold) {
    return true;
  }

  if (zobristHistory.size() >= 3) {
    std::uint8_t count = 0;
    for (std::size_t i = zobristHistory.size() - 1;
         i < zobristHistory.size() && i >= 0; --i) {
      if (zobristHistory[i] == zobrist) {
        count++;
        if (count == 3) {
          return true;
        }
      }
    }
  }
  // Optimization: Only check stalemate for the enemy moves of the node, because
  // if node's color has no legal moves it'll just return the evaluation.
  if (checkStalemate(isEnemyColorWhite)) {
    return true;
  }

  return false;
}
