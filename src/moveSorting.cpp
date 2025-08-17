#include "bitboard.h"
#include "board.h"
#include "legalMoves.h"
#include "luts.h"
#include "searching.h"
#include "sysifus.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

enum MoveOrderBonus : std::uint16_t {
  TRANSPOSITION_TABLE = 50000,
  CAPTURES = 10000,
  KILLER_MOVE = 5000,
  HISTORY = 3000,
};

constexpr std::uint16_t VICTIM_SCALING_FACTOR = 10;

static auto leastValuablePiece(std::uint64_t attackers, const ChessBoard &board,
                               const bool forWhites, Piece &attackerType)
    -> std::uint64_t {
  const auto &color = forWhites ? board.whites : board.blacks;
  for (attackerType = Piece::PAWN; attackerType <= Piece::KING;
       attackerType =
           static_cast<Piece>(static_cast<std::uint8_t>(attackerType) + 1)) {
    const std::uint64_t attackersFromType = color[attackerType] & attackers;
    if (attackersFromType != 0) {
      return attackersFromType & -attackersFromType;
    }
  }

  return 0;
}

static auto getAttackers(std::uint64_t flat, const ChessBoard &board,
                         const bool byWhites,
                         const std::uint32_t attackedSquare) -> std::uint64_t {
  const std::array<std::uint64_t, Piece::KING + 1> &attackingSidePieces =
      byWhites ? board.whites : board.blacks;

  const Coordinate coord = {
      .rank = static_cast<int8_t>(attackedSquare / BOARD_LENGTH),
      .file = static_cast<int8_t>(attackedSquare % BOARD_LENGTH)};

  std::uint64_t result = 0;

  const std::uint64_t pawnAttacksFromSquare =
      generatePawnCaptures(coord, attackingSidePieces[Piece::PAWN], !byWhites);
  result |= pawnAttacksFromSquare;

  result |=
      KNIGHT_ATTACK_MAP[attackedSquare] & attackingSidePieces[Piece::KNIGHT];

  // --- Check for Bishop/Queen attacks (Diagonal Sliders) ---
  // getBishopAttackByOccupancy expects the piece's square, friendly pieces,
  // and enemy pieces. Here, 'friendly' is *all* pieces (occupancy) and
  // 'enemy' is 0, because we are getting the attack lines *from* square,
  // and then checking if actual enemy pieces are on those lines. The friendly
  // parameter to getBishopAttackByOccupancy is used to clear squares the
  // *hypothetical* bishop would occupy itself, which isn't relevant for
  // attacks, so we combine friendly/enemy.
  std::uint64_t bishopAttacksFromSquare = getBishopAttackByOccupancy(
      static_cast<std::int8_t>(attackedSquare),
      0ULL, // This is a dummy 'friendly' because the piece is hypothetical on
            // square
      flat  // All occupied squares are blockers for sliders
  );
  // Check if any actual enemy Bishop or Queen is on these attack lines
  result |= bishopAttacksFromSquare & (attackingSidePieces[Piece::BISHOP] |
                                       attackingSidePieces[Piece::QUEEN]);

  // --- Check for Rook/Queen attacks (Orthogonal Sliders) ---
  std::uint64_t rookAttacksFromSquare =
      getRookAttackByOccupancy(static_cast<std::int8_t>(attackedSquare),
                               0ULL, // Dummy 'friendly'
                               flat  // All occupied squares are blockers
      );
  result |= rookAttacksFromSquare & (attackingSidePieces[Piece::ROOK] |
                                     attackingSidePieces[Piece::QUEEN]);

  result |= KING_ATTACK_MAP[attackedSquare] & attackingSidePieces[Piece::KING];

  return result & flat;
}

