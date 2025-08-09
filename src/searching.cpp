#include "searching.h"
#include "board.h"
#include "legalMoves.h"
#include "move.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>

static constexpr std::int32_t INF = INT32_MAX;
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

static auto probeTTEntry(const TTEntry *entry, EntryProbingCTX &ctx,
                         std::int32_t &outScore) -> bool {
  if (entry == nullptr || entry->depth < ctx.depth) {
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
    newEntry.score += ctx.ply;
  } else if (newEntry.score < -CHECKMATE_THRESHOLD) {
    newEntry.score -= ctx.ply;
  }

  table.store(newEntry);
}

auto Searching::search(std::uint8_t depth, std::int32_t alpha,
                       std::int32_t beta) -> std::int32_t {
  if (depth == 0) {
    return board.evaluate();
  }
  if (board.isDraw(zobristHistory)) {
    return 0;
  }

  const bool forWhites = board.whiteToMove;
  const std::int32_t alphaOriginal = alpha;
  const std::uint8_t ply = MAX_DEPTH - depth;
  std::int32_t bestScore = -INF;

  if (const TTEntry *entry = TT.probe(board.zobrist)) {
    std::int32_t entryScore;
    EntryProbingCTX ctx = {
        .ply = ply, .depth = depth, .alpha = alpha, .beta = beta};
    if (probeTTEntry(entry, ctx, entryScore)) {
      return entryScore;
    }
  }

  MoveGenerator generator;
  generator.generatePseudoLegal(board, forWhites);
  const CastlingRights castlingAttackMask = generateCastlingAttackMask(
      board.getFlat(true) | board.getFlat(false), board);
  generator.appendCastling(board, castlingAttackMask, forWhites);

  bool hasLegalMoves = false;
  searchAllMoves(generator, depth, alpha, beta, bestScore, ply, forWhites,
                 hasLegalMoves, alphaOriginal);

  if (!hasLegalMoves) {
    return board.isKingInCheck(forWhites)
               ? -CHECKMATE_SCORE + depth // Mate in N
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

void Searching::searchAllMoves(MoveGenerator &generator,
                               const std::uint8_t depth, std::int32_t &alpha,
                               const std::int32_t beta, std::int32_t &bestScore,
                               const std::uint8_t ply, const bool forWhites,
                               bool &hasLegalMoves,
                               const std::int32_t alphaOriginal) {
  const TTEntry *entry = TT.probe(board.zobrist);

  for (std::size_t moveIndex = 0; moveIndex < generator.pseudoLegal.size();
       ++moveIndex) {
    const MoveCTX *move =
        pickMove(generator.pseudoLegal, moveIndex, board, depth,
                 entry != nullptr ? &entry->bestMove : nullptr);
    if (move == nullptr) {
      break;
    }

    const UndoCTX undo(*move, board);
    makeMove(board, *move);
    appendZobristHistory();

    if (board.isKingInCheck(forWhites)) {
      undoMove(board, undo);
      continue;
    }

    hasLegalMoves = true;

    std::int32_t score = 0;
    if (moveIndex == 0) {
      score = -search(depth - 1, -beta, -alpha);
    } else {
      score = -search(depth - 1, -alpha - 1, -alpha);
      if (score > alpha && beta - alpha > 1) {
        score = -search(depth - 1, -beta, -alpha);
      }
    }

    undoMove(board, undo);
    popZobristHistory();

    if (score > bestScore) {
      bestScore = score;
      bestMove = *move;
    }
    if (score >= beta) {
      if (move->captured == Piece::NOTHING) {
        // Store killer moves
        if (killers[depth][0] != *move) {
          killers[depth][1] = killers[depth][0];
          killers[depth][0] = *move;
        }

        history[move->from][move->to] = depth * depth;
      }

      return; // Cutoff
    }
    alpha = std::max(score, alpha);
  }
}
