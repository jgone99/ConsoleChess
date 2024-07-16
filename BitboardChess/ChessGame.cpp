#include "ChessGame.h"
#include <iostream>
#include <sstream>
#include <string>
#include <Windows.h>
#include <stdlib.h>

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
	ChessGame::white,
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

const char ChessGame::black_piece_char[6] { 'p', 'r', 'n', 'b', 'q', 'k' };
const char ChessGame::white_piece_char[6] { 'P', 'R', 'N', 'B', 'Q', 'K' };

const int ChessGame::rook_direction[4] { 8, 1, -8, -1 };

const int ChessGame::bishop_direction[4]{ 9, -7, -9, 7 };

U64 ChessGame::mask_pawn_attacks(Position position, bool is_black)
{
	U64 pawns = position.piece_bitboards[nPawn] & position.piece_bitboards[is_black];

	U64 attacks = is_black ? bitboard_union((pawns >> 7) & ~a_file, (pawns >> 9) & ~h_file) : bitboard_union((pawns << 7) & ~h_file, (pawns << 9) & ~a_file);

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

ChessGame::ChessGame(Position position)
{
	current_position = position;
	position_history_3fold[pos_stringid(position)] = 1;
}

ChessGame::ChessGame(std::string fen) : ChessGame(fen_to_pos(fen))
{
}

U64 ChessGame::pawn_moves_mask(int square, Position position, bool is_black)
{
	return (mask_double_pushable_pawns(position, is_black) & (1ULL << square) ? pawn_double_push_mask(square, position, is_black) : pawn_single_push_mask(square, position, is_black)) & position.empty | (pawn_attack_mask(square, position, is_black) & ~position.empty);
}

U64 ChessGame::rook_moves_mask(int square, Position position, bool is_black)
{
	U64 rook_moves = 0ULL;
	for (int d : rook_direction)
	{
		U64 ray, line = 0ULL;
		for (ray = 1ULL << square; (ray = ((d > 0) ? (ray << d) : (ray >> -d))) && !(ray & ~rook_mask_ex(square)) && !((line |= ray) & ~position.empty););
		rook_moves |= line;
	}
	return rook_moves;
}

U64 ChessGame::bishop_moves_mask(int square, Position position, bool is_black)
{
	U64 bishop_moves = 0ULL;
	for (int d : bishop_direction)
	{
		U64 ray, line = 0ULL;
		for (ray = 1ULL << square; (ray = ((d > 0) ? (ray << d) : (ray >> -d))) && !(ray & ~bishop_mask_ex(square)) && !((line |= ray) & ~position.empty););
		bishop_moves |= line;
	}
	return bishop_moves;
}

U64 ChessGame::knight_moves_mask(int square, Position position, bool is_black)
{
	U64 knight = (1ULL << square);
	return ((knight << 6 | knight << 15 | knight >> 10 | knight >> 17) & ~gh_file) |
		((knight << 10 | knight << 17 | knight >> 6 | knight >> 15) & ~ab_file);
}

U64 ChessGame::queen_moves_mask(int square, Position position, bool is_black)
{
	return rook_moves_mask(square, position, is_black) | bishop_moves_mask(square, position, is_black);
}

U64 ChessGame::rook_moves(Position position, bool is_black)
{
	U64 rooks = position.piece_bitboards[nRook] & position.piece_bitboards[is_black];
	U64 rook_moves = 0ULL;
	for (int square = 0; (square = bit_scan_forward(rooks)) != -1; rooks &= rooks - 1)
	{
		rook_moves |= rook_moves_mask(square, position, is_black);
	}
	return rook_moves;
}

U64 ChessGame::bishop_moves(Position position, bool is_black)
{
	U64 bishops = position.piece_bitboards[nBishop] & position.piece_bitboards[is_black];
	U64 bishop_moves = 0ULL;
	for (int square = 0; (square = bit_scan_forward(bishops)) != -1; bishops &= bishops - 1)
	{
		bishop_moves |= bishop_moves_mask(square, position, is_black);
	}
	return bishop_moves;
}

U64 ChessGame::knight_moves(Position position, bool is_black)
{
	U64 knights = position.piece_bitboards[nKnight] & position.piece_bitboards[is_black];

	U64 moves = (((knights << 6 | knights << 15 | knights >> 10 | knights >> 17) & ~gh_file) |
		((knights << 10 | knights << 17 | knights >> 6 | knights >> 15) & ~ab_file)) &
		(position.empty | position.piece_bitboards[!is_black]);

	return moves;
}

U64 ChessGame::queen_moves(Position position, bool is_black)
{
	U64 queens = position.piece_bitboards[nQueen] & position.piece_bitboards[is_black];
	U64 queen_moves = 0ULL;
	for (int square = 0; (square = bit_scan_forward(queens)) != -1; queens &= queens - 1)
	{
		queen_moves |= rook_moves_mask(square, position, is_black) | bishop_moves_mask(square, position, is_black);
	}
	return queen_moves;
}

U64 ChessGame::king_moves(Position position, bool is_black)
{
	U64 king = position.piece_bitboards[nKing] & position.piece_bitboards[is_black];
	Position trans_pos = position;
	trans_pos.piece_bitboards[nKing] &= ~king;
	trans_pos.piece_bitboards[is_black] &= ~king;
	trans_pos.empty |= king;
	return (north_one(king) | north_east_one(king) | east_one(king) | south_east_one(king) | south_one(king) | south_west_one(king) | west_one(king) | north_west_one(king)) & (position.empty | position.piece_bitboards[!is_black]) & ~attacks(trans_pos, !is_black);
}

U64 ChessGame::all_legal_moves(Position position, bool is_black)
{
	U64 color_bb = position.piece_bitboards[is_black];
	U64 moves_bb = 0ULL;

	while (color_bb)
	{
		moves_bb |= moves(bit_scan_forward(color_bb), position, is_black);
		color_bb &= color_bb - 1;
	}

	return moves_bb;
}

U64 ChessGame::moves(int square, Position position, bool is_black, unsigned char flags)
{
	U64 piece_bb = (1ULL << square);
	U64 move_mask = (bool(flags & EMPTY) * position.empty) | (bool(flags & CAPTURE) * position.piece_bitboards[!is_black]) | (bool(flags & DEFEND) * position.piece_bitboards[is_black]);

	if (piece_bb & position.piece_bitboards[nKing] & position.piece_bitboards[is_black]) return king_moves(position, is_black) & move_mask;

	int type;
	U64 moves = 0ULL;
	U64 bitboard = 0ULL;
	
	int king_square = bit_scan_forward(position.piece_bitboards[nKing] & position.piece_bitboards[is_black]);

	//U64 king_check_mask = queen_moves_mask(king_square, position, is_black);

	//U64 rook_check;
	//U64 bishop_check;
	//U64 knight_check;

	//U64 checks = (rook_check = king_check_mask & rook_mask(king_square) & position.piece_bitboards[!is_black] & (position.piece_bitboards[nRook] | position.piece_bitboards[nQueen]))
	//| (bishop_check = king_check_mask & bishop_mask(king_square) & position.piece_bitboards[!is_black] & (position.piece_bitboards[nBishop] | position.piece_bitboards[nQueen]))
	//| (knight_check = knight_moves_mask(king_square, position, is_black) & position.piece_bitboards[!is_black] & position.piece_bitboards[nKnight]);

	//if (checks & (checks - 1)) return 0x0;

	for (type = nPawn; !(bitboard = position.piece_bitboards[type] & piece_bb) && (type < nKing); type++);

	switch (type)
	{
	case nPawn: 
		moves = pawn_moves_mask(square, position, is_black);
		break;
	case nRook:
		moves = rook_moves_mask(square, position, is_black);
		break;
	case nKnight: 
		moves = knight_moves_mask(square, position, is_black);
		break;
	case nBishop:
		moves = bishop_moves_mask(square, position, is_black);
		break;
	case nQueen: 
		moves = queen_moves_mask(square, position, is_black);
		break;
	default:
		break;
	}

	moves &= move_mask;

	if (position.state == CHECK) moves &= position.checking_path_bb;

	//print_bitboard(king_check_mask);
	//print_bitboard(king_check_mask & rook_mask(king_square));
	//print_bitboard(king_check_mask & rook_mask(king_square) & rook_moves_mask(bit_scan_forward(bishop_check), position, !is_black));

	//if (knight_check) moves &= knight_check;
	//if (bishop_check) moves &= king_check_mask & bishop_mask(king_square) & bishop_moves_mask(bit_scan_forward(bishop_check), position, !is_black) | bishop_check;
	//if (rook_check) moves &= king_check_mask & rook_mask(king_square) & rook_moves_mask(bit_scan_forward(rook_check), position, !is_black) | rook_check;

	U64 king_pin_mask = pin_mask(position, is_black);

	if (!(king_pin_mask & piece_bb)) return moves;

	Position trans_position = position;

	trans_position.piece_bitboards[is_black] &= ~piece_bb;
	trans_position.piece_bitboards[type] &= ~piece_bb;
	trans_position.empty |= piece_bb;

	U64 rook_mask_moves = rook_moves_mask(king_square, trans_position, is_black);
	if (rook_mask_moves & (position.piece_bitboards[!is_black] & (trans_position.piece_bitboards[nQueen] | trans_position.piece_bitboards[nRook]))) return moves & rook_mask_ex(square) & rook_mask_ex(king_square);

	U64 bishop_mask_moves = bishop_moves_mask(king_square, trans_position, is_black);
	if (bishop_mask_moves & (position.piece_bitboards[!is_black] & (trans_position.piece_bitboards[nQueen] | trans_position.piece_bitboards[nBishop]))) return moves & bishop_mask_ex(square) & bishop_mask_ex(king_square);

	return moves;
}

U64 ChessGame::rook_attacks(Position position, bool is_black)
{
	return rook_moves(position, is_black) & position.piece_bitboards[!is_black];
}

U64 ChessGame::bishop_attacks(Position position, bool is_black)
{
	return bishop_moves(position, is_black) & position.piece_bitboards[!is_black];
}

U64 ChessGame::knight_attacks(Position position, bool is_black)
{
	return knight_moves(position, is_black) & position.piece_bitboards[!is_black];
}

U64 ChessGame::queen_attacks(Position position, bool is_black)
{
	return queen_moves(position, is_black) & position.piece_bitboards[!is_black];
}

U64 ChessGame::king_attacks(Position position, bool is_black)
{
	return king_moves(position, is_black) & position.piece_bitboards[!is_black];
}

U64 ChessGame::attacks(Position position, bool is_black)
{
	return mask_pawn_attacks(position, is_black)
		| rook_moves(position, is_black)
		| knight_moves(position, is_black)
		| bishop_moves(position, is_black)
		| queen_moves(position, is_black)
		| king_mask(bit_scan_forward(position.piece_bitboards[nKing] & position.piece_bitboards[is_black]));
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

			std::cout << " " << ((bitboard & (1ULL << square)) ? 1 : 0) << " ";
		}
		std::cout << std::endl;
	}
	std::cout << "\n     a  b  c  d  e  f  g  h\n";
}

