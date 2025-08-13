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
};

auto fromAlgebraic(const std::string_view &algebraic, const ChessBoard &board)
    -> MoveCTX;

void insertMoveInfo(MoveCTX &partial, const ChessBoard &board,
                    bool getOriginalType, bool isPieceWhite);

auto moveToUCI(const MoveCTX &move) -> std::string;

void appendContext(MoveCTX &ctx, bool forWhites,
                   const std::array<std::uint64_t, Piece::KING + 1> &enemyColor,
                   std::uint64_t enemyFlat, std::vector<MoveCTX> &pseudoLegal,
                   std::int32_t enPassantSquare);

auto generateCastlingAttackMask(std::uint64_t flat, const ChessBoard &board)
    -> CastlingRights;

constexpr std::uint32_t MAX_MOVES_IN_A_POSITION = 256;

// For only one color
struct MoveGenerator {
  std::vector<MoveCTX> pseudoLegal;

  MoveGenerator() { pseudoLegal.reserve(MAX_MOVES_IN_A_POSITION); }

  void generatePseudoLegal(const ChessBoard &board, bool onlyKills,
                           bool forWhites);

  void appendCastling(const ChessBoard &board,
                      const CastlingRights &castlingAttackMask, bool forWhites);
};
