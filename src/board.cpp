#include "board.h"
#include "luts.h"
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

auto ChessBoard::insufficientMaterial() -> bool {
  // Check for insufficient material
  auto countPieces = [](const std::array<std::uint64_t, Piece::KING + 1> &color)
      -> std::uint8_t {
    std::uint8_t count = 0;
    for (std::uint32_t type = Piece::PAWN; type <= Piece::QUEEN; type++) {
      count += std::popcount(color[type]);
    }
    return count;
  };

  const std::uint8_t whitePieceCount = countPieces(whites);
  const std::uint8_t blackPieceCount = countPieces(blacks);

  // King vs King
  if (whitePieceCount == 1 && blackPieceCount == 1) {
    return true;
  }

  // King vs King + Knight or Bishop
  if (whitePieceCount == 1 && blackPieceCount == 2) {
    if (std::popcount(blacks[Piece::KNIGHT]) == 1 ||
        std::popcount(blacks[Piece::BISHOP]) == 1) {
      return true;
    }
  }
  if (blackPieceCount == 1 && whitePieceCount == 2) {
    if (std::popcount(whites[Piece::KNIGHT]) == 1 ||
        std::popcount(whites[Piece::BISHOP]) == 1) {
      return true;
    }
  }

  // King + Bishop vs King + Bishop (same color)
  if (whitePieceCount == 2 && blackPieceCount == 2) {
    if (std::popcount(whites[Piece::BISHOP]) == 1 &&
        std::popcount(blacks[Piece::BISHOP]) == 1) {
      // Check if bishops are on same color
      const std::uint64_t whiteBishop = whites[Piece::BISHOP];
      const std::uint64_t blackBishop = blacks[Piece::BISHOP];
      const std::uint32_t whiteSquare = std::countr_zero(whiteBishop);
      const std::uint32_t blackSquare = std::countr_zero(blackBishop);
      const bool whiteIsLight =
          ((whiteSquare / BOARD_LENGTH) + (whiteSquare % BOARD_LENGTH)) % 2 ==
          0;
      const bool blackIsLight =
          ((blackSquare / BOARD_LENGTH) + (blackSquare % BOARD_LENGTH)) % 2 ==
          0;

      if (whiteIsLight == blackIsLight) {
        return true;
      }
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
  if (insufficientMaterial()) {
    return true;
  }

  return false;
}
