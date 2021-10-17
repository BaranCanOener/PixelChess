#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include "Pieces.h"
#include "board.h"
#include "engine.h"
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>

enum class GameState { IDLE, MOVING_PIECE };
enum class GameMode {PLAYERVPLAYER, PLAYERVBLACK, PLAYERVWHITE, WHITEVBLACK};

class PixelChess : public olc::PixelGameEngine
{
private:
	const int chessPieceWidth = 16;
	const int chessPieceHeight = 16;
	const int chessPieceSize = 3;
	const std::string strKnight =
		"................"
		"................"
		".........X.X...."
		"......XXXXXXX..."
		".....XXX.XXXXX.."
		"....XXXXXXXXXX.."
		"...XXXXXXXXXXX.."
		"...XXXXXXXXXXX.."
		"...XXX.XXXXXXX.."
		".......XXXXXXX.."
		".......XXXXXX..."
		"......XXXXXXX..."
		".....XXXXXXX...."
		".....XXXXXXX...."
		"...XXXXXXXXXX..."
		"..XXXXXXXXXXXX..";

	const std::string strRook =
		"................"
		"................"
		"................"
		"....XX.XX.XX...."
		"....XXXXXXXX...."
		"....XXXXXXXX...."
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		"...XXXXXXXXXX..."
		"..XXXXXXXXXXXX..";

	const std::string strBishop =
		"................"
		"................"
		"........X......."
		".......XX.X....."
		"......XXX.X....."
		"......XXX.X....."
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		"......XXXX......"
		"......XXXX......"
		"......XXXX......"
		"....XXXXXXXX...."
		"...XXXXXXXXXX..."
		"..XXXXXXXXXXXX..";

	const std::string strPawn =
		"................"
		"................"
		"................"
		"................"
		".......XX......."
		"......XXXX......"
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		"......XXXX......"
		"......XXXX......"
		"......XXXX......"
		"......XXXX......"
		"....XXXXXXXX...."
		"...XXXXXXXXXX..."
		"..XXXXXXXXXXXX..";

	const std::string strQueen =
		"................"
		".......XX......."
		"......XXXX......"
		"...XXXXXXXXXX..."
		"....XXXXXXXX...."
		".....XXXXXX....."
		".....XXXXXX....."
		".....XXXXXX....."
		"......XXXX......"
		"......XXXX......"
		"......XXXX......"
		"......XXXX......"
		"......XXXX......"
		"....XXXXXXXX...."
		"...XXXXXXXXXX..."
		"..XXXXXXXXXXXX..";

	const std::string strKing =
		"................"
		".......XX......."
		"......XXXX......"
		"......XXXX......"
		".......XX......."
		"......XXXX......"
		"....XXXXXXXX...."
		"....XXXXXXXX...."
		".....XXXXXX....."
		"......XXXX......"
		"......XXXX......"
		"......XXXX......"
		"......XXXX......"
		"....XXXXXXXX...."
		"...XXXXXXXXXX..."
		"..XXXXXXXXXXXX..";

	PieceType boardPieceTypes[8][8];
	Colour boardPieceColour[8][8];
	ChessBoard board;
	Engine engine;
	Colour plTurn = Colour::White;
	std::tuple<char, char> selectedPiece = std::tuple<char, char>(-1, -1);
	std::vector<std::tuple<char, char>> possibleMoves;
	GameState state = GameState::IDLE;
	GameMode mode = GameMode::PLAYERVBLACK;
	std::tuple<char, char> piece_movingFrom, piece_movingTo;
	const float MOVE_SEC = 0.25f;
	float move_t = 0.0f;
	MoveData lastMove;
	std::vector<std::string> moveSequence;
	std::thread ai_thread;
	bool ai_thread_working = false;
	bool ai_thread_spawned = false;
	std::chrono::steady_clock::time_point begin, end;
	std::mutex m;
	unsigned int whiteWins = 0;
	unsigned int blackWins = 0;
	unsigned int draws = 0;
	bool reflect = false;
	bool whiteCheckmate = false;
	bool blackCheckmate = false;
	bool stalemate = false;
public:

