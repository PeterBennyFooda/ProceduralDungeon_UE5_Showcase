// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonPathfinder3D.h"
#include "Windows/AllowWindowsPlatformTypes.h"

/*
 * @brief Default constructor
 */
DungeonPathfinder3D::DungeonPathfinder3D() : unitSize(1)
{
	const FVector size = FVector(1, 1, 1);
	grid = Grid3D<DungeonNode>(size, unitSize, unitSize);

	queue = TPriorityQueue<DungeonNode>();
	closedNodes = TArray<DungeonNode*>();
	stack = TArray<FVector>();

	for (int x = 0; x < size.X; ++x)
	{
		for (int y = 0; y < size.Y; ++y)
		{
			for (int z = 0; z < size.Z; ++z)
			{
				FVector index = FVector(x, y, z);
				grid[index] = DungeonNode(FVector(x, y, z));
			}
		}
	}
}

/*
 * @brief Constructor
 * @param size of the grid
 */
DungeonPathfinder3D::DungeonPathfinder3D(const FVector& size, const int& unitSize) : unitSize(unitSize)
{
	grid = Grid3D<DungeonNode>(size, unitSize, unitSize);

	queue = TPriorityQueue<DungeonNode>();
	closedNodes = TArray<DungeonNode*>();
	stack = TArray<FVector>();

	for (int x = 0; x < size.X; x+=unitSize)
	{
		for (int y = 0; y < size.Y; y+=unitSize)
		{
			for (int z = 0; z < size.Z; z+=unitSize)
			{
				FVector index = FVector(x, y, z);
				grid[index] = DungeonNode(FVector(x, y, z));
			}
		}
	}
}

/*
 * @brief Find the path from the start to the target point
 * @param start point
 * @param end point
 * @param costFunction to calculate the cost of the path
 * @return TArray<FVector> path
 */
TArray<FVector> DungeonPathfinder3D::FindPath(const FVector& start, const FVector& end,
	const std::function<DungeonPathInfo(DungeonNode, DungeonNode)>& costFunction)
{
	return FindPath(start, end, costFunction, true);
}
TArray<FVector> DungeonPathfinder3D::FindPath(const FVector& start, const FVector& end,
	const std::function<DungeonPathInfo(DungeonNode, DungeonNode)>& costFunction, bool canChangeFloors)
{
	ResetNodes();
	queue.Empty();
	closedNodes.Empty();

	if(start == end)
		return TArray<FVector>();

	grid[start].Cost = 0;
	queue.Push(grid[start]);

	// Adjust the directions based on the unit size
	TArray<FVector> adjustDirections;
	if(!canChangeFloors)
	{
		for(auto& offset : Directions2D)
		{
			adjustDirections.Add(offset * unitSize);
		}
	}
	else
	{
		for(auto& offset : Directions)
		{
			adjustDirections.Add(offset * unitSize);
		}
	}

	int loopCount = 0;
	while(queue.Num() > 0)
	{
		loopCount++;

		// if(loopCount >= 1000)
		// {
		// 	break;
		// }
		
		DungeonNode tmp = queue.Pop();
		DungeonNode* node = &grid[tmp.Position];
		
		closedNodes.Add(node);

		// Reverse the node linked list to get the path
		if(node->Position == end)
		{
			return ReconstructPath(node);
		}

		// Find the neighbors and update the node
		for(auto& offset : adjustDirections)
		{
			// Check if the node is within the bounds
			if(!grid.InBounds(node->Position + offset)) continue;

			// Check if the node is closed
			DungeonNode* nb = &grid[node->Position + offset];
			if(closedNodes.Contains(nb)) continue;

			// Check if the neighbor is already checked
			if(node->PreviousSet.Contains(nb->Position)) continue;

			// Check if the path is traversable
			DungeonPathInfo pathInfo = costFunction(*node, *nb);
			if(!pathInfo.Traversable) continue;

			if(pathInfo.IsStairs)
			{
				int xDir = FMath::Clamp(FMath::RoundToInt(offset.X), -unitSize, unitSize);
				int yDir = FMath::Clamp(FMath::RoundToInt(offset.Y), -unitSize, unitSize);
				FVector vertircalOffset = FVector(0, 0, offset.Z);
				FVector horizontalOffset = FVector(xDir, yDir, 0);

				// Check if the stairs positions is valid
				if(node->PreviousSet.Contains(node->Position + horizontalOffset)
					|| node->PreviousSet.Contains(node->Position + horizontalOffset*2)
					|| node->PreviousSet.Contains(node->Position + horizontalOffset + vertircalOffset)
					|| node->PreviousSet.Contains(node->Position + horizontalOffset*2 + vertircalOffset))
				{
					continue;
				}
			}

			// Update the cost and previous node
			float newCost = node->Cost + pathInfo.Cost;
			if(newCost < nb->Cost)
			{
				nb->Previous = node;
				nb->Cost = newCost;

				queue.Push(*nb);
				
				// Update the previous set
				nb->PreviousSet.Empty();
				nb->PreviousSet.Append(node->PreviousSet);
				nb->PreviousSet.Add(node->Position);

				// Add the stairs positions to the previous set
				if(pathInfo.IsStairs)
				{
					int xDir = FMath::Clamp(FMath::RoundToInt(offset.X), -unitSize, unitSize);
					int yDir = FMath::Clamp(FMath::RoundToInt(offset.Y), -unitSize, unitSize);
					FVector vertircalOffset = FVector(0, 0, offset.Z);
					FVector horizontalOffset = FVector(xDir, yDir, 0);

					nb->PreviousSet.Add(node->Position + horizontalOffset);
					nb->PreviousSet.Add(node->Position + horizontalOffset*2);
					nb->PreviousSet.Add(node->Position + horizontalOffset + vertircalOffset);
					nb->PreviousSet.Add(node->Position + horizontalOffset*2 + vertircalOffset);
				}
			}
		}
	}

	return TArray<FVector>();
}