void ChessGame::print_board(Position position, U64 moves)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	std::cout << "\n  " << char(201) << char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(203)
		<< char(205) << char(205) << char(205) << char(187) << std::endl;

	for (int rank = max_rank - 1; rank >= 0; rank--)
	{
		std::cout << rank + 1 << ' ' << char(186);
		for (int file = 0; file < max_file; file++)
		{
			U64 sq_bitboard = 1ULL << (rank * max_rank + file);
			int piece_type;
			bool is_white = sq_bitboard & position.piece_bitboards[nWhite];
			bool is_empty_sq = sq_bitboard & position.empty;

			std::cout << ' ';

			if (is_empty_sq)
			{
				if (sq_bitboard & moves)
				{
					std::cout << char(254) << ' ' << char(186);
					continue;
				}
				else
				{
					std::cout << "  " << char(186);
					continue;
				}
			}

			for (piece_type = nPawn; !(sq_bitboard & position.piece_bitboards[piece_type]); piece_type++);

			if (sq_bitboard & moves)
			{
				SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
			}

			//std::cout << piece_type << std::endl;
			std::cout << (is_white ? white_piece_char[piece_type - nPawn] : black_piece_char[piece_type - nPawn]) << ' ';

			SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);

			std::cout << char(186);
		}

		rank > 0 ? (std::cout << "\n  " << char(204) << char(205) << char(205) << char(205) << char(206)
			<< char(205) << char(205) << char(205) << char(206)
			<< char(205) << char(205) << char(205) << char(206)
			<< char(205) << char(205) << char(205) << char(206)
			<< char(205) << char(205) << char(205) << char(206)
			<< char(205) << char(205) << char(205) << char(206)
			<< char(205) << char(205) << char(205) << char(206)
			<< char(205) << char(205) << char(205) << char(185) << std::endl) : 
			(std::cout << "\n  " << char(200) << char(205) << char(205) << char(205) << char(202)
			<< char(205) << char(205) << char(205) << char(202)
			<< char(205) << char(205) << char(205) << char(202)
			<< char(205) << char(205) << char(205) << char(202)
			<< char(205) << char(205) << char(205) << char(202)
			<< char(205) << char(205) << char(205) << char(202)
			<< char(205) << char(205) << char(205) << char(202)
			<< char(205) << char(205) << char(205) << char(188) << std::endl);
	}
	std::cout << "    a   b   c   d   e   f   g   h\n";
}

