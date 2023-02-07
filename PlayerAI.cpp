/*-------------------------------------------------------
File Name: PlayerAI.cpp
Author: Unai Morentin
---------------------------------------------------------*/

#include "PlayerAI.h"
#include <chrono>

PlayerUnai::PlayerUnai() : Player() {
	mMonteCarlo = new MonteCarloTree();
}

PlayerUnai::~PlayerUnai() {
	delete mMonteCarlo;
}

int PlayerUnai::GetNextMoveIndex(Amazons & game, const std::vector<Move> & possibleMoves)
{
	//depending on the game the index of player changes
	wePlayerIndex = dynamic_cast<PlayerUnai*>(game.GetPlayer(0)) ? 0 : 1;
	otherPlayerIndex = wePlayerIndex == 0 ? 1 : 0;

	mMonteCarlo->Start(possibleMoves, game);
	return mMonteCarlo->Execute();
}

//MONTE CARLO IMPLEMENTATION
MonteCarloTree::MonteCarloTree(void) :
	mRoot(nullptr),
	mCurrentIterations(0),
	mMinimumVisitedTimes(1),
	mInitialUCTvar(5.f),
	mUCTvar(5.f)
{
}

MonteCarloTree::~MonteCarloTree(void) {
}

void MonteCarloTree::Start(const std::vector<Move>& possibleMoves, Amazons& game) {
	mCurrentIterations = 0;

	//create root node (always root will be other player's then from there we will create our children representing we movements)
	mRoot = new Node(wePlayerIndex == 0 ? 1 : 0);

	//get current game status
	mActualGame = game;
}

void MonteCarloTree::End(void) {
	//erase all tree
	Eraser(mRoot);

	mRoot = nullptr;
}

void MonteCarloTree::Eraser(Node* n) {
	for (auto& it : n->mChildren) {
		Eraser(it);
	}

	delete n;
}

//MAIN LOGIC of the algorithm
int MonteCarloTree::Execute(void) {
	//reset uct var value
	mUCTvar = mInitialUCTvar;
	auto start = std::chrono::steady_clock::now();
	while (mCurrentIterations < mAIDificulty.GetMaximumNumberOfIterations()) {
		mCurrentIterations++;
		//reset tree game
		mTreeGame = mActualGame;

		//Principle of operations
		Node* leafNode = Selection(mRoot);
		Node* expandedNode = Expansion(leafNode);
		bool simulationValue = Simulation(expandedNode);
		BackPropagation(expandedNode, simulationValue);

		//TIMEOUT
		auto end = std::chrono::steady_clock::now();
		std::chrono::duration<double> elapsed_seconds = end - start;
		//make uct var smaller, making it explode more than explore
		if (elapsed_seconds.count() > mAIDificulty.GetTimeout() * 0.75)
			mUCTvar /= 2.f;
		if (elapsed_seconds.count() > mAIDificulty.GetTimeout() * 0.5)
			mUCTvar /= 2.f;
		if (elapsed_seconds.count() > mAIDificulty.GetTimeout())
			break;
	}

	return GetActualMove();
}

int MonteCarloTree::GetActualMove() {
	int higherValue = -10000;
	int betterOption_idx = 0;

	//Pick the better move
	for (int idx = 0u; idx < mRoot->mChildren.size(); idx++) {
		if (mRoot->mChildren[idx]->mTotalSimulationReward > higherValue) {
			higherValue = mRoot->mChildren[idx]->mTotalSimulationReward;
			betterOption_idx = idx;
		}
	}

	//erase the created tree
	End();

	return betterOption_idx;
}

//PRINCIPLE OF OPERATIONS ----------------------------------------------------------------------------------------------------------------

	/*Start from root R and select successive child nodes until a leaf node L is reached.
	The root is the current game state and a leaf is any node that has a potential child from which
	no simulation (playout) has yet been initiated.
	Upper Confidence Bound applied to trees or shortly UCT*/
MonteCarloTree::Node* MonteCarloTree::Selection(Node* currentNode) {
	//get to a leaf node
	while (currentNode->mChildren.empty() == false) {
		//Use UCT formula
		float bestUCT = -10000.f;
		Node* betterOption = nullptr;
		//Use UCT formula to determine where to explore the tree from
		for (auto& it : currentNode->mChildren) {
			//if never visited UCT calculation will be +inf
			if (it->mVisitedTimes == 0) {
				betterOption = it;
				break;
			}

			//compute the winrate of the node
			float winRate = static_cast<float>(it->mTotalSimulationReward) / it->mVisitedTimes;
			//if this is the player move this will be our loserate
			if (!it->mWe)
				winRate = 1.f - winRate;

			// balance between expansion and exploration
			float uct_value = winRate + mUCTvar * static_cast<float>(sqrt(log(currentNode->mVisitedTimes) / it->mVisitedTimes));

			if (uct_value > bestUCT) {
				bestUCT = uct_value;
				betterOption = it;
			}
		}

		//move onel level downwards in the tree by selecting the child with better UCT value
		currentNode = betterOption;

		//update the board with the better option while traversing tree
		mTreeGame.ApplyMove(mTreeGame.GetPlayer(currentNode->mPlayerIndex)->GetSymbol(), currentNode->mMove);
	}

	//better UCT leaf node
	return currentNode;
}

