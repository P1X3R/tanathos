#include "searching.h"
#include "board.h"
#include "legalMoves.h"
#include "move.h"
#include "sysifus.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

static constexpr std::int32_t CHECKMATE_SCORE = 50000;
static constexpr std::int32_t CHECKMATE_THRESHOLD = CHECKMATE_SCORE - 1000;

struct EntryStoringCTX {
  std::uint8_t ply, depth;
  std::int32_t bestScore, alphaOriginal, beta;
};

struct EntryProbingCTX {
  std::uint8_t ply, depth;
  std::int32_t &alpha, &beta;
};

static auto nowMs() -> std::uint64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

static auto probeTTEntry(const TTEntry *entry, EntryProbingCTX &ctx,
                         std::int32_t &outScore, const ChessBoard &board)
    -> bool {
  if (entry->key != board.zobrist || entry->depth < ctx.depth) {
    return false;
  }

  std::int32_t entryScore = entry->score;

#ifndef NDEBUG
  assert(std::abs(entryScore) <= CHECKMATE_SCORE);
#endif

  if (entryScore > CHECKMATE_THRESHOLD) {
    entryScore -= ctx.ply;
  } else if (entryScore < -CHECKMATE_THRESHOLD) {
    entryScore += ctx.ply;
  }

  if (entry->flag == TTEntry::EXACT) {
    outScore = entryScore;
    return true; // Direct hit
  }

  if (entry->flag == TTEntry::LOWERBOUND && entryScore > ctx.alpha) {
    ctx.alpha = entryScore;
  } else if (entry->flag == TTEntry::UPPERBOUND && entryScore < ctx.beta) {
    ctx.beta = entryScore;
  }

  if (ctx.alpha >= ctx.beta) {
    outScore = entryScore; // Cutoff
    return true;
  }

  return false; // Continue search
}

static void storeEntry(const ChessBoard &board, TranspositionTable &table,
                       const MoveCTX &bestMove, const EntryStoringCTX &ctx) {
  TTEntry newEntry;
  newEntry.key = board.zobrist;
  newEntry.bestMove = bestMove;
  newEntry.depth = ctx.depth;
  newEntry.score = ctx.bestScore;

  if (ctx.bestScore <= ctx.alphaOriginal) {
    newEntry.flag = TTEntry::UPPERBOUND;
  } else if (ctx.bestScore >= ctx.beta) {
    newEntry.flag = TTEntry::LOWERBOUND;
  } else {
    newEntry.flag = TTEntry::EXACT;
  }

  if (newEntry.score > CHECKMATE_THRESHOLD) {
    newEntry.score -= ctx.ply;
  } else if (newEntry.score < -CHECKMATE_THRESHOLD) {
    newEntry.score += ctx.ply;
  }

  table.store(newEntry);
}

auto Searching::iterativeDeepening(const std::uint64_t timeLimitMs) -> MoveCTX {
  const std::uint64_t startingTime = nowMs();
  endTime = timeLimitMs + startingTime;

  MoveCTX bestMove;

  for (std::uint8_t depth = 1; depth < MAX_DEPTH; depth++) {
    if (nowMs() >= endTime) {
      break;
    }

#ifndef NDEBUG
    std::cout << "Max depth: " << static_cast<std::int32_t>(depth) << '\n';
#endif // !NDEBUG

    // The search function assigns the best move
    MoveCTX PVMove = search(depth);

    if (nowMs() < endTime) {
      bestMove = PVMove;
    } else {
      break;
    }
  }

  afterSearch();

  endTime = UINT64_MAX;
  return bestMove;
}

auto Searching::search(std::uint8_t depth) -> MoveCTX {
  MoveCTX bestMove;
  std::int32_t bestScore = -INF;

  const bool forWhites = board.whiteToMove;

  MoveGenerator generator;
  generator.generatePseudoLegal(board, forWhites);
  const CastlingRights castlingAttackMask = generateCastlingAttackMask(
      board.getFlat(true) | board.getFlat(false), board);
  generator.appendCastling(board, castlingAttackMask, forWhites);

  for (std::size_t moveIndex = 0; moveIndex < generator.pseudoLegal.size();
       ++moveIndex) {
    const MoveCTX *move =
        pickMove(generator.pseudoLegal, moveIndex, board, depth, nullptr);
    const UndoCTX undo(*move, board);
    makeMove(board, *move);
    appendZobristHistory();

    if (!board.isKingInCheck(forWhites)) {
      std::int32_t score = -negamax(-INF, INF, depth - 1, 1);
      if (score > bestScore) {
        bestScore = score;
        bestMove = *move;
      }
    }

    undoMove(board, undo);
    popZobristHistory();
  }

  return bestMove;
}

