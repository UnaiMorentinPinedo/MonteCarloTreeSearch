#pragma once
#include "Amazons.h"

//fwd declaration
class MonteCarloTree;

//IMPORTANT, ebcause in each game the indexes change
static int wePlayerIndex;
static int otherPlayerIndex;

class PlayerUnai : public Player
{
  public:
    virtual std::string GetName() const { return "UnaiMorentinPlayer"; }

    virtual int GetNextMoveIndex(Amazons & game, const std::vector<Move> & possibleMoves);

	PlayerUnai();
	~PlayerUnai();

	//AI logic
	MonteCarloTree* mMonteCarlo = nullptr;
};

class MonteCarloTree {
public:
	struct Node {
		//constructor for the root
		Node(int rootIndex);
		//constructo for children
		Node(Node* parent, Move move, bool we);
		~Node();

		void CreateChildren(const Amazons& mTreeGame);
		void PrunPossibleMoves(const std::vector<Move>& possibleMoves, 
			std::vector<std::pair<Move, std::vector<Cell>>>& possiblePrunedMove);

		Node* mParent;
		std::vector<Node*> mChildren;

		Move mMove;
		bool mWe;
		int mPlayerIndex;
		//data
		unsigned int mVisitedTimes;
		int mTotalSimulationReward;
	};
	MonteCarloTree(void);
	~MonteCarloTree(void);

	void Start(const std::vector<Move>& possibleMoves, Amazons& game);
	int Execute(void);
private:
	void End(void);
	void Eraser(Node* n);

	//Principle of operation
	Node* Selection(Node* currentNode);
	Node* Expansion(Node* leafNode);
	bool Simulation(Node* node);
	void BackPropagation(Node*, bool weWon);
	int GetActualMove();

	//Member variables
	Node* mRoot;

	Amazons mTreeGame;
	Amazons mActualGame;

	float mUCTvar;
	float mInitialUCTvar;

	int mCurrentIterations;
	int mMinimumVisitedTimes;
};

void AddMoveToVector(std::vector<std::pair<Move, std::vector<Cell>>>& vector, const Move& move);



static struct {
	int GetMaximumNumberOfIterations() {
		switch (mLevel) {
		default:
			return 1000;
		case Level::L_NORMAL:
			return 10000;
		case Level::L_HARD:
			return 100000;
		case Level::L_GOD_MODE:
			return std::numeric_limits<int>().max();
		}
	}
	double GetTimeout() {
		switch (mLevel) {
		default:
			return 5.;
		case Level::L_NORMAL:
			return 30.;
		case Level::L_HARD:
			return 90.;
		case Level::L_GOD_MODE:
			return 120.;
		}
	}
	enum Level {
		L_EASY,
		L_NORMAL,
		L_HARD,
		L_GOD_MODE
	};
	Level mLevel = Level::L_NORMAL;		//SET HERE THE AI DIFFICULTY 
} mAIDificulty;