void ChessGame::print_position(Position position)
{
	for (U64 bb : position.piece_bitboards)
	{
		print_bitboard(bb);
	}
	print_bitboard(position.empty);
	std::cout << position.color_to_move << std::endl;
}

void ChessGame::start()
{
	std::string input;
	bool game_over = false;

	while (!game_over)
	{
		system("cls");
		print_board(current_position);
		std::cout << "=========== " << (current_position.color_to_move ? "black" : "white") << " to move" << " ===========\n";
		std::cout << "enter a square (e.g. a1): ";
		getline(std::cin, input);

		//system("cls");

		if (input.compare("h") == 0)
		{
			system("cls");
			std::cout << "help page\n";
			getline(std::cin, input);
		}
		else
		{
			int initial_square = 0x0;
			int final_square;
			U64 square_moves = 0x0;
			bool valid_square = (input.length() == 2) && islower(input[0]) && isdigit(input[1]) && ((initial_square = max_file * (input[1] - '1') + (input[0] - 'a')) >= 0) && (initial_square <= 63);
			
			if (valid_square && ((1ULL << initial_square) & current_position.piece_bitboards[current_position.color_to_move]) && (square_moves = moves(initial_square, current_position, current_position.color_to_move)))
			{
				system("cls");
				
				print_board(current_position, square_moves);

				std::cout << "enter a destination square: ";

				getline(std::cin, input);

				valid_square = input.length() == 2 && islower(input[0]) && isdigit(input[1]) && (final_square = max_file * (input[1] - '1') + (input[0] - 'a')) >= 0 && final_square <= 63;

				if (valid_square && ((1ULL << final_square) & square_moves))
				{
					make_move(initial_square, final_square);
					update_game_status();
					
					std::cout << "size: " << position_history_3fold.size() << std::endl;
					getline(std::cin, input);
					
					game_over = current_position.state == CHECKMATE || current_position.state == REPETITION || current_position.state == STALEMATE;
					//std::cout << current_position.state << std::endl;
					//print_bitboard(all_legal_moves(current_position, current_position.color_to_move));
					//print_bitboard(current_position.checking_path_bb);
				}
				else
				{
					message("invalid destination square");
				}
			}
			else
			{
				//std::cout << valid_square << std::endl;

				//print_bitboard((1ULL << initial_square) & current_position.piece_bitboards[current_position.color_to_move]);
				//print_bitboard(square_moves);

				//getline(std::cin, input);

				message("invalid initial square");
			}
		}
	}

	system("cls");
	print_board(current_position);

	switch (current_position.state)
	{
	case CHECKMATE:
		std::cout << "=========== " << (!current_position.color_to_move ? "black" : "white") << " won by checkmate" << " ===========\n";
		break;
	case STALEMATE:
		std::cout << "=========== draw by stalemate ===========\n";
		break;
	case REPETITION:
		std::cout << "=========== draw by 3-fold repetition ===========\n";
		break;
	default:
		break;
	}
}

