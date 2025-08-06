#pragma once

#include "bitboard.h"
#include "legalMoves.h"
#include <bit>
#include <cstdint>
#include <vector>

struct TTEntry {
  std::uint64_t key = 0; // Zobrist
  std::int32_t score = 0;
  std::int8_t depth : 3 = 0;
  enum BoundFlag : std::uint8_t {
    EXACT = 0,      // Exact score
    LOWERBOUND = 1, // score >= true value (alpha cut-off)
    UPPERBOUND = 2  // score <= true value (beta cut-off)
  } flag : 2 = EXACT;
  MoveCTX bestMove;
};

class TranspositionTable {
public:
  TranspositionTable() { Table.reserve(TT_SIZE); }

private:
  static constexpr std::size_t TT_SIZE_MB = 10;
  static constexpr std::size_t MB_TO_BYTE_SCALE_FACTOR = 1048576;

  static constexpr std::size_t TT_SIZE_BYTES =
      TT_SIZE_MB * MB_TO_BYTE_SCALE_FACTOR;
  // Round down to power of 2
  static constexpr std::size_t TT_SIZE =
      1ULL << (BOARD_AREA - 1 -
               std::countr_zero(TT_SIZE_BYTES / sizeof(TTEntry)));
  static constexpr std::uint64_t INDEX_MASK = TT_SIZE - 1;

  std::vector<TTEntry> Table;
};

class Searching {
public:
  Searching();
  Searching(Searching &&) = default;
  Searching(const Searching &) = default;
  auto operator=(Searching &&) -> Searching & = default;
  auto operator=(const Searching &) -> Searching & = default;
  ~Searching();

private:
  TranspositionTable TT;
};
