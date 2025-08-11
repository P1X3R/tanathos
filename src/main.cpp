#include "board.h"
#include "debuggingTools.h"
#include "legalMoves.h"
#include "move.h"
#include "perft.h"
#include "searching.h"
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

auto tokenize(std::string &input) -> std::vector<std::string> {
  std::vector<std::string> tokens;
  std::stringstream stream(input);
  std::string token;

  while (stream >> token) {
    tokens.push_back(token);
  }

  return tokens;
}

class UCI {
private:
  ChessBoard board;
  Searching searcher;

  void setPosition(std::vector<std::string> &tokens) {
    if (tokens.size() < 2) {
      return;
    }
    std::size_t index = 1;
    if (tokens[1] == "startpos") {
      board = ChessBoard(
          "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
      index = 2;
    } else if (tokens[1] == "fen") {
      std::string fen;
      for (index = 2; index < tokens.size() && tokens[index] != "moves";
           index++) {
        fen += tokens[index] + " ";
      }

      // Trim trailing space
      if (!fen.empty()) {
        fen.pop_back();
      }

      board = ChessBoard(fen);
    }

    if (index < tokens.size() && tokens[index] == "moves") {
      for (std::size_t j = index + 1; j < tokens.size(); j++) {
        makeMove(board, fromAlgebraic(tokens[j], board));
      }
    }

    searcher.board = board;
  }

  void go(const std::vector<std::string> &tokens) {
    std::uint64_t timeLimitMs = 0;
    std::uint8_t depth = 0;
    bool isPerft = false;

    for (std::size_t i = 1; i < tokens.size(); ++i) {
      if (tokens[i] == "perft" && i + 1 < tokens.size()) {
        try {
          depth = std::stoi(tokens[i + 1]);
          isPerft = true;
          break;
        } catch (const std::exception &e) {
          std::cout << "info string Invalid depth\n";
          return;
        }
      }
      if (tokens[i] == "movetime" && i + 1 < tokens.size()) {
        try {
          timeLimitMs = std::stoull(tokens[i + 1]);
        } catch (const std::exception &e) {
          std::cout << "info string Invalid movetime\n";
          return;
        }
      } else if (tokens[i] == "depth" && i + 1 < tokens.size()) {
        try {
          depth = std::stoi(tokens[i + 1]);
          // Override movetime if depth is specified
          timeLimitMs = 0;
        } catch (const std::exception &e) {
          std::cout << "info string Invalid depth\n";
          return;
        }
      }
    }

    if (depth > 0) {
      if (isPerft) {
        const std::uint64_t nodes = perft(depth, board, true);

        std::cout << "Nodes searched: " << nodes << '\n';
      } else {
        MoveCTX bestMove;
        searcher.search(depth, -INF, INF, bestMove);
        std::cout << "bestmove " << moveToUCI(bestMove) << "\n";
      }
    } else if (timeLimitMs > 0) {
      // If a time limit is specified, use iterative deepening
      const MoveCTX bestMove = searcher.iterativeDeepening(timeLimitMs);
      std::cout << "bestmove " << moveToUCI(bestMove) << "\n";
    } else {
      // Default behavior if no specific time or depth is given
      // You might want to implement a default search or handle this case
      std::cout << "info string No search parameters provided\n";
    }
  }

public:
  void loop() {
    std::string input;

    while (std::getline(std::cin, input)) {
      std::vector<std::string> tokens = tokenize(input);

      if (tokens.empty()) {
        continue;
      }

      if (tokens[0] == "uci") {
        std::cout << "id name Tanathos\n";
        std::cout << "id author P1x3r\n";
        std::cout << "uciok\n";
      } else if (tokens[0] == "isready") {
        std::cout << "readyok\n";
      } else if (tokens[0] == "position") {
        setPosition(tokens);
      } else if (tokens[0] == "go") {
        go(tokens);
      } else if (tokens[0] == "quit") {
        break;
      }
    }
  }
};

auto main() -> int {
  UCI engine;
  engine.loop();
}