void ChessGame::message(std::string message)
{
	std::string input;
	system("cls");
	std::cout << message << std::endl;
	getline(std::cin, input);
}

void ChessGame::make_move(int initial_square, int final_square)
{
	U64 initial_square_bb = (1ULL << initial_square);
	U64 final_square_bb = (1ULL << final_square);

	enumColor color_to_move = current_position.color_to_move;
	int source_type;
	int dest_type;

	if (final_square_bb & ~current_position.empty)
	{
		for (dest_type = nPawn; (dest_type <= nKing) && !(current_position.piece_bitboards[dest_type] & final_square_bb); dest_type++);
		current_position.piece_bitboards[!color_to_move] &= ~final_square_bb;
		current_position.piece_bitboards[dest_type] &= ~final_square_bb;
		position_history_3fold.clear();
	}

	for (source_type = nPawn; (source_type <= nKing) && !(current_position.piece_bitboards[source_type] & initial_square_bb); source_type++);
	
	current_position.piece_bitboards[color_to_move] = current_position.piece_bitboards[color_to_move] & ~initial_square_bb | final_square_bb;
	current_position.piece_bitboards[source_type] = current_position.piece_bitboards[source_type] & ~initial_square_bb | final_square_bb;

	current_position.empty = ~(current_position.piece_bitboards[nWhite] | current_position.piece_bitboards[nBlack]);
	current_position.color_to_move = enumColor(!color_to_move);

	//print_position(current_position);
}

