#include "searching.h"
#include "board.h"
#include "legalMoves.h"
#include "move.h"
#include "sysifus.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <ostream>

static constexpr std::uint8_t REDUCTION_MAX_DEPTH = 13;
static constexpr std::uint8_t REDUCTION_MAX_MOVE_INDEX = 218;
static const std::array<std::array<std::uint8_t, REDUCTION_MAX_MOVE_INDEX>,
                        REDUCTION_MAX_DEPTH>
    REDUCTION_TABLE = []() {
      std::array<std::array<std::uint8_t, REDUCTION_MAX_MOVE_INDEX>,
                 REDUCTION_MAX_DEPTH>
          result;

      for (std::uint32_t depth = 0; depth < REDUCTION_MAX_DEPTH; depth++) {
        for (std::uint32_t moveIndex = 0; moveIndex < REDUCTION_MAX_MOVE_INDEX;
             moveIndex++) {
          const std::uint32_t reduction =
              1 +
              static_cast<std::uint32_t>(std::log2(moveIndex + 1) * depth / 3);

          result[depth][moveIndex] = reduction;
        }
      }
      return result;
    }();

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
  assert(std::abs(entryScore) <= CHECKMATE_SCORE + MAX_SEARCHING_DEPTH &&
         "Storing score out of reasonable bounds");
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
  if (bestMove == MoveCTX() || std::abs(ctx.bestScore) >= INF) {
    return;
  }

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
    newEntry.score += ctx.ply;
  } else if (newEntry.score < -CHECKMATE_THRESHOLD) {
    newEntry.score -= ctx.ply;
  }

  table.store(newEntry);
}

auto Searching::iterativeDeepening(const std::uint64_t timeLimitMs) -> MoveCTX {
  startingTime = nowMs();
  endTime = timeLimitMs + startingTime;

  MoveCTX bestMove;

  for (std::uint8_t depth = 1; depth < MAX_SEARCHING_DEPTH; depth++) {
    if (nowMs() >= endTime) {
      break;
    }

    seldepth = 0;

    // The search function assigns the best move
    const auto [PVMove, bestScore] = search(depth);

    if (nowMs() < endTime) {
      bestMove = PVMove;

      const double elapsedTimeSeconds =
          static_cast<double>(nowMs() - startingTime) / 1000;
      const double nps = static_cast<double>(nodes) / elapsedTimeSeconds;

      static constexpr std::uint16_t HASHFULL_MULTIPLIER = 1000;

      std::cout << "info depth " << static_cast<std::uint64_t>(depth)
                << " seldepth " << seldepth << " score cp " << bestScore
                << " nodes " << nodes << " nps "
                << static_cast<std::uint64_t>(nps) << " hashfull "
                << static_cast<std::uint64_t>(
                       static_cast<double>(TT.usedEntries) /
                       static_cast<double>(TranspositionTable::size()) *
                       HASHFULL_MULTIPLIER)
                << " pv " << moveToUCI(bestMove) << '\n';
      std::flush(std::cout);
    } else {
      break;
    }
  }

  afterSearch();

  endTime = UINT64_MAX;
  startingTime = 0;

  return bestMove;
}

auto Searching::search(const std::uint8_t depth)
    -> std::pair<MoveCTX, std::int32_t> {
  MoveCTX bestMove;
  std::int32_t bestScore = -INF;
  const bool forWhites = board.whiteToMove;

  static constexpr std::uint8_t BASE_DELTA = 30;
  std::uint8_t delta = BASE_DELTA;
  std::int32_t alpha;
  std::int32_t beta;

  static constexpr std::uint8_t MIN_DEPTH_FOR_ASPIRATION = 4;
  if (depth > MIN_DEPTH_FOR_ASPIRATION &&
      std::abs(lastScore) < CHECKMATE_THRESHOLD) {
    alpha = lastScore - delta;
    beta = lastScore + delta;
  } else {
    alpha = -INF;
    beta = INF;
  }

  const TTEntry *entry = TT.probe(board.zobrist);
  const MoveCTX *entryBestMove =
      entry != nullptr && entry->depth != 0 ? &entry->bestMove : nullptr;

  if (entry != nullptr && entry->depth >= depth - 2) {
    lastScore = entry->score;
  }

  bool foundMove = false;

  MoveGenerator generator(killers, history, board);
  generator.generatePseudoLegal(false, forWhites);
  generator.appendCastling(board, forWhites);
  generator.sort(entryBestMove, 0, forWhites);

  auto searchMoves = [&](const std::int32_t currentAlpha,
                         const std::int32_t currentBeta) {
    bestScore = -INF;
    for (BucketEnum bucket = BucketEnum::TT; bucket <= BucketEnum::QUIET;
         ++bucket) {
      for (const MoveCTX &move : generator.buckets[bucket]) {
        const UndoCTX undo(move, board);
        makeMove(board, move);
        appendZobristHistory();

        if (!board.isKingInCheck(forWhites)) {
          foundMove = true;
          const std::int32_t score =
              -negamax(-currentBeta, -currentAlpha, depth - 1, 1);

          if (score > bestScore) {
            bestScore = score;
            bestMove = move;
          }
        }
        undoMove(board, undo);
        popZobristHistory();
      }
    }
  };

  searchMoves(alpha, beta);

  if (!foundMove) {
    return {MoveCTX(),
            board.isKingInCheck(board.whiteToMove) ? -CHECKMATE_SCORE : 0};
  }

  if (bestScore <= alpha || bestScore >= beta) {
    searchMoves(-INF, INF);
  }

  lastScore = bestScore;
  return {bestMove, bestScore};
}

