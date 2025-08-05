#include "board.h"
#include "legalMoves.h"
#include "move.h"
#include "perft.h"
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
  }

  void go(const std::vector<std::string> &tokens) {
    std::uint8_t depth = 0;
    for (std::size_t i = 1; i < tokens.size(); ++i) {
      if (tokens[i] == "perft" && i + 1 < tokens.size()) {
        try {
          depth = std::stoi(tokens[i + 1]);
          break;
        } catch (const std::exception &e) {
          std::cout << "info string Invalid depth\n";
          return;
        }
      }
    }

    if (depth > 0) {
      // Assuming ChessBoard has a perft function that returns uint64_t
      const std::uint64_t nodes = perft(depth, board, true);
      std::cout << "\nNodes searched: " << nodes << "\n";
    } else {
      std::cout << "info string Invalid or missing perft depth\n";
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
