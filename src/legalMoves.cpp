#include "legalMoves.h"
#include "bitboard.h"
#include "board.h"
#include "sysifus.h" // for getPseudoLegal
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>

void appendContext(MoveCTX &ctx, const bool forWhites,
                   const std::array<std::uint64_t, Piece::KING + 1> &enemyColor,
                   const std::uint64_t enemyFlat,
                   std::vector<MoveCTX> &pseudoLegal,
                   const std::int32_t enPassantSquare) {
  const std::int32_t capturedPawnSquare = forWhites
                                              ? enPassantSquare - BOARD_LENGTH
                                              : enPassantSquare + BOARD_LENGTH;

  const bool isEnPassantCapture =
      ctx.original == Piece::PAWN && enPassantSquare != 0 &&
      std::abs(ctx.from - capturedPawnSquare) == 1 && ctx.to == enPassantSquare;

  const std::uint64_t toBit = 1ULL << ctx.to;

  ctx.capturedSquare = ctx.to;
  if (isEnPassantCapture) {
    ctx.capturedSquare = capturedPawnSquare;
    ctx.to = enPassantSquare;
    ctx.captured = Piece::PAWN;
  } else if ((enemyFlat & toBit) != 0) {
    // Handle regular captures
    for (std::uint32_t enemyType = Piece::PAWN; enemyType <= Piece::KING;
         enemyType++) {
      if ((enemyColor[enemyType] & toBit) != 0) {
        ctx.captured = static_cast<Piece>(enemyType);
        break;
      }
    }
  } else {
    ctx.captured = Piece::NOTHING;
  }

  const auto toRank = static_cast<std::int8_t>(ctx.to / BOARD_LENGTH);
  const std::int8_t promotionRank = forWhites ? 7 : 0;
  if (ctx.original == Piece::PAWN && toRank == promotionRank) {
    // Generate promotion moves for Knight, Bishop, Rook, Queen
    for (std::uint32_t promotion = Piece::KNIGHT; promotion <= Piece::QUEEN;
         promotion++) {
      ctx.promotion = static_cast<Piece>(promotion);
      pseudoLegal.push_back(ctx);
    }
  } else {
    pseudoLegal.push_back(ctx);
  }
}

void MoveGenerator::generatePseudoLegal(const ChessBoard &board,
                                        const bool onlyKills,
                                        const bool forWhites) {
  const std::array<std::uint64_t, Piece::KING + 1> &color =
      forWhites ? board.whites : board.blacks;
  const std::array<std::uint64_t, Piece::KING + 1> &enemyColor =
      forWhites ? board.blacks : board.whites;

  const std::uint64_t friendlyFlat = board.getFlat(forWhites);
  const std::uint64_t enemyFlat = board.getFlat(!forWhites);

  for (std::uint32_t type = Piece::PAWN; type <= Piece::KING; type++) {
    std::uint64_t typeBitboard = color[type];

    while (typeBitboard != 0) {
      const auto fromSquare =
          static_cast<std::int8_t>(std::countr_zero(typeBitboard));
      const std::uint64_t enemyFlatWithEnPassant =
          type == Piece::PAWN ? enemyFlat | (1ULL << board.enPassantSquare)
                              : enemyFlat;
      Move pseudoLegalMoves = (Move){0, 0};
      if (onlyKills) {
        pseudoLegalMoves.kills =
            getKills(static_cast<Piece>(type), fromSquare, friendlyFlat,
                     forWhites, enemyFlatWithEnPassant);
      } else {
        pseudoLegalMoves =
            getPseudoLegal(static_cast<Piece>(type), fromSquare, friendlyFlat,
                           forWhites, enemyFlatWithEnPassant);
      }

      std::uint64_t pseudoLegalBits =
          pseudoLegalMoves.quiet | pseudoLegalMoves.kills;

      while (pseudoLegalBits != 0) {
        // Captured info and promotion are updated on pushContext
        MoveCTX ctx = {
            .from = static_cast<std::uint32_t>(fromSquare),
            .to = static_cast<std::uint32_t>(std::countr_zero(pseudoLegalBits)),
            .capturedSquare = 0,
            .original = static_cast<Piece>(type),
            .captured = Piece::NOTHING,
            .promotion = Piece::NOTHING,
        };

        appendContext(ctx, forWhites, enemyColor, enemyFlat, pseudoLegal,
                      static_cast<std::int8_t>(board.enPassantSquare));

        pseudoLegalBits &= pseudoLegalBits - 1;
      }

      typeBitboard &= typeBitboard - 1;
    }
  }
}