/*create one (or more) child nodes and choose node C from one of them. Child nodes are any valid moves
from the game position defined by L.*/
MonteCarloTree::Node* MonteCarloTree::Expansion(Node* leafNode) {
	//if the leaf node has not been visited yet or it is a terminal state
	if (leafNode->mVisitedTimes == 0) {
		//rollout from this node
		return leafNode;
	}
	else {
		//create this node children actions and rollout from one of them
		leafNode->CreateChildren(mTreeGame);
		if (leafNode->mChildren.empty())
			return leafNode;
		return leafNode->mChildren[0];
	}
}


/* Complete one random playout from node C
A playout may be as simple as choosing uniform random moves until the game is decided */
bool MonteCarloTree::Simulation(Node* node) {

	Amazons rollOutGame = mTreeGame;
	bool simulationValue = false;
	
	// Loop till the game concludes
	while (1)
	{
		//get possible moves from this board position
		std::vector<Move> possibleMoves = rollOutGame.GetNextPlayer()->
			CreateMoveList(rollOutGame.GetBoard(), rollOutGame.GetNextPlayer()->GetSymbol());

		// Get next move randomly
		const int move_idx = possibleMoves.empty() ? -1 : rand() % possibleMoves.size();

		// check if there is a movement (otherwise game is finished)
		if (move_idx == -1)
		{
			//game finished
			//check who run out of movements, return true meaning we won
			simulationValue = rollOutGame.GetNextPlayer()->GetName() != "UnaiMorentinPlayer";
			break;
		}
		else {
			// apply movement and change the turn to next player
			rollOutGame.ApplyMove(rollOutGame.GetNextPlayer()->GetSymbol(), possibleMoves[move_idx]);

			rollOutGame.ChangeTurn();
		}
	}

	return simulationValue;
}

/*Use the result of the playout to update information in the nodes on the path from C to R.*/
void MonteCarloTree::BackPropagation(Node* node, bool weWon) {
	if (node == nullptr)		return; //root node

	//update values
	if (weWon)
		node->mTotalSimulationReward++;

	node->mVisitedTimes++;

	BackPropagation(node->mParent, weWon);
}


MonteCarloTree::Node::Node(int rootIndex) : mParent(nullptr), mVisitedTimes(1u),
mTotalSimulationReward(0), mWe(false), mPlayerIndex(rootIndex), mMove(Move()) {
	
}

MonteCarloTree::Node::Node(Node* parent, Move move, bool we) : 
	mTotalSimulationReward(0), mVisitedTimes(0u) {
	//add parent as parent pointer
	mParent = parent;

	//set move
	mMove = move;

	//set variables to know what player this node represents
	mWe = we;
	mWe ? mPlayerIndex = wePlayerIndex : mPlayerIndex = otherPlayerIndex;

	//add this node to the children vector of parent
	parent->mChildren.push_back(this);
}

MonteCarloTree::Node::~Node() {
}

void MonteCarloTree::Node::CreateChildren(const Amazons& mTreeGame) {
	//call Player:: function to get all possible moves from the player that this node does NOT represent
	int childrenPlayerIndex = mPlayerIndex == 0 ? 1 : 0;
	std::vector<Move> possibleMoves = mTreeGame.GetPlayer(childrenPlayerIndex)->CreateMoveList(mTreeGame.GetBoard(),
		mTreeGame.GetPlayer(childrenPlayerIndex)->GetSymbol());

	std::vector<std::pair<Move, std::vector<Cell>>> possiblePrunedMoves;

	//to many possible moves, do pruning
	if (possibleMoves.size() > 350u) {
		PrunPossibleMoves(possibleMoves, possiblePrunedMoves);
		//create node children
		for (auto& it : possiblePrunedMoves) {
			Node* childNode = new Node(this, it.first, !mWe);
			//set a random arrow location
			Cell whereToThrowArrow;
			//check if cassualioty happens that we are shooting arrow where we are moving
			int iterations = 0; //sanity check
			do {
				whereToThrowArrow = it.second[rand() % it.second.size()];
				iterations++;
			} while (childNode->mMove.to.col == whereToThrowArrow.col && childNode->mMove.to.row == whereToThrowArrow.row &&
				iterations <= 100);

			//sanity check if this move is not possible at all
			if (iterations >= 100) {
				mChildren.erase(mChildren.end() - 1);
				delete childNode;
				continue;
			}

			childNode->mMove.arrow = whereToThrowArrow;
		}
	}
	else {
		for (auto& it : possibleMoves) {
			new Node(this, it, !mWe);
		}
	}
}


//get all possible moves and prun them by dividing moves into actual moving or shooting arrow
void MonteCarloTree::Node::PrunPossibleMoves(const std::vector<Move>& possibleMoves,
	std::vector<std::pair<Move, std::vector<Cell>>>& possiblePrunedMoves) {

	for (unsigned i = 0u; i < possibleMoves.size(); i++) {
		AddMoveToVector(possiblePrunedMoves, possibleMoves[i]);
	}
}

//added to the vectors if theere are not there already
void AddMoveToVector(std::vector<std::pair<Move, std::vector<Cell>>>& vector, const Move& move) {
	for (auto& it : vector) {
		if (it.first.from.col == move.from.col && it.first.from.row == move.from.row &&
			it.first.to.col == move.to.col && it.first.to.row == move.to.row) {
			//already in the vector the movement, add possible arrow
			it.second.push_back(move.arrow);
			return;
		}
	}
	//if movement cell was not in vector already create it
	std::vector<Cell> arrowCells;
	arrowCells.push_back(move.arrow);
	vector.push_back({ move, arrowCells });
}