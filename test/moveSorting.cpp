#include "board.h"
#include "legalMoves.h"
#include "gtest/gtest.h"

class MoveSortingTest : public ::testing::Test {
protected:
};

TEST_F(MoveSortingTest, SEEPosition1) {
  const ChessBoard board("1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - - 0 1");
  const MoveCTX move = fromAlgebraic("e1e5", board);

  EXPECT_EQ(move.see(board, board.getFlat(true) | board.getFlat(false)), 100);
}

TEST_F(MoveSortingTest, SEEPosition2) {
  const ChessBoard board(
      "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1");
  const MoveCTX move = fromAlgebraic("d3e5", board);

  EXPECT_EQ(move.see(board, board.getFlat(true) | board.getFlat(false)), -220);
}