	void engine_callback(Engine* engine) {

	}

	void drawUpdate() {
		int posX = 8 * chessPieceHeight * chessPieceSize + 5;
		int posY = 5;
		std::chrono::steady_clock::time_point update_clock;
		if (ai_thread_working)
			update_clock = std::chrono::steady_clock::now();
		else
			update_clock = end;
		if (engine.getNodes() > 0)
			DrawString(posX, posY, "Nodes: " + std::to_string((engine.getNodes() + engine.getQuiescenceNodes()) / 1000) + "kN, " +
				std::to_string((engine.getNodes() + engine.getQuiescenceNodes()) / std::max(std::chrono::duration_cast<std::chrono::milliseconds>(update_clock - begin).count(), (long long)1)) + " kN/s");
		else DrawString(posX, posY, "Nodes: 0");
		posY += 10;
		DrawString(posX, posY, "Hash Hits: " + std::to_string(board.transpos_table.getHashHits()));
		posY += 10;
		DrawString(posX, posY, "Depth: " + std::to_string(engine.depthLimit) + "+" + std::to_string(engine.quiescenceLimit));
		posY += 10;
		DrawString(posX, posY, "Valuation: " + std::to_string(engine.optimalValue));
		posY += 10;
		FillRect(posX, posY, 200 * std::chrono::duration_cast<std::chrono::milliseconds>(update_clock - begin).count() / 1000.0f / engine.timeLimit, 20);
		posY += 25;
		DrawString(posX, posY, "Commands:");
		posY += 10;
		DrawString(posX, posY, "  New Game (no engine): 1 ");
		posY += 10;
		DrawString(posX, posY, "  New Game vs Black: 2 ");
		posY += 10;
		DrawString(posX, posY, "  New Game vs White: 3 ");
		posY += 20;
		if (blackCheckmate)
			DrawString(posX, posY, "Black is Checkmate.", olc::RED);
		else if (whiteCheckmate)
			DrawString(posX, posY, "White is Checkmate.", olc::RED);
		else if (stalemate)
			DrawString(posX, posY, "Draw.", olc::RED);
		posY += 20;
		DrawString(posX, posY, "White wins: " + std::to_string(whiteWins));
		posY += 10;
		DrawString(posX, posY, "Black wins: " + std::to_string(blackWins));
		posY += 10;
		DrawString(posX, posY, "Draws: " + std::to_string(draws));
	}

