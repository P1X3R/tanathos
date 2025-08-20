#include "bitboard.h"
#include "sysifus.h"
#include <array>
#include <cstdint>

std::array<std::array<std::int32_t, BOARD_AREA>, Piece::KING + 1> MIDGAME_PSQT =
    {
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
};

std::array<std::array<std::int32_t, BOARD_AREA>, Piece::KING + 1> ENDGAME_PSQT =
    {
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
        std::array<std::int32_t, BOARD_AREA>{},
};