auto Searching::negamax(std::int32_t alpha, std::int32_t beta,
                        const std::uint8_t depth, const std::uint8_t ply)
    -> std::int32_t {
  const bool forWhites = board.whiteToMove;
  const auto forWhitesInteger = static_cast<const std::uint8_t>(forWhites);
#ifndef NDEBUG
  nodes++;
#endif // !NDEBUG

  if (board.isDraw(zobristHistory)) {
    return 0;
  }
  if (depth == 0 || nowMs() >= endTime) {
    return board.evaluate();
  }

  const std::uint8_t alphaOriginal = alpha;

  const TTEntry *entry = TT.probe(board.zobrist);
  if (entry != nullptr) {
    std::int32_t entryScore;
    EntryProbingCTX ctx = {
        .ply = ply, .depth = depth, .alpha = alpha, .beta = beta};
    if (probeTTEntry(entry, ctx, entryScore, board)) {
#ifndef NDEBUG
      TTHits++;
#endif // !NDEBUG
      return entryScore;
    }
  }

  std::int32_t bestScore = -INF;
  const MoveCTX *entryBestMove = entry != nullptr ? &entry->bestMove : nullptr;
  MoveCTX bestMove;

  MoveGenerator generator;
  generator.generatePseudoLegal(board, forWhites);
  const CastlingRights castlingAttackMask = generateCastlingAttackMask(
      board.getFlat(true) | board.getFlat(false), board);
  generator.appendCastling(board, castlingAttackMask, forWhites);

  const std::uint64_t enemyKingBitboard =
      (forWhites ? board.blacks : board.whites)[Piece::KING];

  bool hasLegalMoves = false;
  for (std::size_t moveIndex = 0; moveIndex < generator.pseudoLegal.size();
       ++moveIndex) {
    const MoveCTX *move =
        pickMove(generator.pseudoLegal, moveIndex, board, ply, entryBestMove);
    const UndoCTX undo(*move, board);
    makeMove(board, *move);
    appendZobristHistory();

    if (nowMs() >= endTime) {
      undoMove(board, undo);
      popZobristHistory();
      return std::max(board.evaluate(), bestScore);
    }

    if (!board.isKingInCheck(forWhites)) {
      hasLegalMoves = true;

      std::int32_t score;

      const bool isKillerMove =
          killers[ply][0] == *move || killers[ply][1] == *move;
      const bool isCheckMove =
          (1ULL << move->capturedSquare) == enemyKingBitboard;
      static constexpr std::uint16_t HISTORY_GOOD = 1000;
      const bool isGoodMove =
          history[forWhitesInteger][move->from][move->to] > HISTORY_GOOD;
      // const std::uint32_t reduction =
      //     1 + static_cast<std::uint32_t>(std::floor(0.75 * std::log(depth)))
      //     + (moveIndex / 6);
      const std::uint32_t reduction = (depth >= 3 && moveIndex >= 4) ? 1 : 0;

      if (moveIndex == 0 || move->captured != Piece::NOTHING ||
          move->promotion != Piece::NOTHING || isKillerMove || isCheckMove ||
          isGoodMove || depth < 2) {
        score = -negamax(-beta, -alpha, depth - 1, ply + 1);
      } else {
        score = -negamax(-alpha - 1, -alpha,
                         reduction >= depth ? depth - 1 : depth - reduction,
                         ply + 1);
        if (score > alpha && score < beta) {
          score = -negamax(-beta, -alpha, depth - 1, ply + 1);
        }
      }

      if (score > bestScore) {
        bestScore = score;
        bestMove = *move;
      }
      alpha = std::max(score, alpha);

      if (alpha >= beta) {
#ifndef NDEBUG
        cuts++;
#endif

        if (move->captured == Piece::NOTHING) {
          // Store killer moves
          if (killers[ply][0] != *move) {
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = *move;
          }

          history[forWhitesInteger][move->from][move->to] += depth * depth;
          if (history[forWhitesInteger][move->from][move->to] >
              UINT16_MAX - (depth * depth)) {
            history[forWhitesInteger][move->from][move->to] = UINT16_MAX;
          }
        }

        undoMove(board, undo);
        popZobristHistory();
        break;
      }
    }

    undoMove(board, undo);
    popZobristHistory();
  }

  if (!hasLegalMoves) {
    return board.isKingInCheck(forWhites)
               ? -(CHECKMATE_SCORE - ply) // Mate in N
               : 0;                       // Stalemate
  }

  storeEntry(board, TT, bestMove,
             {.ply = ply,
              .depth = depth,
              .bestScore = bestScore,
              .alphaOriginal = alphaOriginal,
              .beta = beta});

  return bestScore;
}