auto Searching::negamax(std::int32_t alpha, std::int32_t beta,
                        const std::uint8_t depth, const std::uint8_t ply)
    -> std::int32_t {
  const bool forWhites = board.whiteToMove;
  const auto forWhitesInteger = static_cast<const std::uint8_t>(forWhites);

  if (depth == 0) {
    return quiescene(alpha, beta, ply);
  }

  seldepth = std::max(seldepth, static_cast<std::uint64_t>(ply));
  nodes++;

  if (board.isDraw(zobristHistory)) {
    return 0;
  }
  const std::int32_t staticEvaluation =
      forWhites ? board.evaluate() : -board.evaluate();
  if (nowMs() >= endTime) {
    return staticEvaluation;
  }

  const std::int32_t alphaOriginal = alpha;

  const TTEntry *entry = TT.probe(board.zobrist);
  if (entry != nullptr) {
    std::int32_t entryScore;
    EntryProbingCTX ctx = {
        .ply = ply, .depth = depth, .alpha = alpha, .beta = beta};
    if (probeTTEntry(entry, ctx, entryScore, board)) {
      return entryScore;
    }
  }

  const auto &enemyColor = forWhites ? board.blacks : board.whites;

  Piece opponentBestPiece = Piece::QUEEN;
  for (; opponentBestPiece >= Piece::PAWN && enemyColor[opponentBestPiece] == 0;
       opponentBestPiece = static_cast<Piece>(
           static_cast<std::uint8_t>(opponentBestPiece) - 1)) {
  }
  const bool inCheck = board.isKingInCheck(forWhites);
  const bool canFutilityPrune = depth == 1 && !inCheck;
  constexpr std::int32_t FUTILITY_MARGIN = 200;

  std::int32_t bestScore = -INF;
  const MoveCTX *entryBestMove =
      entry != nullptr && entry->depth != 0 ? &entry->bestMove : nullptr;
  MoveCTX bestMove;

  MoveGenerator generator(killers, history, board);
  generator.generatePseudoLegal(false, forWhites);
  generator.appendCastling(board, forWhites);
  generator.sort(entryBestMove, ply, forWhites);

  const std::int32_t mateScore = CHECKMATE_SCORE - ply; // Mate in N

  bool hasLegalMoves = false;
  std::uint8_t moveIndex = 0;
  for (BucketEnum bucket = BucketEnum::TT; bucket <= BucketEnum::QUIET;
       ++bucket) {
    for (const MoveCTX &move : generator.buckets[bucket]) {
      const UndoCTX undo(move, board);
      makeMove(board, move);
      appendZobristHistory();

      if (!board.isKingInCheck(forWhites)) {
        hasLegalMoves = true;

        const bool isQuiet = bucket == QUIET || bucket == KILLERS ||
                             bucket == HISTORY_HEURISTICS;
        if (canFutilityPrune && staticEvaluation + FUTILITY_MARGIN < alpha &&
            isQuiet) {
          undoMove(board, undo);
          popZobristHistory();
          continue;
        }

        std::int32_t score;

        constexpr std::uint16_t HISTORY_GOOD = 1000;
        const bool isGoodMove =
            history[forWhitesInteger][move.from][move.to] > HISTORY_GOOD;

        // Not reduce if bucket is: TT, checks, good capture, killer or
        // promotion
        if (bucket <= BucketEnum::PROMOTIONS || inCheck || moveIndex == 0 ||
            isGoodMove || depth < 2) {
          score = -negamax(-beta, -alpha, depth - 1, ply + 1);
        } else {
          score = -negamax(
              -alpha - 1, -alpha,
              std::max(1, depth - REDUCTION_TABLE[depth][moveIndex]), ply + 1);
          if (score > alpha && score < beta) {
            score = -negamax(-beta, -alpha, depth - 1, ply + 1);
          }
        }

        if (score > bestScore) {
          bestScore = score;
          bestMove = move;
        }
        alpha = std::max(score, alpha);

        if (mateScore < beta) {
          beta = mateScore;
          if (alpha >= mateScore) {
            undoMove(board, undo);
            popZobristHistory();
            return -mateScore;
          }
        }

        if (nowMs() >= endTime) {
          undoMove(board, undo);
          popZobristHistory();
          return 0;
        }

        if (alpha >= beta) {
          if (move.captured == Piece::NOTHING) {
            // Store killer moves
            if (killers[ply][0] != move) {
              killers[ply][1] = killers[ply][0];
              killers[ply][0] = move;
            }

            std::uint16_t &entry =
                history[forWhitesInteger][move.from][move.to];
            const std::uint16_t bonus = depth * depth;
            entry = (entry > UINT16_MAX - bonus) ? UINT16_MAX : entry + bonus;
          }

          undoMove(board, undo);
          popZobristHistory();
          goto searchEnd;
        }
      }

      undoMove(board, undo);
      popZobristHistory();
      moveIndex++;
    }
  }

searchEnd:
  if (!hasLegalMoves) {
    // If king is in check it's checkmate, if no it's stalemate
    return inCheck ? -mateScore : 0;
  }

  storeEntry(board, TT, bestMove,
             {.ply = ply,
              .depth = depth,
              .bestScore = bestScore,
              .alphaOriginal = alphaOriginal,
              .beta = beta});

  return bestScore;
}