[[nodiscard]] auto MoveCTX::see(const ChessBoard &board,
                                const std::uint64_t flat) const
    -> std::int32_t {
  static constexpr std::size_t GAIN_LEN = 32;
  std::array<std::int32_t, GAIN_LEN> gain{};
  std::uint8_t depth = 0;
  bool forWhites = board.whiteToMove;
  std::uint64_t mayXRay =
      board.whites[Piece::PAWN] | board.whites[Piece::BISHOP] |
      board.whites[Piece::ROOK] | board.whites[Piece::QUEEN] |
      board.blacks[Piece::PAWN] | board.blacks[Piece::BISHOP] |
      board.blacks[Piece::ROOK] | board.blacks[Piece::QUEEN];
  std::uint64_t fromSet = 1ULL << from;
  std::uint64_t occupancy = flat;
  std::uint64_t attackers = getAttackers(flat, board, true, to) |
                            getAttackers(flat, board, false, to);
  Piece attackerType = original;
  gain[depth] = PIECE_VALUES[captured];

  do {
    forWhites = !forWhites;
    depth++;
    gain[depth] = PIECE_VALUES[attackerType] - gain[depth - 1];

    attackers ^= fromSet;
    occupancy ^= fromSet;

    if ((fromSet & mayXRay) != 0) {
      const std::array<std::uint64_t, Piece::KING + 1> &attackingSidePieces =
          forWhites ? board.whites : board.blacks;
      const Coordinate coord = {.rank = static_cast<int8_t>(to / BOARD_LENGTH),
                                .file = static_cast<int8_t>(to % BOARD_LENGTH)};
      attackers |= (generatePawnCaptures(
                        coord, attackingSidePieces[Piece::PAWN], forWhites) |
                    (getBishopAttackByOccupancy(static_cast<std::int8_t>(to),
                                                0ULL, occupancy) &
                     (attackingSidePieces[Piece::BISHOP] |
                      attackingSidePieces[Piece::QUEEN])) |
                    (getRookAttackByOccupancy(static_cast<std::int8_t>(to),
                                              0ULL, occupancy) &
                     (attackingSidePieces[Piece::ROOK] |
                      attackingSidePieces[Piece::QUEEN]))) &
                   occupancy;
    }

    fromSet = leastValuablePiece(attackers, board, forWhites, attackerType);
  } while (fromSet != 0 && attackers != 0 && depth < GAIN_LEN);

  while (--depth > 0) {
    gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
  }

  return gain[0];
}

auto MoveCTX::score(
    const MoveCTX *entryBestMove,
    const std::array<std::array<MoveCTX, 2>, MAX_DEPTH + 1> &killers,
    const std::array<
        std::array<std::array<std::uint16_t, BOARD_AREA>, BOARD_AREA>, 2>
        &history,
    const std::uint8_t ply, const ChessBoard &board,
    const std::uint64_t flat) const -> std::uint16_t {
  if (entryBestMove != nullptr && *entryBestMove == *this) {
    return TRANSPOSITION_TABLE;
  }
  std::uint16_t score = 0;

  if (captured != Piece::NOTHING) {
    static constexpr std::uint8_t SEE_FACTOR = 10;
    score += CAPTURES + (see(board, flat) * SEE_FACTOR);
  }

  score +=
      HISTORY + history[static_cast<std::size_t>(!board.whiteToMove)][from][to];

  if (killers[ply][0] == *this || killers[ply][1] == *this) {
    score += KILLER_MOVE;
  }

  score += PIECE_VALUES[promotion] * VICTIM_SCALING_FACTOR;

  return score;
}

[[nodiscard]] auto
Searching::pickMove(std::vector<MoveCTX> &moves, std::uint8_t moveIndex,
                    const ChessBoard &board, std::uint8_t ply,
                    const MoveCTX *entryBestMove, const std::uint64_t flat)
    -> const MoveCTX * {
  if (moves.empty()) {
    return nullptr;
  }

  std::int32_t bestMoveScore = -1;
  std::int16_t bestMoveIndex = -1;

  for (std::size_t i = moveIndex; i < moves.size(); i++) {
    const MoveCTX &move = moves[i];
    const std::uint16_t moveScore =
        move.score(entryBestMove, killers, history, ply, board, flat);

    if (moveScore > bestMoveScore) {
      bestMoveScore = moveScore;
      bestMoveIndex = static_cast<std::int16_t>(i);
    }
  }

  if (bestMoveIndex != -1) {
    std::swap(moves[bestMoveIndex], moves[moveIndex]);
  }

  return &moves[moveIndex];
}
