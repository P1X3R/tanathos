#pragma once

#include "bitboard.h"
#include "board.h"
#include "legalMoves.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

struct TTEntry {
  std::uint64_t key = 0; // Zobrist
  std::int16_t score = 0;
  std::uint8_t depth = 0;
  enum BoundFlag : std::uint8_t {
    EXACT = 0,      // Exact score
    LOWERBOUND = 1, // score >= true value (alpha cut-off)
    UPPERBOUND = 2  // score <= true value (beta cut-off)
  } flag = EXACT;
  MoveCTX bestMove;
};

class TranspositionTable {
public:
  TranspositionTable() { Table.resize(TT_SIZE); }

  [[nodiscard]] auto probe(const std::uint64_t key) const -> const TTEntry * {
    const TTEntry *entry = &Table[key & INDEX_MASK];

    if (entry->key == key) {
      return entry;
    }

    return nullptr;
  }

  void store(const TTEntry &newEntry) {
    TTEntry *entry = &Table[newEntry.key & INDEX_MASK];
    if (entry->key != newEntry.key || newEntry.depth >= entry->depth) {
      *entry = newEntry;
    }
  }

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

constexpr std::array<int, Piece::NOTHING + 1> PIECE_VALUES = {
    100, 300, 300, 500, 900, 0, 0};

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

  static constexpr std::size_t ZOBRIST_HISTORY_CAPACITY = 10;

  std::array<std::array<MoveCTX, MAX_DEPTH>, 2> killers;
  std::array<std::uint64_t, ZOBRIST_HISTORY_CAPACITY> zobristHistory;
  std::array<std::array<std::uint16_t, BOARD_AREA>, BOARD_AREA> history;

  [[nodiscard]] auto pickMove(std::vector<MoveCTX> &moves,
                              std::uint8_t moveIndex, const ChessBoard &board,
                              std::uint8_t depth) -> const MoveCTX *;
};
