#include "ChessGame.h"
#include <iostream>


/*
	DONE:
	- board representaion
	- position definition
	- bitboard console representation
	- pawn pattern / attacks
	- knight pattern / attacks

	TODO:
	- sliding pieces pattern / attacks (rook, bishop, queen)
	- king pattern / attacks
	- checks / checkmate
	- board console representation
	- gui
*/


const ChessGame::Position ChessGame::starting_position
{
	{
		0x000000000000FFFF, // white pieces
		0xFFFF000000000000, // black pieces
		0x00FF00000000FF00, // pawns
		0x8100000000000081, // rooks
		0x4200000000000042, // knights
		0x2400000000000024, // bishops
		0x0800000000000008, // queens
		0x1000000000000010, // kings
	},
	0x0000FFFFFFFF0000,	// empty squares
};

// De Bruijn sequence to 64-index mapping
const int ChessGame::index64[64]
{
	0, 47,  1, 56, 48, 27,  2, 60,
	57, 49, 41, 37, 28, 16,  3, 61,
	54, 58, 35, 52, 50, 42, 21, 44,
	38, 32, 29, 23, 17, 11,  4, 62,
	46, 55, 26, 59, 40, 36, 15, 53,
	34, 51, 20, 43, 31, 22, 10, 45,
	25, 39, 14, 33, 19, 30,  9, 24,
	13, 18,  8, 12,  7,  6,  5, 63
};

const U64 ChessGame::rank_mask_west_offsets[8]
{
	0xFEFEFEFEFEFEFEFE,
	0xFCFCFCFCFCFCFCFC,
	0xF8F8F8F8F8F8F8F8,
	0xF0F0F0F0F0F0F0F0,
	0xE0E0E0E0E0E0E0E0,
	0xC0C0C0C0C0C0C0C0,
	0x8080808080808080,
	0x0000000000000000,
};

const U64 ChessGame::file_mask_north_offsets[8]
{
	~0ULL << 8,
	~0ULL << 16,
	~0ULL << 24,
	~0ULL << 32,
	~0ULL << 40,
	~0ULL << 48,
	~0ULL << 56,
	0ULL,
};

const int ChessGame::rook_direction[4] { 8, 1, -8, -1 };

const int ChessGame::bishop_direction[4]{ 9, -7, -9, 7 };

U64 ChessGame::mask_pawn_attacks(Position position, bool is_black)
{
	U64 pawns = position.piece_bitboards[nPawn] & position.piece_bitboards[is_black];

	U64 attacks = is_black ? bitboard_union((pawns << 7) & ~h_file, (pawns << 9) & ~a_file) : bitboard_union((pawns >> 7) & ~a_file, (pawns >> 9) & ~h_file);

	return attacks;
}

U64 ChessGame::mask_single_pushable_pawns(Position position, bool is_black)
{
	U64 pawns = position.piece_bitboards[ChessGame::nPawn] & position.piece_bitboards[is_black];

	U64 pushable = _rotr64(position.empty, 8 - (is_black << 4)) & pawns;

	return pushable;
}

U64 ChessGame::mask_double_pushable_pawns(Position position, bool is_black)
{
	U64 pushable_rank[2]{ forth_rank, fifth_rank };

	U64 pawns = position.piece_bitboards[ChessGame::nPawn] & position.piece_bitboards[is_black];

	U64 pushable = _rotr64(pushable_rank[is_black], 16 - (is_black << 5)) & starting_position.piece_bitboards[nPawn] & mask_single_pushable_pawns(position, is_black);

	return pushable;
}

U64 ChessGame::mask_single_pawn_push(Position position, bool is_black)
{
	U64 pawns = position.piece_bitboards[ChessGame::nPawn] & position.piece_bitboards[is_black];

	U64 pushed = _rotl64(pawns, 8 - (is_black << 4)) & position.empty;
	return pushed;
}

U64 ChessGame::mask_double_pawn_push(Position position, bool is_black)
{
	U64 pawns = position.piece_bitboards[ChessGame::nPawn] & position.piece_bitboards[is_black];

	U64 pushed = _rotl64(pawns, 16 - (is_black << 5)) & position.empty;
	return pushed;
}

U64 ChessGame::mask_knight_moves(Position position, bool is_black)
{
	U64 knights = position.piece_bitboards[nKnight] & position.piece_bitboards[is_black];

	U64 moves = (((knights << 6 | knights << 15 | knights >> 10 | knights >> 17) & ~gh_file) |
		((knights << 10 | knights << 17 | knights >> 6 | knights >> 15) & ~ab_file)) &
		position.empty;

	return moves;
}

U64 ChessGame::mask_knight_attacks(Position position, bool is_black)
{
	U64 knights = position.piece_bitboards[nKnight] & position.piece_bitboards[is_black];

	U64 moves = (((knights << 6 | knights << 15 | knights >> 10 | knights >> 17) & ~gh_file) |
		((knights << 10 | knights << 17 | knights >> 6 | knights >> 15) & ~ab_file)) &
		position.piece_bitboards[!is_black];

	return moves;
}

