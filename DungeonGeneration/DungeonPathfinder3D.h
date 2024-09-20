// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NetworkingPrototype/DungeonGeneration/Grid3D.h"
#include "NetworkingPrototype/DungeonGeneration/TPriorityQueue.h"
#include <functional>

// Node for the dungeon pathfinding
struct DungeonNode
{
public:
	FVector Position;
	DungeonNode* Previous;
	TArray<FVector> PreviousSet;
	float Cost;

	DungeonNode()
	{
		Position = FVector::ZeroVector;
		Previous = nullptr;
		PreviousSet = TArray<FVector>();
		Cost = MAX_flt;
	}

	DungeonNode(FVector pos)
	{
		Position = pos;
		Previous = nullptr;
		PreviousSet = TArray<FVector>();
		Cost= MAX_flt;
	}

	bool operator==(const DungeonNode& other) const
	{
		return Position == other.Position && Cost == other.Cost && Previous == other.Previous && PreviousSet == other.PreviousSet;
	}

	bool operator<(const DungeonNode& other) const
	{
		return Cost < other.Cost;
	}
};

// Cost of a path in the dungeon
struct DungeonPathInfo
{
public:
	bool Traversable = false;
	float Cost = 0.f;
	bool IsStairs = false;
};

// Offset for the all directions from a node in 3D space
static const FVector Directions[] =
{
	// Horizontal
	FVector(1, 0, 0),
	FVector(-1, 0, 0),
	FVector(0, 1, 0),
	FVector(0, -1, 0),

	// Diagnostic directions up
	FVector(3, 0, 1),
	FVector(-3, 0, 1),
	FVector(0, 3, 1),
	FVector(0, -3, 1),

	// Diagnostic directions down
	FVector(3, 0, -1),
	FVector(-3, 0, -1),
	FVector(0, 3, -1),
	FVector(0, -3, -1)
};

// Offset for the all directions from a node in 2D space
static const FVector Directions2D[] =
{
	// Horizontal directions
	FVector(1, 0, 0),
	FVector(-1, 0, 0),
	FVector(0, 1, 0),
	FVector(0, -1, 0)
};

/**
 * 
 */
class NETWORKINGPROTOTYPE_API DungeonPathfinder3D
{
public:
	DungeonPathfinder3D();
	DungeonPathfinder3D(const FVector& size, const int& unitSize);

	TArray<FVector> FindPath(const FVector& start, const FVector& end, const std::function<DungeonPathInfo(DungeonNode, DungeonNode)>& costFunction);
	TArray<FVector> FindPath(const FVector& start, const FVector& end, const std::function<DungeonPathInfo(DungeonNode, DungeonNode)>& costFunction, bool canChangeFloors);
	TArray<FVector> GetNebighors(const FVector& pos);
	TArray<FVector> GetNebighors2D(const FVector& pos);

private:
	void ResetNodes();
	TArray<FVector> ReconstructPath(DungeonNode* node);
	
	Grid3D<DungeonNode> grid;
	int unitSize = 1;
	
	TArray<FVector> stack;
	TPriorityQueue<DungeonNode> queue;
	
	TArray<DungeonNode*> closedNodes;
};
