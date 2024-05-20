#include "ChessGame.h"
#include <iostream>

int main(int argc, char* argv[])
{
	ChessGame::Position random_position
	{
		{
			0x0000A0402008D580,
			0xADA3500008000001,
			0x00A370002000C500,
			0x2100000000000080,
			0x0000004008000000,
			0x0400000000080000,
			0x0800800000000001,
			0x8000000000001000,
		},
		~(0x0000A0402008D580 |
		0xADA3500008000001 |
		0x00A370002000D500 |
		0x2100000000000080 |
		0x0000004008000000 |
		0x0400000000080000 |
		0x0800800000000001 |
		0x8000000000001000)
	};

	int square = ChessGame::e4;

	ChessGame::print_bitboard(ChessGame::bishop_attack(square, ~random_position.empty));
}