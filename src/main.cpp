#include "board.h"
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
      } else if (tokens[0] == "go") {
      } else if (tokens[0] == "quit") {
        break;
      }
    }
  }
};

auto main() -> int {}
