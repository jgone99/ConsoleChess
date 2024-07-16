
#include "ChessGame.h"
#include <iostream>
#include <sstream>
#include <string>

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
		~(0x0000A0402008D580 | 0xADA3500008000001),
		ChessGame::white
	};

	std::string fen = "1rb2r1k/4bpRp/p2p4/3N1P1P/n2BP3/P3qP2/1PPpR3/1K5 w - - 0 24";
	std::string stalemate_fen = "2Q2bnr/4p1pq/5pkr/7p/7P/4P3/PPPP1PP1/RNB1KBNR w KQ - 1 10";
	std::string pin_fen = "7k/8/8/3q4/8/3Q4/3K4/8 w - - 0 1";

	//ChessGame::Position position = ChessGame::fen_to_pos(stalemate_fen);

	ChessGame chess_game;

	chess_game.start();

	//ChessGame::print_board(ChessGame::starting_position);
	//ChessGame::print_bitboard(ChessGame::pawn_double_push_mask(ChessGame::c2, position, false));

	return 0;
}