static auto checkCastlingPath(const std::uint64_t piecePath,
                              const std::array<std::int32_t, 3> &attackPath,
                              const std::uint64_t flatBoard, const bool isBlack,
                              const bool castlingRight, const ChessBoard &board)
    -> bool {
  if (!castlingRight) {
    return false;
  }

  return std::ranges::none_of(attackPath,
                              [&](const std::int32_t square) -> bool {
                                return board.isSquareUnderAttack(square,
                                                                 isBlack);
                              }) &&
         (flatBoard & piecePath) == 0;
}

struct CastlingPath {
  uint64_t piecePath;
  std::array<int32_t, 3> attackPath;
};

auto generateCastlingAttackMask(const std::uint64_t flat,
                                const ChessBoard &board) -> CastlingRights {
  static constexpr std::array<CastlingPath, 4> CASTLING_PATHS = {{
      // White king-side
      {.piecePath = (1ULL << F1) | (1ULL << G1), .attackPath = {E1, F1, G1}},
      // White queen-side
      {.piecePath = (1ULL << B1) | (1ULL << C1) | (1ULL << D1),
       .attackPath = {E1, D1, C1}},
      // Black king-side
      {.piecePath = (1ULL << F8) | (1ULL << G8), .attackPath = {E8, F8, G8}},
      // Black queen-side
      {.piecePath = (1ULL << B8) | (1ULL << C8) | (1ULL << D8),
       .attackPath = {E8, D8, C8}},
  }};

  return {
      .whiteKingSide = checkCastlingPath(
          CASTLING_PATHS[0].piecePath, CASTLING_PATHS[0].attackPath, flat,
          false, board.castlingRights.whiteKingSide, board),
      .whiteQueenSide = checkCastlingPath(
          CASTLING_PATHS[1].piecePath, CASTLING_PATHS[1].attackPath, flat,
          false, board.castlingRights.whiteQueenSide, board),
      .blackKingSide = checkCastlingPath(
          CASTLING_PATHS[2].piecePath, CASTLING_PATHS[2].attackPath, flat, true,
          board.castlingRights.blackKingSide, board),
      .blackQueenSide = checkCastlingPath(
          CASTLING_PATHS[3].piecePath, CASTLING_PATHS[3].attackPath, flat, true,
          board.castlingRights.blackQueenSide, board),
  };
}

void MoveGenerator::appendCastling(const ChessBoard &board,
                                   const CastlingRights &castlingAttackMask,
                                   const bool forWhites) {
  std::uint64_t castleMask = 0;

  if (forWhites) {
    const bool canKingSide =
        board.castlingRights.whiteKingSide && castlingAttackMask.whiteKingSide;
    const bool canQueenSide = board.castlingRights.whiteQueenSide &&
                              castlingAttackMask.whiteQueenSide;

    castleMask = (canKingSide ? (1ULL << BoardSquare::G1) : 0) |
                 (canQueenSide ? (1ULL << BoardSquare::C1) : 0);
  } else {
    const bool canKingSide =
        board.castlingRights.blackKingSide && castlingAttackMask.blackKingSide;
    const bool canQueenSide = board.castlingRights.blackQueenSide &&
                              castlingAttackMask.blackQueenSide;

    castleMask = (canKingSide ? (1ULL << BoardSquare::G8) : 0) |
                 (canQueenSide ? (1ULL << BoardSquare::C8) : 0);
  }

  MoveCTX ctx = {
      .from = forWhites ? BoardSquare::E1 : BoardSquare::E8,
      .to = 0,
      .capturedSquare = 0,
      .original = Piece::KING,
      .captured = Piece::NOTHING,
      .promotion = Piece::NOTHING,
  };

  while (castleMask != 0) {
    ctx.to = std::countr_zero(castleMask);

    pseudoLegal.push_back(ctx);

    castleMask &= castleMask - 1;
  }
}