/*
 * @brief Get the neighbors of a position
 * @param pos to get the neighbors from
 * @return TArray<FVector> neighbors
 */
TArray<FVector> DungeonPathfinder3D::GetNebighors(const FVector& pos)
{
	// Adjust the directions based on the unit size
	TArray<FVector> adjustDirections;
	for(auto& offset : Directions)
	{
		adjustDirections.Add(offset * unitSize);
	}

	TArray<FVector> result;
	for(auto& offset : adjustDirections)
	{
		// Check if the node is within the bounds
		if(!grid.InBounds(pos + offset)) continue;

		result.Add(pos + offset);
	}

	return result;
}

/*
 * @brief Get the neighbors of a position in 2D space
 * @param pos to get the neighbors from
 * @return TArray<FVector> neighbors
 */
TArray<FVector> DungeonPathfinder3D::GetNebighors2D(const FVector& pos)
{
	// Adjust the directions based on the unit size
	TArray<FVector> adjustDirections;
	for(auto& offset : Directions2D)
	{
		adjustDirections.Add(offset * unitSize);
	}

	TArray<FVector> result;
	for(auto& offset : adjustDirections)
	{
		// Check if the node is within the bounds
		if(!grid.InBoundsIgnoreOffset(pos + offset)) continue;

		FVector tmp = pos + offset;
		result.Add(tmp);
	}

	return result;
}

// ============ Helper Functions ============

/*
 * @brief Reset the nodes for the pathfinding
 */
void DungeonPathfinder3D::ResetNodes()
{
	FVector size = grid.GetSize();

	for (int x = 0; x < size.X; x+=unitSize)
	{
		for (int y = 0; y < size.Y; y+=unitSize)
		{
			for (int z = 0; z < size.Z; z+=unitSize)
			{
				FVector index = FVector(x, y, z);
				grid[index].Previous = nullptr;
				grid[index].Cost = MAX_flt;
				grid[index].PreviousSet.Empty();
			}
		}
	}
}

/*
 * @brief Reconstruct the path from the node
 * @param node to reconstruct the path from
 * @return TArray<FVector> path
 */
TArray<FVector> DungeonPathfinder3D::ReconstructPath(DungeonNode* node)
{
	TArray<FVector> result = TArray<FVector>();
	stack = TArray<FVector>();
	
	// Reverse the node linked list to get the path
	while(node != nullptr)
	{
		if(stack.Contains(node->Position))
			break;
		stack.Push(node->Position);
		node = node->Previous;
	}
	
	while(stack.Num() > 0)
	{
		result.Add(stack.Pop());
	}

	return result;
}
