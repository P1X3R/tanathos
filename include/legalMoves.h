#pragma once

#include "board.h"
#include "sysifus.h"
#include <cstdint>
#include <string>
#include <vector>

static constexpr std::uint8_t MAX_DEPTH = 120;

struct MoveCTX {
  std::uint32_t from : 6 = 0; // The square where the moved piece comes from
  std::uint32_t to : 6 = 0;   // The square where the moved piece shall land
  // The square where the captured piece was. Useful for en passant handling
  std::uint32_t capturedSquare : 6 = 0;
  Piece original : 3 = Piece::NOTHING, captured : 3 = Piece::NOTHING,
                   promotion : 3 = Piece::NOTHING;

  auto operator==(const MoveCTX &other) const -> bool {
    return this->from == other.from && this->to == other.to &&
           this->capturedSquare == other.capturedSquare &&
           this->original == other.original &&
           this->captured == other.captured &&
           this->promotion == other.promotion;
  }

  [[nodiscard]] auto see(std::uint64_t whitesFlat, const ChessBoard &board,
                         std::uint64_t blacksFlat) const -> std::int32_t;
};

auto fromAlgebraic(const std::string_view &algebraic, const ChessBoard &board)
    -> MoveCTX;

auto moveToUCI(const MoveCTX &move) -> std::string;

constexpr std::uint32_t MAX_MOVES_IN_A_POSITION = 218;

static constexpr std::uint8_t BUCKETS_LEN = 8;
enum BucketEnum : std::uint8_t {
  TT = 0,
  CHECKS,
  GOOD_CAPTURES,
  KILLERS,
  PROMOTIONS,
  HISTORY_HEURISTICS,
  BAD_CAPTURES,
  QUIET,
};

inline auto operator++(BucketEnum &bucket) -> BucketEnum & {
  bucket = static_cast<BucketEnum>(static_cast<std::uint8_t>(bucket) + 1);
  return bucket;
}

// For only one color
class MoveGenerator {
public:
  std::vector<MoveCTX> pseudoLegal;
  std::array<std::vector<MoveCTX>, BUCKETS_LEN> buckets;

  const std::array<std::array<MoveCTX, 2>, MAX_DEPTH + 1> *killers = nullptr;
  const std::array<
      std::array<std::array<std::uint16_t, BOARD_AREA>, BOARD_AREA>, 2>
      *history = nullptr;

  const ChessBoard &board;

  MoveGenerator(
      const std::array<std::array<MoveCTX, 2>, MAX_DEPTH + 1> &_killers,
      const std::array<
          std::array<std::array<std::uint16_t, BOARD_AREA>, BOARD_AREA>, 2>
          &_history,
      const ChessBoard &_board)
      : killers(&_killers), history(&_history), board(_board) {
    pseudoLegal.reserve(MAX_MOVES_IN_A_POSITION);

    buckets[BucketEnum::TT].reserve(TT_RESERVE);
    buckets[BucketEnum::CHECKS].reserve(CHECKS_RESERVE);
    buckets[BucketEnum::GOOD_CAPTURES].reserve(GOOD_CAPTURES_RESERVE);
    buckets[BucketEnum::KILLERS].reserve(KILLERS_RESERVE);
    buckets[BucketEnum::PROMOTIONS].reserve(PROMOTIONS_RESERVE);
    buckets[BucketEnum::HISTORY_HEURISTICS].reserve(HISTORY_HEURISTICS_RESERVE);
    buckets[BucketEnum::BAD_CAPTURES].reserve(BAD_CAPTURES_RESERVE);
    buckets[BucketEnum::QUIET].reserve(QUIET_RESERVE);
  }

  explicit MoveGenerator(const ChessBoard &_board) : board(_board) {
    pseudoLegal.reserve(MAX_MOVES_IN_A_POSITION);

    buckets[BucketEnum::TT].reserve(TT_RESERVE);
    buckets[BucketEnum::CHECKS].reserve(CHECKS_RESERVE);
    buckets[BucketEnum::GOOD_CAPTURES].reserve(GOOD_CAPTURES_RESERVE);
    buckets[BucketEnum::KILLERS].reserve(KILLERS_RESERVE);
    buckets[BucketEnum::PROMOTIONS].reserve(PROMOTIONS_RESERVE);
    buckets[BucketEnum::HISTORY_HEURISTICS].reserve(HISTORY_HEURISTICS_RESERVE);
    buckets[BucketEnum::BAD_CAPTURES].reserve(BAD_CAPTURES_RESERVE);
    buckets[BucketEnum::QUIET].reserve(QUIET_RESERVE);
  }

  void generatePseudoLegal(bool onlyKills, bool forWhites);

  void appendCastling(const ChessBoard &board, bool forWhites);

  void sort(const MoveCTX *entryBestMove, std::uint8_t ply, bool forWhites);

private:
  static constexpr std::uint8_t TT_RESERVE = 1;
  static constexpr std::uint8_t CHECKS_RESERVE = 16;
  static constexpr std::uint8_t GOOD_CAPTURES_RESERVE = 8;
  static constexpr std::uint8_t KILLERS_RESERVE = 2;
  static constexpr std::uint8_t PROMOTIONS_RESERVE = 24;
  static constexpr std::uint8_t HISTORY_HEURISTICS_RESERVE = 32;
  static constexpr std::uint8_t BAD_CAPTURES_RESERVE = 8;
  static constexpr std::uint8_t QUIET_RESERVE = 48;

  std::uint64_t friendlyFlat = 0;
  std::uint64_t enemyFlat = 0;

  void appendContext(MoveCTX &ctx, bool forWhites);
};
