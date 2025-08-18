#include "bitboard.h"
#include "board.h"
#include "legalMoves.h"
#include "luts.h"
#include "searching.h"
#include "sysifus.h"
#include <algorithm>
#include <array>
#include <bit>
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

static constexpr std::array<std::array<std::uint16_t, Piece::KING + 1>,
                            Piece::KING + 1>
    MVV_LVA = [] {
      std::array<std::array<std::uint16_t, Piece::KING + 1>, Piece::KING + 1>
          table{};
      constexpr std::uint16_t AGGRESSOR_DECREASING = 6;

      for (int aggressor = 0; aggressor <= Piece::KING; ++aggressor) {
        for (int victim = 0; victim <= Piece::KING; ++victim) {
          table[aggressor][victim] =
              PIECE_VALUES[victim] * VICTIM_SCALING_FACTOR +
              (AGGRESSOR_DECREASING - aggressor);
        }
      }
      return table;
    }();

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

static auto getPinnedAttackers(const std::uint64_t attackers,
                               const bool forWhites, const ChessBoard &board,
                               const std::int8_t attackerKingSquare,
                               const std::uint64_t attackerFlat,
                               const std::uint64_t attackersEnemyFlat)
    -> std::uint64_t {
  const auto &enemyColor = forWhites ? board.blacks : board.whites;

  std::uint64_t bishopXRays = getBishopAttackByOccupancy(
      attackerKingSquare, 0ULL,
      enemyColor[Piece::BISHOP] | enemyColor[Piece::QUEEN]);
  std::uint64_t bishopPinners = bishopXRays & attackersEnemyFlat;

  std::uint64_t rookXRays = getRookAttackByOccupancy(
      attackerKingSquare, 0ULL,
      enemyColor[Piece::ROOK] | enemyColor[Piece::QUEEN]);
  std::uint64_t rookPinners = rookXRays & attackersEnemyFlat;

  std::uint64_t result = 0;

  while (bishopPinners != 0) {
    const auto pinnerSquare =
        static_cast<std::int8_t>(std::countr_zero(bishopPinners));

    const std::uint64_t xRay =
        getBishopAttackByOccupancy(pinnerSquare, attackersEnemyFlat,
                                   attackerFlat) &
        bishopXRays;

    if (std::popcount(xRay & attackerFlat) == 1) {
      result |= xRay & attackerFlat;
    }

    bishopPinners &= bishopPinners - 1;
  }

  while (rookPinners != 0) {
    const auto pinnerSquare =
        static_cast<std::int8_t>(std::countr_zero(rookPinners));

    const std::uint64_t xRay =
        getRookAttackByOccupancy(pinnerSquare, attackersEnemyFlat,
                                 attackerFlat) &
        rookXRays;

    if (std::popcount(xRay & attackerFlat) == 1) {
      result |= xRay & attackerFlat;
    }

    rookPinners &= rookPinners - 1;
  }

  return result;
}

static auto squaresInBetween(std::uint32_t squareA, std::uint32_t squareB)
    -> std::uint64_t {
  const std::uint32_t rankA = squareA / BOARD_LENGTH;
  const std::uint32_t rankB = squareB / BOARD_LENGTH;

  const std::uint32_t fileA = squareA % BOARD_LENGTH;
  const std::uint32_t fileB = squareB % BOARD_LENGTH;

  if (rankA == rankB || fileA == fileB) {
    return ROOK_ATTACK_MAP[squareA][0] & ROOK_ATTACK_MAP[squareB][0];
  }
  if (rankA - fileA == rankB - fileB || rankA + fileA == rankB + fileB) {
    return BISHOP_ATTACK_MAP[squareA][0] & BISHOP_ATTACK_MAP[squareB][0];
  }

  return 0;
}

[[nodiscard]] auto MoveCTX::see(std::uint64_t whitesFlat,
                                const ChessBoard &board,
                                std::uint64_t blacksFlat) const
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
  std::uint64_t occupancy = whitesFlat | blacksFlat;
  std::uint64_t attackers = getAttackers(occupancy, board, true, to) |
                            getAttackers(occupancy, board, false, to);
  Piece attackerType = original;
  std::uint32_t whiteKingSquare = std::countr_zero(board.whites[Piece::KING]);
  std::uint32_t blackKingSquare = std::countr_zero(board.blacks[Piece::KING]);
  gain[depth] = PIECE_VALUES[captured];

  do {
    forWhites = !forWhites;
    depth++;
    gain[depth] = PIECE_VALUES[attackerType] - gain[depth - 1];
    std::uint64_t &attackerFlat = forWhites ? whitesFlat : blacksFlat;

    attackers ^= fromSet;
    occupancy ^= fromSet;
    attackerFlat ^= fromSet;

    std::uint32_t &attackerKingSquare =
        forWhites ? whiteKingSquare : blackKingSquare;

    const std::uint64_t pinned =
        getPinnedAttackers(attackers, forWhites, board,
                           static_cast<std::int8_t>(attackerKingSquare),
                           attackerFlat, forWhites ? blacksFlat : whitesFlat);
    const std::uint64_t kingRay =
        squaresInBetween(attackerKingSquare, to) | (1ULL << to);
    attackers &= ~pinned | (pinned & kingRay);

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

    if (attackerType == Piece::KING) {
      attackerKingSquare = to;
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
    const std::uint8_t ply, const ChessBoard &board) const
    -> std::uint16_t {
  if (entryBestMove != nullptr && *entryBestMove == *this) {
    return TRANSPOSITION_TABLE;
  }
  std::uint16_t score = 0;

  if (captured != Piece::NOTHING) {
    static constexpr std::uint8_t SEE_FACTOR = 10;
    score += CAPTURES + (MVV_LVA[original][captured] * SEE_FACTOR);
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
                    const MoveCTX *entryBestMove) -> const MoveCTX * {
  if (moves.empty()) {
    return nullptr;
  }

  std::int32_t bestMoveScore = -1;
  std::int16_t bestMoveIndex = -1;

  for (std::size_t i = moveIndex; i < moves.size(); i++) {
    const MoveCTX &move = moves[i];
    const std::uint16_t moveScore = move.score(
        entryBestMove, killers, history, ply, board);

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
