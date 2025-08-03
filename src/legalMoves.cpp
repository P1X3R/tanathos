#include "legalMoves.h"
#include "bitboard.h"
#include "board.h"
#include "sysifus.h" // for getPseudoLegal
#include <bit>
#include <cstdint>
#include <cstdlib>

void appendContext(MoveCTX &ctx, const bool forWhites,
                   const std::array<std::uint64_t, Piece::KING + 1> &enemyColor,
                   const std::uint64_t enemyFlat,
                   std::vector<MoveCTX> &pseudoLegal,
                   const std::int32_t enPassantSquare) {
  // Adjust capturedSquare for en passant capture
  const std::int32_t enPassantCapture = forWhites
                                            ? enPassantSquare - BOARD_LENGTH
                                            : enPassantSquare + BOARD_LENGTH;

  const bool isEnPassantCapture =
      ctx.original == Piece::PAWN && enPassantSquare != 0 &&
      std::abs(ctx.from - enPassantCapture) == 1 && ctx.to == enPassantSquare &&
      (enemyColor[Piece::PAWN] & (1ULL << enPassantCapture)) != 0;

  const std::uint64_t toBit = 1ULL << ctx.to;

  ctx.capturedSquare = ctx.to;
  if (isEnPassantCapture) {
    ctx.capturedSquare = enPassantCapture;
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
      const Move pseudoLegalMoves =
          getPseudoLegal(static_cast<Piece>(type), fromSquare, friendlyFlat,
                         forWhites, enemyFlatWithEnPassant);
      std::uint64_t pseudoLegalBits =
          pseudoLegalMoves.quiet | pseudoLegalMoves.kills;

      kills |= pseudoLegalMoves.kills;

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

auto generateCastlingAttackMask(const std::uint64_t flat,
                                const std::uint64_t whiteKills,
                                const std::uint64_t blackKills)
    -> CastlingRights {

  CastlingRights castlingAttackMask = {
      .whiteKingSide = false,
      .whiteQueenSide = false,
      .blackKingSide = false,
      .blackQueenSide = false,
  };

  // White Castling Checks
  // King-side (O-O)
  static constexpr std::uint64_t whiteKingSidePiecePath =
      (1ULL << BoardSquare::F1) | (1ULL << BoardSquare::G1);
  static constexpr std::uint64_t whiteKingSideAttackPath =
      (1ULL << BoardSquare::E1) | (1ULL << BoardSquare::F1) |
      (1ULL << BoardSquare::G1);

  castlingAttackMask.whiteKingSide =
      ((flat & whiteKingSidePiecePath) == 0) &&
      ((blackKills & whiteKingSideAttackPath) == 0);

  // Queen-side (O-O-O)
  static constexpr std::uint64_t whiteQueenSidePiecePath =
      (1ULL << BoardSquare::B1) | (1ULL << BoardSquare::C1) |
      (1ULL << BoardSquare::D1);
  static constexpr std::uint64_t whiteQueenSideAttackPath =
      (1ULL << BoardSquare::E1) | (1ULL << BoardSquare::D1) |
      (1ULL << BoardSquare::C1);

  castlingAttackMask.whiteQueenSide =
      ((flat & whiteQueenSidePiecePath) == 0) &&
      ((blackKills & whiteQueenSideAttackPath) == 0);

  // Black Castling Checks
  // King-side (O-O)
  static constexpr std::uint64_t blackKingSidePiecePath =
      (1ULL << BoardSquare::F8) | (1ULL << BoardSquare::G8);
  static constexpr std::uint64_t blackKingSideAttackPath =
      (1ULL << BoardSquare::E8) | (1ULL << BoardSquare::F8) |
      (1ULL << BoardSquare::G8);

  castlingAttackMask.blackKingSide =
      ((flat & blackKingSidePiecePath) == 0) &&
      ((whiteKills & blackKingSideAttackPath) == 0);

  // Queen-side (O-O-O)
  static constexpr std::uint64_t blackQueenSidePiecePath =
      (1ULL << BoardSquare::B8) | (1ULL << BoardSquare::C8) |
      (1ULL << BoardSquare::D8);
  static constexpr std::uint64_t blackQueenSideAttackPath =
      (1ULL << BoardSquare::E8) | (1ULL << BoardSquare::D8) |
      (1ULL << BoardSquare::C8);

  castlingAttackMask.blackQueenSide =
      ((flat & blackQueenSidePiecePath) == 0) &&
      ((whiteKills & blackQueenSideAttackPath) == 0);

  return castlingAttackMask;
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
