#pragma once
#include <string>
#include <map>

typedef unsigned long long U64;

class ChessGame
{
public:
	const enum enumColor
	{
		white,
		black
	};

	const enum enumGameState
	{
		NORMAL,
		CHECK,
		CHECKMATE,
		REPETITION,
		STALEMATE
	};

	const enum enumPiece
	{
		nWhite,		// all white pieces
		nBlack,		// all black pieces
		nPawn,		// all pawns
		nRook,		// all rooks
		nKnight,	// all knights
		nBishop,	// all bishops
		nQueen,		// all queens
		nKing		// all kings
	};

	const enum enumSquare 
	{
		a1, b1, c1, d1, e1, f1, g1, h1,
		a2, b2, c2, d2, e2, f2, g2, h2,
		a3, b3, c3, d3, e3, f3, g3, h3,
		a4, b4, c4, d4, e4, f4, g4, h4,
		a5, b5, c5, d5, e5, f5, g5, h5,
		a6, b6, c6, d6, e6, f6, g6, h6,
		a7, b7, c7, d7, e7, f7, g7, h7,
		a8, b8, c8, d8, e8, f8, g8, h8
	};

	const enum enumIncludeMove
	{
		EMPTY	= 0x01,
		CAPTURE = 0x02,
		DEFEND	= 0x04
	};

	struct Position
	{
		U64 piece_bitboards[8];
		U64 empty;
		enumColor color_to_move = white;
		enumGameState state = NORMAL;
		U64 checking_path_bb = 0ULL;
	};

	const static char white_piece_char[6];
	const static char black_piece_char[6];

	// De Bruijn sequence to 64-index mapping
	const static int index64[64];

	// De Bruijn sequence over alphabet {0, 1}. B(2, 6)
	const static U64 debruijn64 = 0x03f79d71b4cb0a89;

	const static U64 rank_mask_west_offsets[8];
	const static U64 file_mask_north_offsets[8];

	const static int max_rank = 8;
	const static int max_file = 8;

	const static U64 a_file = 0x0101010101010101;
	const static U64 h_file = 0x8080808080808080;
	const static U64 first_rank = 0x00000000000000FF;
	const static U64 eighth_rank = 0xFF00000000000000;
	const static U64 a1_h8 = 0x8040201008040201;
	const static U64 a8_h1 = 0x0102040810204080;
	const static U64 ab_file = a_file | (a_file << 1);
	const static U64 gh_file = h_file | (h_file >> 1);
	const static U64 forth_rank = first_rank << 24;
	const static U64 fifth_rank = forth_rank << 8;

	const static int rook_direction[4];
	const static int bishop_direction[4];

	ChessGame(Position position = starting_position);
	ChessGame(std::string fen);

	std::map<std::string, int> position_history_3fold;

	Position current_position{};

	void start();
	void message(std::string);
	void make_move(int initial_square, int final_square);
	void update_game_status();
	static Position fen_to_pos(std::string fen);
	static std::string pos_stringid(Position position);
	inline static U64 get_bit(U64 bitboard, int square) { return bitboard &= (1ULL << square); }
	inline static void set_bit(U64& bitboard, int square) { bitboard |= (1ULL << square); }
	inline static U64 bitboard_union(U64 bitboard1, U64 bitboard2) { return bitboard1 | bitboard2; }
	inline static int bit_scan_forward(U64 bitboard) { return bitboard ? (index64[((bitboard ^ (bitboard - 1)) * debruijn64) >> 58]) : -1; }
	inline static int bit_scan_reverse(U64 bitboard) {
		bitboard |= bitboard >> 1;
		bitboard |= bitboard >> 2;
		bitboard |= bitboard >> 4;
		bitboard |= bitboard >> 8;
		bitboard |= bitboard >> 16;
		bitboard |= bitboard >> 32;
		return index64[(bitboard * debruijn64) >> 58];
	}
	inline static U64 north_one(U64 bitboard) { return (bitboard << 8); }
	inline static U64 north_east_one(U64 bitboard) { return (bitboard << 9) & ~a_file; }
	inline static U64 east_one(U64 bitboard) { return (bitboard << 1) & ~a_file; }
	inline static U64 south_east_one(U64 bitboard) { return (bitboard >> 7) & ~a_file; }
	inline static U64 south_one(U64 bitboard) { return (bitboard >> 8); }
	inline static U64 south_west_one(U64 bitboard) { return (bitboard >> 9) & ~h_file; }
	inline static U64 west_one(U64 bitboard) { return (bitboard >> 1) & ~h_file; }
	inline static U64 north_west_one(U64 bitboard) { return (bitboard << 7) & ~h_file; }
	
