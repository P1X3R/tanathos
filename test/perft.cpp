#include "perft.h"
#include "board.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <string>

class PerftTest : public ::testing::Test {
protected:
};

TEST_F(PerftTest, StartingPosition) {
  static const std::string startingPositionFEN =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  ChessBoard startingBoard(startingPositionFEN);

  const std::uint64_t result = perft(4, startingBoard, false);

  EXPECT_EQ(result, 197281);
}

TEST_F(PerftTest, Kiwipete) {
  static const std::string fen =
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 3 2";
  ChessBoard board(fen);

  const std::uint64_t result = perft(2, board, false);

  EXPECT_EQ(result, 2039);
}