U64 ChessGame::rook_attack(int square, U64 occ)
{
	U64 rook_attack_bb = 0ULL;
	for (int d : rook_direction)
	{
		U64 sq;
		for (sq = 1ULL << square; (sq = ((d > 0) ? (sq << d) : (sq >>= -d)) & rook_mask(square)) && !(sq & occ););
		rook_attack_bb |= sq;
	}
	return rook_attack_bb;
}

U64 ChessGame::bishop_attack(int square, U64 occ)
{
	U64 bishop_attack_bb = 0ULL;
	for (int d : bishop_direction)
	{
		U64 sq;
		for (sq = 1ULL << square; (sq = ((d > 0) ? (sq << d) : (sq >>= -d)) & bishop_mask(square)) && !(sq & occ););
		bishop_attack_bb |= sq;
	}
	return bishop_attack_bb;
}

int ChessGame::pop_count(U64 bitboard) 
{
	int count = 0;
	while (bitboard) {
		count++;
		bitboard &= bitboard - 1;
	}
	return count;
}

U64 ChessGame::diagonal_mask(int square) {
	const U64 maindia = a1_h8;
	int diag = 8 * (square & 7) - (square & 56);
	int nort = -diag & (diag >> 31);
	int sout = diag & (-diag >> 31);
	return (maindia >> sout) << nort;
}

U64 ChessGame::anti_diag_mask(int square) {
	const U64 maindia = a8_h1;
	int diag = 56 - 8 * (square & 7) - (square & 56);
	int nort = -diag & (diag >> 31);
	int sout = diag & (-diag >> 31);
	return (maindia >> sout) << nort;
}

void ChessGame::print_bitboard(U64 bitboard)
{
	for (int rank = max_rank - 1; rank >= 0; rank--)
	{
		std::cout << rank + 1 << "   ";

		for (int file = 0; file < max_file; file++)
		{
			int square = max_rank * rank + file;

			std::cout << " " << (get_bit(bitboard, square) ? 1 : 0) << " ";
		}

		std::cout << std::endl;
	}

	std::cout << "\n     a  b  c  d  e  f  g  h\n";
}

void ChessGame::print_board(Position position)
{
	int index = 0;

	//top border
	std::cout << "\n  " << char(201) << char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(187) << std::endl;



	//std::cout << Board::BOARD_WIDTH << ' ' << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ' << char(186) << std::endl;

	//for (int k = 1; k < Board::BOARD_WIDTH; k++)
	//{
	//	std::cout << "  " << char(204) << char(205) << char(205) << char(205) << char(206)
	//		<< char(205) << char(205) << char(205) << char(206)
	//		<< char(205) << char(205) << char(205) << char(206)
	//		<< char(205) << char(205) << char(205) << char(206)
	//		<< char(205) << char(205) << char(205) << char(206)
	//		<< char(205) << char(205) << char(205) << char(206)
	//		<< char(205) << char(205) << char(205) << char(206)
	//		<< char(205) << char(205) << char(205) << char(185) << std::endl;

	//	std::cout << Board::BOARD_WIDTH - k << ' ' << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//	std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//	std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//	std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//	std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//	std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//	std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ';
	//	std::cout << char(186) << ' ' << squareToChar(board->getSquare(index++)) << ' ' << char(186) << std::endl;

	//}

	std::cout << "  " << char(200) << char(205) << char(205) << char(205) << char(202)
		<< char(205) << char(205) << char(205) << char(202)
		<< char(205) << char(205) << char(205) << char(202)
		<< char(205) << char(205) << char(205) << char(202)
		<< char(205) << char(205) << char(205) << char(202)
		<< char(205) << char(205) << char(205) << char(202)
		<< char(205) << char(205) << char(205) << char(202)
		<< char(205) << char(205) << char(205) << char(188) << std::endl;

	//std::cout << "    a   b   c   d   e   f   g   h\n";
}

/*
void ChessGame::init_magics(bool is_rook, U64 piece_table[], Magic magics[])
{
	int size = 0;
	U64 b = 0;
	U64 occ[4096];
	U64 ref[4096];
	for (int sq = a1; sq < h8; sq++)
	{
		U64 edges_ex = (((fifth_rank | eighth_rank) & ~rank_mask(sq)) | ((a_file | h_file) & ~file_mask(sq)));

		Magic& m = magics[sq];
		m.mask = (is_rook ? rook_mask_ex(sq) : bishop_mask_ex(sq)) & ~edges_ex;
		m.shift = 64 - pop_count(m.mask);
		m.attacks = sq == a1 ? piece_table : magics[sq - 1].attacks + size;

		b = size = 0;

		do
		{
			occ[size] = b;
			ref[size] = is_rook ? rook_attack(sq, b) : bishop_attack(sq, b);
			size++;
			b = (b - m.mask) & m.mask;
		} while (b);
	}
}
*/