	static U64 pawn_moves_mask(int square, Position position, bool is_black);
	static U64 rook_moves_mask(int square, Position position, bool is_black);
	static U64 bishop_moves_mask(int square, Position position, bool is_black);
	static U64 knight_moves_mask(int square, Position position, bool is_black);
	static U64 queen_moves_mask(int square, Position position, bool is_black);
	static U64 rook_moves(Position position, bool is_black);
	static U64 bishop_moves(Position position, bool is_black);
	static U64 knight_moves(Position position, bool is_black);
	static U64 queen_moves(Position position, bool is_black);
	static U64 king_moves(Position position, bool is_black);
	static U64 all_legal_moves(Position position, bool is_black);
	static U64 moves(int square, Position position, bool is_black, unsigned char flags = EMPTY|CAPTURE);
	
	static U64 rook_attacks(Position position, bool is_black);
	static U64 bishop_attacks(Position position, bool is_black);
	static U64 knight_attacks(Position position, bool is_black);
	static U64 queen_attacks(Position position, bool is_black);
	static U64 king_attacks(Position position, bool is_black);

	static U64 attacks(Position position, bool is_black);

	static int pop_count(U64 bitboard);

	inline static U64 rank_mask(int square) { return first_rank << (square & 56); }
	inline static U64 file_mask(int square) { return a_file << (square & 7); }

	inline static U64 pawn_single_push_mask(int square, Position position, bool is_black) { return is_black ? ((1ULL << square) >> 8) : ((1ULL << square) << 8); }
	inline static U64 pawn_double_push_mask(int square, Position position, bool is_black) { return pawn_single_push_mask(square, position, is_black) | (is_black ? ((1ULL << square) >> 16) : ((1ULL << square) << 16)); }
	inline static U64 pawn_attack_mask(int square, Position position, bool is_black) { return is_black ? (((1ULL << square) >> 7) & ~a_file | ((1ULL << square) >> 9) & ~h_file) : (((1ULL << square) << 7) & ~h_file | ((1ULL << square) << 9) & ~a_file); }
	inline static U64 rook_mask(int square) { return rank_mask(square) | file_mask(square); }
	inline static U64 bishop_mask(int square) { return diagonal_mask(square) | anti_diag_mask(square); }
	inline static U64 rook_mask_ex(int square) { return rank_mask(square) ^ file_mask(square); }
	inline static U64 bishop_mask_ex(int square) { return diagonal_mask(square) ^ anti_diag_mask(square); }
	inline static U64 queen_mask(int square) { return rook_mask(square) | bishop_mask(square); }
	inline static U64 queen_mask_ex(int square) { return rook_mask(square) ^ bishop_mask(square); }
	inline static U64 king_mask(int square) { return north_one(1ULL << square) 
		| north_east_one(1ULL << square)
		| east_one(1ULL << square) 
		| south_east_one(1ULL << square) 
		| south_one(1ULL << square) 
		| south_west_one(1ULL << square) 
		| west_one(1ULL << square) 
		| north_west_one(1ULL << square); 
	};

	inline static U64 pin_mask(Position position, bool is_black) { return queen_mask_ex(bit_scan_forward(position.piece_bitboards[nKing] & position.piece_bitboards[is_black])); }

	void static print_bitboard(U64 bitboard);
	U64 static mask_pawn_attacks(Position, bool is_black);
	U64 static mask_single_pushable_pawns(Position position, bool is_black);
	U64 static mask_double_pushable_pawns(Position position, bool is_black);
	U64 static mask_single_pawn_push(Position position, bool is_black);
	U64 static mask_double_pawn_push(Position position, bool is_black);
	U64 static mask_rook_moves(Position position, bool is_black);
	U64 static mask_rook_attacks(Position position, bool is_black);
	U64 static mask_bishop_moves(Position position, bool is_black);
	U64 static mask_bishop_attacks(Position position, bool is_black);
	U64 static diagonal_mask(int square);
	U64 static anti_diag_mask(int square);


	void static print_board(Position position, U64 moves = 0x0);
	void static print_position(Position position);

	const static Position starting_position;


	// Magic Bitnboard implementation (WIP)
	
	//static void init_magics(bool is_rook, U64 piece_table[], Magic magics[]);
	//struct Magic
	//{
	//	U64* attacks;
	//	U64 mask;
	//	U64 magic;
	//	int shift;

	//	unsigned index(U64 occ)
	//	{
	//		return unsigned(((occ & mask) * magic) >> shift);
	//	}
	//};

	//U64 rook_table[0x19000];
	//U64 bishop_table[0x1480];

	//Magic rook_magics[64];
	//Magic bishop_magics[64];
};

