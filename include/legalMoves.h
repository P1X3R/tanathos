#pragma once

#include "board.h"
#include "sysifus.h"
#include <cstdint>
#include <string>
#include <vector>

static constexpr std::uint8_t MAX_DEPTH = 80;

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

  [[nodiscard]] auto
  score(const MoveCTX *entryBestMove,
        const std::array<std::array<MoveCTX, 2>, MAX_DEPTH + 1> &killers,
        const std::array<
            std::array<std::array<std::uint16_t, BOARD_AREA>, BOARD_AREA>, 2>
            &history,
        std::uint8_t ply, const ChessBoard &board) const -> std::uint16_t;

  [[nodiscard]] auto see(std::uint64_t whitesFlat, const ChessBoard &board,
                         std::uint64_t blacksFlat) const -> std::int32_t;
};

auto fromAlgebraic(const std::string_view &algebraic, const ChessBoard &board)
    -> MoveCTX;

auto moveToUCI(const MoveCTX &move) -> std::string;

constexpr std::uint32_t MAX_MOVES_IN_A_POSITION = 256;

static constexpr std::uint8_t BUCKETS_LEN = 7;
enum BucketEnum : std::uint8_t {
  TT = 0,
  GOOD_CAPTURES,
  KILLERS,
  PROMOTIONS,
  HISTORY_HEURISTICS,
  BAD_CAPTURES,
  QUIET,
};

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

    buckets[BucketEnum::TT].reserve(1);
    buckets[BucketEnum::GOOD_CAPTURES].reserve(8);
    buckets[BucketEnum::KILLERS].reserve(2);
    buckets[BucketEnum::PROMOTIONS].reserve(24);
    buckets[BucketEnum::HISTORY_HEURISTICS].reserve(32);
    buckets[BucketEnum::BAD_CAPTURES].reserve(8);
    buckets[BucketEnum::QUIET].reserve(48);
  }

  explicit MoveGenerator(const ChessBoard &_board) : board(_board) {
    pseudoLegal.reserve(MAX_MOVES_IN_A_POSITION);

    buckets[BucketEnum::TT].reserve(1);
    buckets[BucketEnum::GOOD_CAPTURES].reserve(8);
    buckets[BucketEnum::KILLERS].reserve(2);
    buckets[BucketEnum::PROMOTIONS].reserve(24);
    buckets[BucketEnum::HISTORY_HEURISTICS].reserve(32);
    buckets[BucketEnum::BAD_CAPTURES].reserve(8);
    buckets[BucketEnum::QUIET].reserve(48);
  }

  void generatePseudoLegal(bool onlyKills, bool forWhites);

  void appendCastling(const ChessBoard &board, bool forWhites);

private:
  std::uint64_t friendlyFlat = 0;
  std::uint64_t enemyFlat = 0;

  void appendContext(MoveCTX &ctx, bool forWhites);
};