[[nodiscard]] auto Searching::quiescene(std::int32_t alpha, std::int32_t beta,
                                        const std::uint8_t ply)
    -> std::int32_t {
  const bool forWhites = board.whiteToMove;
  const std::int32_t alphaOriginal = alpha;

  nodes++;
  seldepth = std::max(seldepth, static_cast<std::uint64_t>(ply));

  const std::int32_t staticEvaluation =
      forWhites ? board.evaluate() : -board.evaluate();
  std::int32_t bestValue = staticEvaluation;
  if (bestValue >= beta) {
    return bestValue;
  }
  alpha = std::max(bestValue, alpha);

  if (nowMs() >= endTime) {
    return bestValue;
  }

  const bool isInCheck = board.isKingInCheck(forWhites);

  const TTEntry *entry = TT.probe(board.zobrist);

  // This generates only pseudo-legal kills, that's why's the `true` flag there
  MoveGenerator generator(killers, history, board);
  generator.generatePseudoLegal(!isInCheck, forWhites);
  generator.sort(entry != nullptr ? &entry->bestMove : nullptr, ply, forWhites);

  for (BucketEnum bucket = BucketEnum::TT; bucket <= BucketEnum::QUIET;
       ++bucket) {
    if (!isInCheck && bucket != GOOD_CAPTURES) {
      continue;
    }

    for (const MoveCTX &move : generator.buckets[bucket]) {
      const UndoCTX undo(move, board);
      makeMove(board, move);
      appendZobristHistory();

      std::int32_t score = -INF;
      if (!board.isKingInCheck(forWhites)) {
        score = -quiescene(-beta, -alpha, ply + 1);
      }

      undoMove(board, undo);
      popZobristHistory();

      if (score >= beta) {
        storeEntry(board, TT, move,
                   {.ply = ply,
                    .depth = 0,
                    .bestScore = score,
                    .alphaOriginal = alphaOriginal,
                    .beta = beta});
        return score;
      }

      bestValue = std::max(score, bestValue);
      alpha = std::max(score, alpha);

      if (nowMs() >= endTime) {
        return bestValue;
      }
    }
  }

  storeEntry(board, TT, MoveCTX(),
             {.ply = ply,
              .depth = 0,
              .bestScore = bestValue,
              .alphaOriginal = alphaOriginal,
              .beta = beta});

  return bestValue;
}
