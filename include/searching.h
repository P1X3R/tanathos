#pragma once

#include "bitboard.h"
#include "board.h"
#include "legalMoves.h"
#include "zobrist.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

static constexpr std::int32_t INF = INT32_MAX;

struct TTEntry {
  std::uint64_t key = 0; // Zobrist
  std::int32_t score = 0;
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
      std::bit_floor(TT_SIZE_BYTES / sizeof(TTEntry));
  static constexpr std::uint64_t INDEX_MASK = TT_SIZE - 1;

  std::vector<TTEntry> Table;
};

constexpr std::array<int, Piece::NOTHING + 1> PIECE_VALUES = {
    100, 300, 300, 500, 900, 0, 0};

class Searching {
public:
  Searching() = default;
  Searching(Searching &&) = default;
  Searching(const Searching &) = default;
  auto operator=(Searching &&) -> Searching & = default;
  auto operator=(const Searching &) -> Searching & = default;
  ~Searching() = default;

  ChessBoard board;

  auto search(std::uint8_t depth, std::int32_t alpha, std::int32_t beta,
              MoveCTX &bestMove) -> std::int32_t;

  auto iterativeDeepening(std::uint64_t timeLimitMs) -> MoveCTX;

private:
  TranspositionTable TT;

  std::uint8_t zobristHistoryIndex = 0;
  std::uint64_t endTime = 0;
  std::uint64_t nodes;

  std::array<std::array<MoveCTX, 2>, MAX_DEPTH + 1> killers;
  std::array<std::uint64_t, ZOBRIST_HISTORY_SIZE> zobristHistory;
  std::array<std::array<std::uint16_t, BOARD_AREA>, BOARD_AREA> history;

  [[nodiscard]] auto pickMove(std::vector<MoveCTX> &moves,
                              std::uint8_t moveIndex, const ChessBoard &board,
                              std::uint8_t depth, const MoveCTX *entryBestMove)
      -> const MoveCTX *;

  void appendZobristHistory() {
    zobristHistory[zobristHistoryIndex] = board.zobrist;
    zobristHistoryIndex = (zobristHistoryIndex + 1) % ZOBRIST_HISTORY_SIZE;
  }

  void popZobristHistory() {
    zobristHistoryIndex =
        (zobristHistoryIndex + ZOBRIST_HISTORY_SIZE - 1) % ZOBRIST_HISTORY_SIZE;
    zobristHistory[zobristHistoryIndex] = 0;
  }

  void searchAllMoves(MoveGenerator &generator, std::uint8_t depth,
                      std::int32_t &alpha, std::int32_t beta,
                      std::int32_t &bestScore, std::uint8_t ply, bool forWhites,
                      bool &hasLegalMoves, std::int32_t alphaOriginal,
                      MoveCTX &bestMove);
};