void ChessGame::update_game_status()
{
	std::cout << pos_stringid(current_position) << std::endl;
	if ((position_history_3fold[pos_stringid(current_position)] += 1) > 2)
	{
		current_position.state = REPETITION;
		return;
	}

	bool is_black = current_position.color_to_move;
	int king_square = bit_scan_forward(current_position.piece_bitboards[nKing] & current_position.piece_bitboards[is_black]);
	U64 king_check_mask = queen_moves_mask(king_square, current_position, is_black);

	U64 rook_check;
	U64 bishop_check;
	U64 knight_check;

	U64 checks = (rook_check = king_check_mask & rook_mask(king_square) & current_position.piece_bitboards[!is_black] & (current_position.piece_bitboards[nRook] | current_position.piece_bitboards[nQueen]))
		| (bishop_check = king_check_mask & bishop_mask(king_square) & current_position.piece_bitboards[!is_black] & (current_position.piece_bitboards[nBishop] | current_position.piece_bitboards[nQueen]))
		| (knight_check = knight_moves_mask(king_square, current_position, is_black) & current_position.piece_bitboards[!is_black] & current_position.piece_bitboards[nKnight]);

	bool king_can_move = king_moves(current_position, is_black);

	if (checks)
	{
		bool double_check = checks & (checks - 1);
		if (king_can_move)
		{
			current_position.state = CHECK;
		}
		else
		{
			if (double_check)
			{
				current_position.state = CHECKMATE;
			}
			else
			{
				U64 pin_path = 0ULL;
				U64 moves_bb = 0ULL;

				if (knight_check) pin_path = knight_check;
				if (bishop_check) pin_path = king_check_mask & bishop_mask(king_square) & bishop_moves_mask(bit_scan_forward(bishop_check), current_position, !is_black) | bishop_check;
				if (rook_check) pin_path = king_check_mask & rook_mask(king_square) & rook_moves_mask(bit_scan_forward(rook_check), current_position, !is_black) | rook_check;

				for (int type = nPawn; type <= nQueen; type++)
				{
					U64 type_bb = current_position.piece_bitboards[type] & current_position.piece_bitboards[is_black];
					while (type_bb)
					{
						int square = bit_scan_forward(type_bb);
						moves_bb |= moves(square, current_position, is_black) & pin_path;
						type_bb &= type_bb - 1;
					}
				}

				current_position.state = moves_bb ? CHECK : CHECKMATE;
				current_position.checking_path_bb = pin_path;
			}
		}
	}
	else if (!king_can_move)
	{
		U64 bb = 0ULL;

		for (int type = nPawn; type <= nQueen; type++)
		{
			U64 type_bb = current_position.piece_bitboards[type] & current_position.piece_bitboards[is_black];
			while (type_bb)
			{
				int square = bit_scan_forward(type_bb);

				bb |= moves(square, current_position, is_black);

				type_bb &= type_bb - 1;
			}
		}

		current_position.state = bb ? NORMAL : STALEMATE;
	}
	else
	{
		current_position.state = NORMAL;
	}
}

