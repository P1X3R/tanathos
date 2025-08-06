#include "board.h"
#include "luts.h"
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