	void drawChessBoard() {
		int xDrawCoord, yDrawCoord;
		char x, y;
		for (char x_ = 0; x_ < 8; x_++)
			for (char y_ = 0; y_ < 8; y_++) {
				if (!reflect) {
					x = x_;
					y = y_;
				}
				else {
					x = 7 - x_;
					y = 7 - y_;
				}
				if (selectedPiece == std::tuple<char, char>(x, 7 - y))
					FillRect(x_ * chessPieceWidth * chessPieceSize, y_ * chessPieceHeight * chessPieceSize, chessPieceWidth * chessPieceSize, chessPieceHeight * chessPieceSize, olc::CYAN);
				else if (((x_ + y_) % 2 == 0))
					FillRect(x_ * chessPieceWidth * chessPieceSize, y_ * chessPieceHeight * chessPieceSize, chessPieceWidth * chessPieceSize, chessPieceHeight * chessPieceSize, olc::VERY_DARK_GREY);
				else
					FillRect(x_ * chessPieceWidth * chessPieceSize, y_ * chessPieceHeight * chessPieceSize, chessPieceWidth * chessPieceSize, chessPieceHeight * chessPieceSize, olc::DARK_GREY);
			}

		if (whiteCheckmate) {
			char x_ = std::get<0>(board.kingWhiteLocation);
			char y_ = std::get<1>(board.kingWhiteLocation);
			if (!reflect) {
				x = x_;
				y = 7 - y_;
			}
			else {
				x = x_;
				y = 7 - y_;
			}
			FillRect(x * chessPieceWidth * chessPieceSize, y * chessPieceHeight * chessPieceSize, chessPieceWidth * chessPieceSize, chessPieceHeight * chessPieceSize, olc::RED);
		}
		else if (blackCheckmate) {
			char x_ = std::get<0>(board.kingBlackLocation);
			char y_ = std::get<1>(board.kingBlackLocation);
			if (!reflect) {
				x = x_;
				y = 7 - y_;
			}
			else {
				x = 7 - x_;
				y = y_;
			}
			FillRect(x * chessPieceWidth * chessPieceSize, y * chessPieceHeight * chessPieceSize, chessPieceWidth * chessPieceSize, chessPieceHeight * chessPieceSize, olc::RED);
		}

		for (char x_ = 0; x_ < 8; x_++)
			for (char y_ = 0; y_ < 8; y_++) {
				if (!reflect) {
					x = x_;
					y = y_;
				}
				else {
					x = 7 - x_;
					y = 7 - y_;
				}
				if ((state == GameState::MOVING_PIECE) && (piece_movingFrom == std::tuple<char, char>(x, 7 - y))) {
					auto piece_movingFromScreen = boardCoordToScreenCoord(piece_movingFrom);
					auto piece_movingToScreen = boardCoordToScreenCoord(piece_movingTo);
					xDrawCoord = (int)(std::get<0>(piece_movingFromScreen) + ((std::get<0>(piece_movingToScreen) - std::get<0>(piece_movingFromScreen)) * move_t) / MOVE_SEC);
					yDrawCoord = (int)(std::get<1>(piece_movingFromScreen) + ((std::get<1>(piece_movingToScreen) - std::get<1>(piece_movingFromScreen)) * move_t) / MOVE_SEC);
				}
				else {
					xDrawCoord = x_ * chessPieceWidth * chessPieceSize;
					yDrawCoord = y_ * chessPieceHeight * chessPieceSize;
				}
				if (boardPieceTypes[x][y] == PieceType::Pawn)
					drawChessPiece(strPawn, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
				else if (boardPieceTypes[x][y] == PieceType::Knight)
					drawChessPiece(strKnight, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
				else if (boardPieceTypes[x][y] == PieceType::Rook)
					drawChessPiece(strRook, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
				else if (boardPieceTypes[x][y] == PieceType::Bishop)
					drawChessPiece(strBishop, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
				else if (boardPieceTypes[x][y] == PieceType::Queen)
					drawChessPiece(strQueen, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
				else if (boardPieceTypes[x][y] == PieceType::King)
					drawChessPiece(strKing, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
			}

		if ((state == GameState::MOVING_PIECE)) {
			auto piece_movingFromScreen = boardCoordToScreenCoord(piece_movingFrom);
			auto piece_movingToScreen = boardCoordToScreenCoord(piece_movingTo);
			xDrawCoord = (int)(std::get<0>(piece_movingFromScreen) + ((std::get<0>(piece_movingToScreen) - std::get<0>(piece_movingFromScreen)) * move_t) / MOVE_SEC);
			yDrawCoord = (int)(std::get<1>(piece_movingFromScreen) + ((std::get<1>(piece_movingToScreen) - std::get<1>(piece_movingFromScreen)) * move_t) / MOVE_SEC);
			if (!reflect) {
				x = std::get<0>(piece_movingFrom);
				y = 7 - std::get<1>(piece_movingFrom);
			}
			else {
				x = std::get<0>(piece_movingFrom);
				y = 7 - std::get<1>(piece_movingFrom);
			}
			if (boardPieceTypes[x][y] == PieceType::Pawn)
				drawChessPiece(strPawn, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
			else if (boardPieceTypes[x][y] == PieceType::Knight)
				drawChessPiece(strKnight, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
			else if (boardPieceTypes[x][y] == PieceType::Rook)
				drawChessPiece(strRook, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
			else if (boardPieceTypes[x][y] == PieceType::Bishop)
				drawChessPiece(strBishop, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
			else if (boardPieceTypes[x][y] == PieceType::Queen)
				drawChessPiece(strQueen, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
			else if (boardPieceTypes[x][y] == PieceType::King)
				drawChessPiece(strKing, xDrawCoord, yDrawCoord, boardPieceColour[x][y]);
		}

		if (selectedPiece != std::tuple<char, char>(-1, -1))
			for (auto it = possibleMoves.begin(); it != possibleMoves.end(); ++it) {
				char x_ = std::get<0>(*it);
				char y_ = std::get<1>(*it);
				if (!reflect) {
					x = x_;
					y = 7 - y_;
				}
				else {
					x = 7 - x_;
					y = y_;
				}
				xDrawCoord = (int)((x + 0.5f) * chessPieceWidth * chessPieceSize);
				yDrawCoord = (int)((y + 0.5f) * chessPieceHeight * chessPieceSize);
				FillCircle(xDrawCoord, yDrawCoord, chessPieceWidth / 4);
			}
	}

	void drawInfo() {
		int posY = 8 * chessPieceHeight * chessPieceSize + 5;
		std::string s;
		for (auto i = 0; i < moveSequence.size(); i++) {
			s += std::to_string(i + 1) + "." + moveSequence.at(i) + " ";
			if (s.length() > 60) {
				DrawString(0, posY, s);
				posY += 10;
				s = "";
			}
		}
		DrawString(0, posY, s);
	}

	void copyToTempBoard() {
		while (!m.try_lock());
		for (char x = 0; x < 8; x++)
			for (char y = 0; y < 8; y++) {
				if (board.squares[x][7 - y] == nullptr)
					boardPieceTypes[x][y] = PieceType::None;
				else {
					if (board.squares[x][7 - y]->getPieceType() == PieceType::Pawn)
						boardPieceTypes[x][y] = PieceType::Pawn;
					else if (board.squares[x][7 - y]->getPieceType() == PieceType::Bishop)
						boardPieceTypes[x][y] = PieceType::Bishop;
					else if (board.squares[x][7 - y]->getPieceType() == PieceType::Rook)
						boardPieceTypes[x][y] = PieceType::Rook;
					else if (board.squares[x][7 - y]->getPieceType() == PieceType::Knight)
						boardPieceTypes[x][y] = PieceType::Knight;
					else if (board.squares[x][7 - y]->getPieceType() == PieceType::Queen)
						boardPieceTypes[x][y] = PieceType::Queen;
					else if (board.squares[x][7 - y]->getPieceType() == PieceType::King)
						boardPieceTypes[x][y] = PieceType::King;
					boardPieceColour[x][y] = board.squares[x][7 - y]->colour;
				}

			}
		m.unlock();
	}

	void drawChessPiece(std::string piece, int posX, int posY, Colour pieceColour) {
		olc::Pixel colour = (pieceColour == Colour::White ? olc::WHITE : olc::BLACK);
		for (int x = 0; x < chessPieceWidth; x++)
			for (int y = 0; y < chessPieceHeight; y++)
				switch (piece[x + y * chessPieceWidth]) {
				case 'X': FillRect(posX + x * chessPieceSize, posY + y * chessPieceSize, chessPieceSize, chessPieceSize, colour);
				}
	}

	std::tuple<char, char> screenCoordToBoardCoord(std::tuple<int, int> screenCoord) {
		std::tuple<char, char> coord;
		if (!reflect)
			coord = std::tuple<char, char>(std::get<0>(screenCoord) / (chessPieceWidth * chessPieceSize), 7 - std::get<1>(screenCoord) / (chessPieceHeight * chessPieceSize));
		else
			coord = std::tuple<char, char>(7 - std::get<0>(screenCoord) / (chessPieceWidth * chessPieceSize), std::get<1>(screenCoord) / (chessPieceHeight * chessPieceSize));

		//Clamp coordinate to {0,..,7}
		if (std::get<0>(coord) > 7)
			std::get<0>(coord) = 7;
		else if (std::get<0>(coord) < 0)
			std::get<0>(coord) = 0;
		if (std::get<1>(coord) > 7)
			std::get<1>(coord) = 7;
		else if (std::get<1>(coord) < 0)
			std::get<1>(coord) = 0;

		return coord;
	}

	std::tuple<int, int> boardCoordToScreenCoord(std::tuple<char, char> boardCoord) {
		if (!reflect)
			return std::tuple<int, int>(std::get<0>(boardCoord) * (chessPieceWidth * chessPieceSize), (7 - std::get<1>(boardCoord)) * (chessPieceHeight * chessPieceSize));
		else
			return std::tuple<int, int>((7 - std::get<0>(boardCoord)) * (chessPieceWidth * chessPieceSize), std::get<1>(boardCoord) * (chessPieceHeight * chessPieceSize));
	}

	PixelChess()
	{
		sAppName = "Pixel Chess";
	}

	void ai_thread_fct()
	{
		begin = std::chrono::steady_clock::now();
		engine.calculateMove_iterativeDeepening(&board, plTurn);
		end = std::chrono::steady_clock::now();
		char xOrig = std::get<0>(engine.optimalTurnSequence.at(0));
		char yOrig = std::get<1>(engine.optimalTurnSequence.at(0));
		char xDest = std::get<2>(engine.optimalTurnSequence.at(0));
		char yDest = std::get<3>(engine.optimalTurnSequence.at(0));
		copyToTempBoard();
		lastMove = board.moveTo(std::tuple<char, char>(xOrig, yOrig), std::tuple<char, char>(xDest, yDest));
		if (lastMove.validMove) {
			move_t = 0;
			moveSequence.push_back(board.getAlgebraicNotation(lastMove));
			piece_movingFrom = std::tuple<char, char>(xOrig, yOrig);
			piece_movingTo = std::tuple<char, char>(xDest, yDest);
			state = GameState::MOVING_PIECE;
			plTurn = board.flip(plTurn);
			selectedPiece = std::tuple<char, char>(-1, -1);
			updateGameState();
		}
		ai_thread_working = false;
	}

	void updateGameState() {
		if ((plTurn == Colour::White) && (board.getPossibleMoves(Colour::White).empty())) {
			if (board.isChecked(Colour::White)) {
				whiteCheckmate = true;
				blackWins++;
			}
			else {
				stalemate = true;
				draws++;
			}
		}
		else if ((plTurn == Colour::Black) && (board.getPossibleMoves(Colour::Black).empty()))
		{
			if (board.isChecked(Colour::Black)) {
				blackCheckmate = true;
				whiteWins++;
			}
			else {
				stalemate = true;
				draws++;
			}
		}
		else if (board.drawBy50Moves()) {
			draws++;
			stalemate = true;
		}
	}

public:
	bool OnUserCreate() override
	{
		// Called once at the start, so create things here
		copyToTempBoard();
		//engine.updateFct = std::bind(&PixelChess::engine_callback, this, std::placeholders::_1);
		engine.useHashtable = true;
		engine.timeLimit = 3;
		engine.quiescenceLimit = 2;
		engine.nullmove = false;
		board.improvedDrawDetection = true;
		engine.useKingEndgameScoreboard = true;
		engine.MVV_LVAquiescence = true;
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		// called once per frame
		Clear(olc::BLACK);
		if ((GetMouse(0).bReleased) && !(ai_thread_spawned) && !(stalemate) && !(blackCheckmate) && !(whiteCheckmate))
		{
			std::tuple<int, int> screenCoord = std::tuple<int, int>(GetMouseX(), GetMouseY());
			std::tuple<char, char> boardCoord = screenCoordToBoardCoord(screenCoord);
			if (selectedPiece == std::tuple<char, char>(-1, -1)) {
				if ((board.squares[std::get<0>(boardCoord)][std::get<1>(boardCoord)] != nullptr) && (board.squares[std::get<0>(boardCoord)][std::get<1>(boardCoord)]->colour == plTurn)) {
					selectedPiece = boardCoord;
					possibleMoves = board.squares[std::get<0>(boardCoord)][std::get<1>(boardCoord)]->getMoveList(board.squares, std::get<0>(boardCoord), std::get<1>(boardCoord));
				}
			}
			else if ((board.squares[std::get<0>(boardCoord)][std::get<1>(boardCoord)] != nullptr) && (board.squares[std::get<0>(boardCoord)][std::get<1>(boardCoord)]->colour == plTurn)) {
				selectedPiece = boardCoord;
				possibleMoves = board.squares[std::get<0>(boardCoord)][std::get<1>(boardCoord)]->getMoveList(board.squares, std::get<0>(boardCoord), std::get<1>(boardCoord));
			}
			else {
				lastMove = board.moveTo(selectedPiece, boardCoord);
				if (lastMove.validMove) {
					moveSequence.push_back(board.getAlgebraicNotation(lastMove));
					move_t = 0;
					piece_movingFrom = selectedPiece;
					piece_movingTo = boardCoord;
					state = GameState::MOVING_PIECE;
					plTurn = board.flip(plTurn);
					selectedPiece = std::tuple<char, char>(-1, -1);
					updateGameState();
					if ((!blackCheckmate) && (!whiteCheckmate) && (!stalemate) && (mode != GameMode::PLAYERVPLAYER)) {
						ai_thread_working = true;
						ai_thread_spawned = true;
						ai_thread = std::thread(&PixelChess::ai_thread_fct, this);

					}
				}
				else {

				}
			}
		}
		else if ((GetKey(olc::Key::K1).bPressed) && (!ai_thread_spawned))
		{
			selectedPiece = std::tuple<char, char>(-1, -1);
			mode = GameMode::PLAYERVPLAYER;
			plTurn = Colour::White;
			board.resetBoard();
			copyToTempBoard();
			reflect = false;
			whiteCheckmate = false;
			blackCheckmate = false;
			stalemate = false;
			state = GameState::IDLE;
			moveSequence.clear();
		}
		else if ((GetKey(olc::Key::K2).bPressed) && (!ai_thread_spawned))
		{
			selectedPiece = std::tuple<char, char>(-1, -1);
			mode = GameMode::PLAYERVBLACK;
			plTurn = Colour::White;
			board.resetBoard();
			copyToTempBoard();
			reflect = false;
			whiteCheckmate = false;
			blackCheckmate = false;
			stalemate = false;
			state = GameState::IDLE;
			moveSequence.clear();
		}
		else if ((GetKey(olc::Key::K3).bPressed) && (!ai_thread_spawned))
		{
			selectedPiece = std::tuple<char, char>(-1, -1);
			mode = GameMode::PLAYERVWHITE;
			plTurn = Colour::White;
			board.resetBoard();
			copyToTempBoard();
			reflect = true;
			whiteCheckmate = false;
			blackCheckmate = false;
			stalemate = false;
			state = GameState::IDLE;
			ai_thread_working = true;
			ai_thread_spawned = true;
			moveSequence.clear();
			ai_thread = std::thread(&PixelChess::ai_thread_fct, this);
		}

		if (state == GameState::MOVING_PIECE) {
			if (move_t <= MOVE_SEC)
				move_t += fElapsedTime;
			if ((move_t > MOVE_SEC)) {
				move_t = MOVE_SEC;
				if (!ai_thread_spawned) {
					state = GameState::IDLE;
					copyToTempBoard();
				}
			}
		}
		if ((ai_thread_spawned) && !(ai_thread_working)) {
			ai_thread.join();
			ai_thread_spawned = false;
		}

		FillRect(0, 8 * chessPieceHeight * chessPieceSize, 600, 4);
		FillRect(8 * chessPieceWidth * chessPieceSize, 0, 4, 8 * chessPieceHeight * chessPieceSize);
		drawChessBoard();
		drawInfo();
		drawUpdate();
		return true;
	}
};


int main()
{
	PixelChess demo;
	if (demo.Construct(600, 600, 1, 1))
		demo.Start();

	return 0;
}