ChessGame::Position ChessGame::fen_to_pos(std::string fen)
{
	ChessGame::Position position = {};

	size_t meta_index;

	std::stringstream ss(fen.substr(0, meta_index = fen.find_first_of(' ')));
	std::stringstream ss_meta(fen.substr(meta_index));
	std::string token;

	int square = 63;

	while (std::getline(ss, token, '/'))
	{
		for (int k = static_cast<int>(token.length() - 1); k >= 0; k--)
		{
			char c = token[k];
			if (isdigit(c))
			{
				square -= (c - '0');
				continue;
			}

			switch (c)
			{
			case 'P':
				set_bit(position.piece_bitboards[ChessGame::nPawn], square);
				set_bit(position.piece_bitboards[ChessGame::nWhite], square);
				break;
			case 'R':
				set_bit(position.piece_bitboards[ChessGame::nRook], square);
				set_bit(position.piece_bitboards[ChessGame::nWhite], square);
				break;
			case 'N':
				set_bit(position.piece_bitboards[ChessGame::nKnight], square);
				set_bit(position.piece_bitboards[ChessGame::nWhite], square);
				break;
			case 'B':
				set_bit(position.piece_bitboards[ChessGame::nBishop], square);
				set_bit(position.piece_bitboards[ChessGame::nWhite], square);
				break;
			case 'Q':
				set_bit(position.piece_bitboards[ChessGame::nQueen], square);
				set_bit(position.piece_bitboards[ChessGame::nWhite], square);
				break;
			case 'K':
				set_bit(position.piece_bitboards[ChessGame::nKing], square);
				set_bit(position.piece_bitboards[ChessGame::nWhite], square);
				break;
			case 'p':
				set_bit(position.piece_bitboards[ChessGame::nPawn], square);
				set_bit(position.piece_bitboards[ChessGame::nBlack], square);
				break;
			case 'r':
				set_bit(position.piece_bitboards[ChessGame::nRook], square);
				set_bit(position.piece_bitboards[ChessGame::nBlack], square);
				break;
			case 'n':
				set_bit(position.piece_bitboards[ChessGame::nKnight], square);
				set_bit(position.piece_bitboards[ChessGame::nBlack], square);
				break;
			case 'b':
				set_bit(position.piece_bitboards[ChessGame::nBishop], square);
				set_bit(position.piece_bitboards[ChessGame::nBlack], square);
				break;
			case 'q':
				set_bit(position.piece_bitboards[ChessGame::nQueen], square);
				set_bit(position.piece_bitboards[ChessGame::nBlack], square);
				break;
			case 'k':
				set_bit(position.piece_bitboards[ChessGame::nKing], square);
				set_bit(position.piece_bitboards[ChessGame::nBlack], square);
				break;
			default:
				break;
			}
			square--;
		}
	}
	position.empty = ~(position.piece_bitboards[ChessGame::nWhite] | position.piece_bitboards[ChessGame::nBlack]);
	
	std::getline(ss_meta, token, ' ');

	position.color_to_move = enumColor(token[0] == 'b');

	return position;
}

std::string ChessGame::pos_stringid(Position position)
{
	std::string stringid = "";
	for (int type = nPawn; type <= nKing; type++)
	{
		stringid += std::to_string(position.piece_bitboards[type]);
	}
	stringid += std::to_string(position.color_to_move;

	return stringid;
}





/*
	Magic Bitboard implementation (WIP)
*/

/*
void ChessGame::init_magics(bool is_rook, U64 piece_table[], Magic magics[])
{
	int size = 0;
	U64 b = 0;
	U64 occ[4096];
	U64 ref[4096];
	for (int sq = a1; sq <= h8; sq++)
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