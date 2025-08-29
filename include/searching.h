#pragma once

#include "bitboard.h"
#include "board.h"
#include "legalMoves.h"
#include "zobrist.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

static constexpr std::int32_t CHECKMATE_SCORE = 50000;
static constexpr std::int32_t CHECKMATE_THRESHOLD = CHECKMATE_SCORE - 1000;
static constexpr std::int32_t INF = CHECKMATE_SCORE + 1000;
static constexpr std::uint8_t MAX_SEARCHING_DEPTH = 63;

struct TTEntry {
  std::uint64_t key = UINT64_MAX; // Zobrist
  std::int32_t score = 0;
  std::uint8_t depth = 0;
  enum BoundFlag : std::uint8_t {
    EXACT = 0,      // Exact score
    LOWERBOUND = 1, // score >= true value (alpha cut-off)
    UPPERBOUND = 2  // score <= true value (beta cut-off)
  } flag = EXACT;
  MoveCTX bestMove{};
};

class TranspositionTable {
public:
  TranspositionTable() { table.resize(TT_SIZE); }

  std::uint64_t usedEntries = 0;

  [[nodiscard]] auto probe(const std::uint64_t key) const -> const TTEntry * {
    const TTEntry *entry = &table[key & INDEX_MASK];

    if (entry->key == key) {
      return entry;
    }

    return nullptr;
  }

  void store(const TTEntry &newEntry) {
    TTEntry *entry = &table[newEntry.key & INDEX_MASK];
    if (entry->key == UINT64_MAX) {
      usedEntries++;
      *entry = newEntry;
    }
    if (newEntry.depth >= entry->depth) {
      *entry = newEntry;
    }
  }

  void clear() {
    table.clear();
    table.resize(TT_SIZE);
    usedEntries = 0;
  }

  static auto size() -> std::size_t { return TT_SIZE; }

private:
  static constexpr std::size_t TT_SIZE_MB = 64;
  static constexpr std::size_t MB_TO_BYTE_SCALE_FACTOR = 1048576;

  static constexpr std::size_t TT_SIZE_BYTES =
      TT_SIZE_MB * MB_TO_BYTE_SCALE_FACTOR;
  // Round down to power of 2
  static constexpr std::size_t TT_SIZE =
      std::bit_floor(TT_SIZE_BYTES / sizeof(TTEntry));

  static const std::uint64_t INDEX_MASK = TT_SIZE - 1;

  std::vector<TTEntry> table;
};

constexpr std::array<std::int32_t, Piece::NOTHING + 1> PIECE_VALUES = {
    100, 320, 330, 500, 900, 20000, 0};

enum class NodeType : std::uint8_t {
  PV,
  NonPV,
};

class Searching {
public:
  explicit Searching(ChessBoard &_board) : board(_board) {
    zobristHistory.fill(~0ULL);
    for (auto &color : history) {
      for (auto &row : color) {
        row.fill(0); // Initialize history to 0
      }
    }
    for (auto &killer : killers) {
      killer.fill(MoveCTX{}); // Initialize killers to empty moves
    }
  }; // ~0ULL means no key
  Searching(Searching &&) = default;
  Searching(const Searching &) = default;
  auto operator=(Searching &&) -> Searching & = delete;
  auto operator=(const Searching &) -> Searching & = delete;
  ~Searching() = default;

  ChessBoard &board;
  std::uint64_t nodes = 0;
  std::uint64_t seldepth = 0;

  [[nodiscard]] auto search(std::uint8_t depth)
      -> std::pair<MoveCTX, std::int32_t>;

  auto iterativeDeepening(std::uint64_t timeLimitMs) -> MoveCTX;

  void afterSearch() {
    auto reduceOldBonusColor = [&](const std::uint8_t forWhites) {
      for (std::uint32_t fromSquare = 0; fromSquare < BOARD_AREA;
           fromSquare++) {
        for (std::uint32_t toSquare = 0; toSquare < BOARD_AREA; toSquare++) {
          history[forWhites][fromSquare][toSquare] >>= 1;
        }
      }
    };

    reduceOldBonusColor(1);
    reduceOldBonusColor(0);

    for (auto &depth : killers) {
      depth.fill(MoveCTX());
    }

    TT.clear();

    nodes = 0;
    seldepth = 0;
  }

  void appendZobristHistory() {
    zobristHistory[zobristHistoryIndex] = board.zobrist;
    zobristHistoryIndex = (zobristHistoryIndex + 1) % ZOBRIST_HISTORY_SIZE;
  }

  void clear() {
    TT.clear();

    startingTime = 0;
    endTime = UINT64_MAX;

    for (auto &depth : killers) {
      depth.fill(MoveCTX());
    }

    zobristHistoryIndex = 0;
    zobristHistory.fill(~0ULL);

    for (auto &color : history) {
      for (auto &row : color) {
        row.fill(0);
      }
    }

    nodes = 0;
    seldepth = 0;
  }

private:
  TranspositionTable TT;

  std::uint8_t zobristHistoryIndex = 0;
  std::uint64_t endTime = UINT64_MAX;
  std::uint64_t startingTime = UINT64_MAX;

  std::array<std::array<MoveCTX, 2>, MAX_DEPTH + 1> killers{};
  std::array<std::uint64_t, ZOBRIST_HISTORY_SIZE> zobristHistory{};
  std::array<std::array<std::array<std::uint16_t, BOARD_AREA>, BOARD_AREA>, 2>
      history{};

  std::int32_t lastScore = 0;

  template <NodeType nodeType>
  [[nodiscard]] auto negamax(std::int32_t alpha, std::int32_t beta,
                             std::uint8_t depth, std::uint8_t ply)
      -> std::int32_t;
  [[nodiscard]] auto quiescene(std::int32_t alpha, std::int32_t beta,
                               std::uint8_t ply) -> std::int32_t;

  void popZobristHistory() {
    zobristHistoryIndex =
        (zobristHistoryIndex + ZOBRIST_HISTORY_SIZE - 1) % ZOBRIST_HISTORY_SIZE;
    zobristHistory[zobristHistoryIndex] = ~0ULL;
  }
};
