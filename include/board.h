#pragma once

#include "luts.h"
#include "sysifus.h"
#include <array>
#include <bit>
#include <cstdint>
#include <string>

struct CastlingRights {
  bool whiteKingSide : 1;
  bool whiteQueenSide : 1;
  bool blackKingSide : 1;
  bool blackQueenSide : 1;
};

struct ChessBoard {
  std::array<std::uint64_t, Piece::KING + 1> whites, blacks;
  std::uint64_t zobrist;
  std::uint32_t halfmoveClock : 7;
  std::uint32_t enPassantSquare : 6; // 0 means "no en passant"
  bool whiteToMove : 1;
  CastlingRights castlingRights;

  ChessBoard() = default;
  explicit ChessBoard(const std::string &fen);

  [[nodiscard]] auto getFlat(const bool forWhites) const -> std::uint64_t {
    const std::array<std::uint64_t, Piece::KING + 1> &color =
        forWhites ? this->whites : this->blacks;

    std::uint64_t result = 0;
    for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
      result |= color[type];
    }

    return result;
  }

  [[nodiscard]] auto getCompressedCastlingRights() const -> std::uint8_t {
    // Masked to ensure only the first 4 bits can be set
    static constexpr std::uint8_t irrelevantBitMask = 0x0F;
    return std::bit_cast<std::uint8_t>(castlingRights) & irrelevantBitMask;
  }

  [[nodiscard]] auto isKingInCheck(bool kingIsWhite) const -> bool {
    // Determine the king's actual bitboard and square
    const std::array<std::uint64_t, Piece::KING + 1> &kingSidePieces =
        kingIsWhite ? this->whites : this->blacks;
    const auto kingSquare =
        static_cast<std::int8_t>(std::countr_zero(kingSidePieces[Piece::KING]));

    // Determine the attacking side's pieces and their flat bitboard
    const std::array<std::uint64_t, Piece::KING + 1> &attackingSidePieces =
        kingIsWhite ? this->blacks : this->whites;

    // Get the all-pieces bitboard for slider attacks
    const std::uint64_t allPiecesFlat = getFlat(true) | getFlat(false);

    // --- Check for Pawn attacks ---
    // We need to check if any *enemy pawns* can capture the kingSquare.
    // Pawn attacks are directional.
    // For white king, check if black pawns attack.
    // For black king, check if white pawns attack.
    // generatePawnCaptures expects coord, enemy (pieces it can capture),
    // isWhite (color of the pawn generating captures)
    Coordinate kingCoord = {
        .rank = static_cast<int8_t>(kingSquare / BOARD_LENGTH),
        .file = static_cast<int8_t>(kingSquare % BOARD_LENGTH)};

    std::uint64_t pawn_attacks_from_king_square =
        generatePawnCaptures(kingCoord, 0ULL, !kingIsWhite);
    if ((pawn_attacks_from_king_square & attackingSidePieces[Piece::PAWN]) !=
        0) {
      return true;
    }

    if ((KNIGHT_ATTACK_MAP[kingSquare] & attackingSidePieces[Piece::KNIGHT]) !=
        0) {
      return true; // King is attacked by a knight
    }

    // --- Check for Bishop/Queen attacks (Diagonal Sliders) ---
    // getBishopAttackByOccupancy expects the piece's square, friendly pieces,
    // and enemy pieces. Here, 'friendly' is *all* pieces (occupancy) and
    // 'enemy' is 0, because we are getting the attack lines *from* kingSquare,
    // and then checking if actual enemy pieces are on those lines. The friendly
    // parameter to getBishopAttackByOccupancy is used to clear squares the
    // *hypothetical* bishop would occupy itself, which isn't relevant for
    // attacks, so we combine friendly/enemy.
    std::uint64_t bishop_attacks_from_king_square = getBishopAttackByOccupancy(
        kingSquare,
        0ULL, // This is a dummy 'friendly' because the piece is hypothetical on
              // kingSquare
        allPiecesFlat // All occupied squares are blockers for sliders
    );
    // Check if any actual enemy Bishop or Queen is on these attack lines
    if ((bishop_attacks_from_king_square &
         (attackingSidePieces[Piece::BISHOP] |
          attackingSidePieces[Piece::QUEEN])) != 0) {
      return true; // King is attacked by a bishop or queen
    }

    // --- Check for Rook/Queen attacks (Orthogonal Sliders) ---
    std::uint64_t rook_attacks_from_king_square = getRookAttackByOccupancy(
        kingSquare,
        0ULL,         // Dummy 'friendly'
        allPiecesFlat // All occupied squares are blockers
    );
    // Check if any actual enemy Rook or Queen is on these attack lines
    if ((rook_attacks_from_king_square & (attackingSidePieces[Piece::ROOK] |
                                          attackingSidePieces[Piece::QUEEN])) !=
        0) {
      return true; // King is attacked by a rook or queen
    }

    // --- King vs King check (Optional, as positions are illegal if king moves
    // into king attack) --- Not strictly necessary for engine legality, as king
    // moves into check are invalid. But for a complete isSquareAttacked, you
    // could include:
    if ((KING_ATTACK_MAP[kingSquare] & attackingSidePieces[Piece::KING]) != 0) {
      // This means the opponent's king is adjacent to our king. This is an
      // illegal position.
      return true;
    }

    return false; // No attacking pieces found
  }
};

enum BoardSquare : std::uint8_t {
  A1 = 0,
  B1,
  C1,
  D1,
  E1,
  F1,
  G1,
  H1,
  A2,
  B2,
  C2,
  D2,
  E2,
  F2,
  G2,
  H2,
  A3,
  B3,
  C3,
  D3,
  E3,
  F3,
  G3,
  H3,
  A4,
  B4,
  C4,
  D4,
  E4,
  F4,
  G4,
  H4,
  A5,
  B5,
  C5,
  D5,
  E5,
  F5,
  G5,
  H5,
  A6,
  B6,
  C6,
  D6,
  E6,
  F6,
  G6,
  H6,
  A7,
  B7,
  C7,
  D7,
  E7,
  F7,
  G7,
  H7,
  A8,
  B8,
  C8,
  D8,
  E8,
  F8,
  G8,
  H8
};
