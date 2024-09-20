// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonGenerator.h"

#include "Net/UnrealNetwork.h"

// Sets default values
ADungeonGenerator::ADungeonGenerator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	bReplicates = true;
	bAlwaysRelevant = true;
	freeGenerationMode = false;
}

// Called when the game starts or when spawned
void ADungeonGenerator::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ADungeonGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

void ADungeonGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADungeonGenerator, ReplicatedRoomLocations);
	DOREPLIFETIME(ADungeonGenerator, IsGenerated);
}

/*
 * @brief Spawn a room at the transform location
 * @param FTransform transform of the room
 * @param TSubclassOf<AMainRoom> The class reference of the room to spawn
 * @return AMainRoom* The spawned room
 */
AMainRoom* ADungeonGenerator::SpawnStructure(FTransform transform, TSubclassOf<AMainRoom> room, bool checkCollision = true)
{
	if(checkCollision &&
		IsLocationOccupied(
			transform.GetLocation(),
			transform.GetRotation(), 
			FVector(1.0f, 1.0f, 1.0f)
			))
	{
		return nullptr;
	}
	
	if(IsValid(room->GetClass()))
	{
		return GetWorld()->SpawnActor<AMainRoom>(room, transform);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Room class is invalid or null!"));
		return nullptr;
	}
}

/*
 * @brief Spawn a door at the transform location
 * @param FTransform transform of the door
 * @param TSubclassOf<ABasicDoor> The class reference of the door to spawn
 * @return ABasicDoor* The spawned door
 */
ABasicDoor* ADungeonGenerator::SpawnDoor(FTransform transform, TSubclassOf<ABasicDoor> door)
{
	if(IsValid(door->GetClass()))
	{
		return GetWorld()->SpawnActor<ABasicDoor>(door, transform);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Door class is invalid or null!"));
		return nullptr;
	}
}

/*
 * @brief Generate the unconnected rooms
 * @param FTransform Starting point of the dungeon
 * @param int the number of rooms to spawn
 */
void ADungeonGenerator::GenerateRooms(FTransform startingPoint, int roomSpawnSteps)
{
	// Generate the entrance room
	AMainRoom* entranceSpawned = SpawnStructure(startingPoint, EntranceRoom, false);
	
	if(!entranceSpawned)
	{
		UE_LOG(LogTemp, Error, TEXT("Entrance Room is invalid or null!"));
		return;
	}

	entranceSpawned->InitInfo(startingPoint, FVector::OneVector, entranceSpawned->Bounds);
	spawnedRooms.Add(TArray<AMainRoom*>());
	spawnedRooms[0].Add(entranceSpawned);
	currentRoomGroupIndex++;

	// Check if we have room to spawn
	if(RoomList.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Room list is empty or null!"));
		return;
	}
	
	// Generate the rest of the rooms
	for(int i = 1; i<roomSpawnSteps; ++i)
	{
		if(IsRoomProcGen)
		{
			GenerateProcGenRooms();
		}
		else
		{
			GeneratePremadeRooms();
		}
	}
	
	// Sort the room map
	floorRoomMap.KeySort([](int a, int b) { return a < b; });
}

/*
 * @brief Triangulate the rooms for hallway generation
 */
void ADungeonGenerator::Triangulate()
{
	if(IsDungeonFloorBased)
	{
		for(auto& floor : floorRoomMap)
		{
			for(const auto& room : floor.Value)
			{
				if(!floorVertexMap.Contains(floor.Key))
				{
					floorVertexMap.Add(floor.Key, TArray<FVector>());
				}
				FVector center = FVector(room->Bounds.GetCenter().X, room->Bounds.GetCenter().Y, room->Bounds.Min.Z);
				floorVertexMap[floor.Key].Add(center);
			}

			// Add room positions as stair vertices for stairs generation
			if(!floorStairVertexMap.Contains(floor.Key))
			{
				floorStairVertexMap.Add(floor.Key, TArray<FVector>());
			}
			for(int i = 0; i<MaxStairCaseCount; ++i)
			{
				int roomIndex = FMath::RandRange(0, floor.Value.Num()-1);
				floorStairVertexMap[floor.Key].Add(floorVertexMap[floor.Key][roomIndex]);
			}
		}

		// Add next floor stair vertices to the current floor vertices
		for(auto it = floorStairVertexMap.CreateConstIterator(); it; ++it)
		{
			auto nextIt = it;
			++nextIt;

			if(nextIt)
			{
				for(auto& stairVert : floorStairVertexMap[nextIt.Key()])
				{
					floorVertexMap[it.Key()].Add(stairVert);
				}
			}
			else
			{
				// We still need vertex at different Z so we can triangulate the floor
				FVector vert = FVector(DungeonUnit, DungeonUnit, DungeonUnit);
				floorVertexMap[it.Key()].Add(vert);
			}
		}
	}
	else
	{
		for(const auto& roomGroup : spawnedRooms)
		{
			for(const auto& room : roomGroup)
			{
				roomVertices.Add(room->Bounds.GetCenter());
			}
		}

		// Triangulate the rooms for hallway generation
		delaunay.Triangulate(roomVertices);	
	}
}

/*
 * @brief Find possible hallway routes between rooms
 */
void ADungeonGenerator::FindPossibleHallways()
{
	if(IsDungeonFloorBased)
	{
		FindPossibleHallwaysFloorBased();
	}
	else
	{
		FindPossibleHallwaysNormal();
	}
}

/*
 * @brief Generate the optimal path between rooms
 */
void ADungeonGenerator::GenerateHallways()
{
	pathfinder = DungeonPathfinder3D(DungeonSize, DungeonUnit);
	
	if(IsDungeonFloorBased)
	{
		GenerateFloorBasedHallways();
	}
	else
	{
		GenerateNormalHallways();
	}
}

/*
 * @brief Generate the courtyard if needed
 */
void ADungeonGenerator::GenerateCourtyard()
{
	if(!DebugMode && IsDungeonFloorBased && IsGroundFloorCourtyard)
	{
		FVector scale = FVector::OneVector;
		const int index = FMath::RandRange(0, RoomList.Num()-1);
		const TSubclassOf<AMainRoom> newRoom = RoomList[index];
		
		FBox defaultBounds = newRoom->GetDefaultObject<AMainRoom>()->GetComponentsBoundingBox();
		FVector defaultOrigin, defaultExtent;
		defaultOrigin = defaultBounds.GetCenter();
		defaultExtent = DefaultRoomSize * 0.5f;
		
		int z = DungeonUnit*(GroundFloorIndex+1);
		for(int y = DungeonUnit*2; y<DungeonSize.Y-DungeonUnit; y+=DungeonUnit)
		{
			for(int x = DungeonUnit*2; x<DungeonSize.X-DungeonUnit; x+=DungeonUnit)
			{
				FVector location = FVector(x, y, z);
				FVector newOrigin = location + defaultOrigin;
				FVector newExtent = scale * defaultExtent + sizeGap;
				FBox newBounds = FBox(newOrigin - newExtent, newOrigin + newExtent);

				FTransform transform = FTransform(FRotator::ZeroRotator, location, FVector::OneVector);
				AMainRoom* newRoomSpawned = nullptr;
					
				switch(grid[location])
				{
				case EStructureType::NONE:
					newRoomSpawned = SpawnStructure(transform, newRoom);
					if(newRoomSpawned)
					{
						newRoomSpawned->InitInfo(transform, scale, newBounds);
						spawnedRooms[GroundFloorIndex].Add(newRoomSpawned);

						if(!floorRoomMap.Contains(location.Z))
						{
							floorRoomMap.Add(location.Z, TArray<AMainRoom*>());
						}
						floorRoomMap[location.Z].Add(newRoomSpawned);

						// Set the structure type of the room in the grid
						if(DefaultRoomSize.X > 1 && DefaultRoomSize.Y > 1 && DefaultRoomSize.Z > 1)
						{
							TArray<FVector> posInRoom = GetAllIntegerPointsInBox(newRoomSpawned->Bounds);
							for(auto& pos : posInRoom)
							{
								grid[pos] = EStructureType::ROOM;
							}
						}
						else
						{
							grid[location] = EStructureType::ROOM;
						}
					}
					break;
				default:
					break;
				}
			}
		}
	}
}

/*
 * @brief Generate the cellings
 */
void ADungeonGenerator::GenerateCeilings()
{
	if(!DebugMode && IsDungeonFloorBased && ShouldGenerateBuilding)
	{
		FVector scale = FVector::OneVector;
		const int index = FMath::RandRange(0, RoomList.Num()-1);
		const TSubclassOf<AMainRoom> newRoom = RoomList[index];
		
		FBox defaultBounds = newRoom->GetDefaultObject<AMainRoom>()->GetComponentsBoundingBox();
		FVector defaultOrigin, defaultExtent;
		defaultOrigin = defaultBounds.GetCenter();
		defaultExtent = DefaultRoomSize * 0.5f;

		int startFloor = DungeonUnit*(GroundFloorIndex+2);
		if(startFloor >= DungeonSize.Z)
			startFloor = DungeonSize.Z - DungeonUnit;
		
		for(int z = startFloor; z<NormalFloorSize.Z; z+=DungeonUnit)
		{
			for(int y = DungeonUnit*2; y<NormalFloorSize.Y; y+=DungeonUnit)
			{
				for(int x = DungeonUnit*2; x<NormalFloorSize.X; x+=DungeonUnit)
				{
					FVector location = FVector(x, y, z);
					FVector newOrigin = location + defaultOrigin;
					FVector newExtent = scale * defaultExtent + sizeGap;
					FBox newBounds = FBox(newOrigin - newExtent, newOrigin + newExtent);

					FTransform transform = FTransform(FRotator::ZeroRotator, location, FVector::OneVector);
					AMainRoom* newRoomSpawned = nullptr;

					if(!grid.InBoundsIgnoreOffset(location))
						continue;
					
					switch(grid[location])
					{
					case EStructureType::NONE:
						newRoomSpawned = SpawnStructure(transform, newRoom, false);
						if(newRoomSpawned)
						{
							newRoomSpawned->InitInfo(transform, scale, newBounds);
						}
						break;
					default:
						break;
					}
				}
			}	
		}
	}
}

/*
 * @brief Generate the walls
 */
void ADungeonGenerator::GenerateWalls()
{
	if(WallList.Num() < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Wall list is empty or null!"));
		return;
	}

	// Spawn walls for each room
	for (auto& roomGroup : spawnedRooms)
	{
		int doorCounter = 0;
		int randomDoorLimit = FMath::RandRange(1, MaxDoorCount);

		for (auto& room : roomGroup)
		{
			FVector pos = FVector(room->Bounds.GetCenter().X, room->Bounds.GetCenter().Y, room->Bounds.Min.Z);
			TArray<FVector> nbs = pathfinder.GetNebighors2D(pos);
			TArray<FVector> doorPoints = room->GetDoorPoints();

			for (auto& nb : nbs)
			{
				FVector wallDirection = nb - pos;
				wallDirection = wallDirection.GetSafeNormal2D();
				float yawRotation = FMath::Atan2(wallDirection.Y, wallDirection.X) * (180.0f / PI);

				FRotator wallRot = FRotator(0, yawRotation, 0);

				// Spawn walls but don't block the stairs and room itself
				FVector wallPos = pos + (nb - pos) * 0.5f;
				int height = room->Bounds.GetSize().Z / DungeonUnit;

				//if(IsRoomProcGen)
				//{
					// Walls should be at least 1 unit high bur 1 unit less than the bounds height
					if (height > 1)
					{
						height--;
					}

					// Check if door count maxed out
					if (doorPoints.Contains(wallPos))
					{
						// Check if a door must be spawned
						bool mustSpawnDoor = false;
						if (grid[nb] == EStructureType::STAIRS) // If the wall is next to stairs
						{
							mustSpawnDoor = true;
						}

						TArray<FVector> nbs2 = pathfinder.GetNebighors2D(nb);
						// If the wall's neighbor is next to stairs
						for (auto& nb2 : nbs2)
						{
							if (grid[nb2] == EStructureType::STAIRS)
							{
								mustSpawnDoor = true;
								break;
							}
						}

						// If so spawn a wall instead of a door
						if (doorCounter >= randomDoorLimit && !mustSpawnDoor)
						{
							if (grid[nb] != EStructureType::ROOM)
							{
								for (int i = 0; i < height; ++i)
								{
									FVector finalWallPos = wallPos + FVector(0, 0, i * DungeonUnit);
									FTransform transform = FTransform(wallRot, finalWallPos, FVector::OneVector);
									SpawnStructure(transform, WallList[0], false);
								}
							}
						}
						else if(!DoorList.IsEmpty())
						{
							if(!spawnedDoors.Contains(wallPos))
							{
								FTransform transform = FTransform(wallRot, wallPos, FVector::OneVector);
								ABasicDoor* spawnedDoor = SpawnDoor(transform, DoorList[0]);
								spawnedDoors.Add(wallPos, spawnedDoor);
							}
						}

						// Spawn walls above the door if needed
						if (height > 1)
						{
							if (grid[nb] != EStructureType::ROOM)
							{
								for (int i = 1; i < height; ++i)
								{
									FVector finalWallPos = wallPos + FVector(0, 0, i * DungeonUnit);
									FTransform transform = FTransform(wallRot, finalWallPos, FVector::OneVector);
									SpawnStructure(transform, WallList[0], false);
								}
							}
						}

						doorCounter++;
					}

					// Spawn walls
					if (!doorPoints.Contains(wallPos) && grid[nb] != EStructureType::ROOM && grid[nb] != EStructureType::STOP)
					{
						// Spawn walls based on the height of the room
						for (int i = 0; i < height; ++i)
						{
							FVector finalWallPos = wallPos + FVector(0, 0, i * DungeonUnit);
							FTransform transform = FTransform(wallRot, finalWallPos, FVector::OneVector);
							SpawnStructure(transform, WallList[0], false);
						}
					}
				//}
			}
		}
	}

	// Spawn walls for each hallway
	for(auto& pos : hallwaysVertices)
	{
		TArray<FVector> nbs = pathfinder.GetNebighors2D(pos);

		for(auto& nb : nbs)
		{
			FVector wallDirection = nb - pos;
			wallDirection = wallDirection.GetSafeNormal2D();
			float yawRotation = FMath::Atan2(wallDirection.Y, wallDirection.X) * (180.0f / PI);

			FVector wallPos = pos + (nb - pos) * 0.5f;
			FRotator wallRot = FRotator(0, yawRotation, 0);
			
			if(grid[nb] == EStructureType::NONE || grid[nb] == EStructureType::STOP)
			{
				FTransform transform = FTransform(wallRot, wallPos, FVector::OneVector);
				SpawnStructure(transform, WallList[0], false);
			}
		}
	}

	// Spawn outer walls
	if (IsDungeonFloorBased && ShouldGenerateBuilding)
	{
		for (int z = 0; z < DungeonSize.Z; z += DungeonUnit)
		{
			for (int y = 0; y < DungeonSize.Y; y += DungeonUnit)
			{
				for (int x = 0; x < DungeonSize.X; x += DungeonUnit)
				{
					FVector pos = FVector(x, y, z);
					if (pos.Z == DungeonUnit * (GroundFloorIndex + 1))
					{
						if ((pos.X == DungeonUnit * 2 && pos.Y <= NormalFloorSize.Y)
							|| (pos.Y == DungeonUnit * 2 && pos.X <= NormalFloorSize.X)
							|| (pos.X == NormalFloorSize.X && pos.Y <= NormalFloorSize.Y)
							|| (pos.Y == NormalFloorSize.Y && pos.X <= NormalFloorSize.X))
						{
							TArray<FVector> nbs = pathfinder.GetNebighors2D(pos);
							for (auto& nb : nbs)
							{
								FVector wallDirection = nb - pos;
								wallDirection = wallDirection.GetSafeNormal2D();
								float yawRotation = FMath::Atan2(wallDirection.Y, wallDirection.X) * (180.0f / PI);

								FVector wallPos = pos + (nb - pos) * 0.5f;
								FRotator wallRot = FRotator(0, yawRotation, 0);
								
								FTransform transform = FTransform(wallRot, wallPos, FVector::OneVector);

								if(grid[nb] == EStructureType::NONE)
									SpawnStructure(transform, WallList[0], false);

								// Spawn walls based on the height of the room
								int outerWallHeight = (DungeonSize.Z - DungeonUnit * 2) / DungeonUnit;
								// Exclude basement
								for (int i = 0; i < outerWallHeight; ++i)
								{
									FVector nbHeight = FVector(nb.X, nb.Y, nb.Z + i * DungeonUnit);
									if(grid[nbHeight] != EStructureType::NONE)
										continue;
									
									FVector finalWallPos = wallPos + FVector(0, 0, i * DungeonUnit);
									FTransform transform2 = FTransform(wallRot, finalWallPos, FVector::OneVector);
									SpawnStructure(transform2, WallList[0], false);
								}
							}
						}
					}
				}
			}
		}
	}
}

/*
 * @brief Generate the dungeon
 * @param FTransform Starting point of the dungeon
 * @param int the number of rooms to spawn
 */
void ADungeonGenerator::GenerateDungeon(FTransform startingPoint, int roomCount)
{
	// Reset variables
	grid = Grid3D<EStructureType>(DungeonSize, DungeonUnit, DungeonUnit);
	
	spawnedRooms.Empty();
	
	selectedEdges.Empty();
	roomVertices.Empty();
	hallwaysVertices.Empty();

	floorRoomCount.Empty();
	currentFloorIndex = 0;
	for(int i = 0; i<=currentFloorIndex; ++i)
	{
		floorRoomCount.Add(0);
	}

	// Generate the rooms
	GenerateRooms(startingPoint, roomCount);

	// Triangulate the rooms
	Triangulate();

	// Find all possible connections between rooms
	FindPossibleHallways();

	// Generate the optimal path between rooms
	GenerateHallways();

	// Clean up the dungeon
	CleanUpDungeon();

	// Generate courtyard if needed
	GenerateCourtyard();

	// Generate the ceilings
	GenerateCeilings();

	// Generate walls
	GenerateWalls();

	// Set the dungeon as generated
	IsGenerated = true;

	// DEBUG
	// loop through the grid and print the structure type
	if(DebugMode)
	{
		for(int z = 0; z<DungeonSize.Z; z+=DungeonUnit)
		{
			for(int y = 0; y<DungeonSize.Y; y+=DungeonUnit)
			{
				for(int x = 0; x<DungeonSize.X; x+=DungeonUnit)
				{
					FVector pos = FVector(x, y, z);
					switch(grid[pos])
					{
					case EStructureType::STOP:
						DrawDebugSphere(GetWorld(), pos, 0.25*DungeonUnit, 8, FColor::White, true, -1);
							break;
					case EStructureType::ROOM:
						if(DebugType == EDungenDebugType::ROOM|| DebugType == EDungenDebugType::ALL)
							DrawDebugSphere(GetWorld(), pos, 0.25*DungeonUnit, 8, FColor::Blue, true, -1);
						break;
					case EStructureType::HALLWAY:
						if(DebugType == EDungenDebugType::HALLWAY|| DebugType == EDungenDebugType::ALL)
							DrawDebugSphere(GetWorld(), pos, 0.25*DungeonUnit, 8, FColor::Green, true, -1);
						break;
					case EStructureType::STAIRS:
						if(DebugType == EDungenDebugType::STAIRS|| DebugType == EDungenDebugType::ALL)
							DrawDebugSphere(GetWorld(), pos, 0.25*DungeonUnit, 8, FColor::Cyan, true, -1);
						break;
					default:
						break;
					}
				}
			}
		}
	}
}

/*
 * @brief Get a random room location
 * @return FVector Random room location
 */
FVector ADungeonGenerator::GetRandomRoomLocation()
{
	if(ReplicatedRoomLocations.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Room locations are empty or null!"));
		return FVector::ZeroVector;
	}
	
	int randomIndex = FMath::RandRange(0, ReplicatedRoomLocations.Num()-1);
	return ReplicatedRoomLocations[randomIndex];
}

/*
 * @brief Get the room locations
 * @return TArray<FVector> Room locations
 */
int ADungeonGenerator::GetCurrentFloorNumber(const FVector& location) const
{
	FVector pos = location.GridSnap(DungeonUnit);
	float Zpos = pos.Z;
	int floor = (Zpos-DungeonUnit*3)/DungeonUnit;
	return floor;
}

// ============ Helper Functions ============

/*
 * @brief Get random room properties
 * @param FVector& location of the room
 * @param FVector& scale of the room
 */
void ADungeonGenerator::GetRandomRoomProperties(TArray<FVector>& locations, FVector& scale)
{
	// For easier calculations, we only return integer values
	int scaleX = FMath::RandRange(static_cast<int>(MinRoomScale.X), static_cast<int>(MaxRoomScale.X));
	int scaleY = FMath::RandRange(static_cast<int>(MinRoomScale.Y), static_cast<int>(MaxRoomScale.Y));
	int scaleZ = FMath::RandRange(static_cast<int>(MinRoomScale.Z), static_cast<int>(MaxRoomScale.Z));
	
	// Get the center location of the room
	FVector centerlocation;

	if(!freeGenerationMode)
	{
		centerlocation = FVector(
			GetRandomNumberWithInterval(0, static_cast<int>(NormalFloorSize.X)),
			GetRandomNumberWithInterval(0, static_cast<int>(NormalFloorSize.Y)),
			DungeonUnit*(currentFloorIndex+1)
		);
	}
	else
	{
		centerlocation = FVector(
			GetRandomNumberWithInterval(0, static_cast<int>(NormalFloorSize.X)),
			GetRandomNumberWithInterval(0, static_cast<int>(NormalFloorSize.Y)),
			GetRandomNumberWithInterval(0, static_cast<int>(NormalFloorSize.Z))
		);
	}

	locations.Add(centerlocation);
	if(scaleX>1)
	{
		// Get all locations left of the center
		for(int i = 0; i<scaleX-1; ++i)
		{
			FVector offset = FVector(-DungeonUnit*(i+1), 0, 0);
			locations.Add(centerlocation + offset);
		}

		// Get all locations right  of the center
		for(int i = 0; i<scaleX-1; ++i)
		{
			FVector offset = FVector(DungeonUnit*(i+1), 0, 0);
			locations.Add(centerlocation + offset);
		}
	}

	if(scaleY>1)
	{
		// Get all locations backward of the center
		for(int i = 0; i<scaleY-1; ++i)
		{
			FVector offset = FVector(0, -DungeonUnit*(i+1), 0);
			locations.Add(centerlocation + offset);
		}

		// Get all locations forward of the center
		for(int i = 0; i<scaleY-1; ++i)
		{
			FVector offset = FVector(0, DungeonUnit*(i+1), 0);
			locations.Add(centerlocation + offset);
		}
	}

	if(scaleX>1 && scaleY>1)
	{
		// Get all locations left-backward of the center
		for(int i = 0; i<scaleX-1; ++i)
		{
			for(int j = 0; j<scaleY-1; ++j)
			{
				FVector offset = FVector(-DungeonUnit*(i+1), -DungeonUnit*(j+1), 0);
				locations.Add(centerlocation + offset);
			}
		}

		// Get all locations right-backward of the center
		for(int i = 0; i<scaleX-1; ++i)
		{
			for(int j = 0; j<scaleY-1; ++j)
			{
				FVector offset = FVector(DungeonUnit*(i+1), -DungeonUnit*(j+1), 0);
				locations.Add(centerlocation + offset);
			}
		}

		// Get all locations left-forward of the center
		for(int i = 0; i<scaleX-1; ++i)
		{
			for(int j = 0; j<scaleY-1; ++j)
			{
				FVector offset = FVector(-DungeonUnit*(i+1), DungeonUnit*(j+1), 0);
				locations.Add(centerlocation + offset);
			}
		}

		// Get all locations right-forward of the center
		for(int i = 0; i<scaleX-1; ++i)
		{
			for(int j = 0; j<scaleY-1; ++j)
			{
				FVector offset = FVector(DungeonUnit*(i+1), DungeonUnit*(j+1), 0);
				locations.Add(centerlocation + offset);
			}
		}
	}

	scaleX = scaleX + (scaleX-1);
	scaleY = scaleY + (scaleY-1);
	scaleZ = scaleZ + (scaleZ-1);
	scale = FVector(
			scaleX,
			scaleY,
			scaleZ
		);
}

/*
 * @brief Get a random number within an interval
 * @param int min
 * @param int max
 * @return int Random number within the interval
 */
int ADungeonGenerator::GetRandomNumberWithInterval(int min, int max) const
{
	int32 range = (max - min) / DungeonUnit + 1;
	int32 randomValue = FMath::RandRange(0, range - 1);
	return min + randomValue * DungeonUnit;
}

/*
 * @brief Check if two rooms intersect
 * @param const FBox& roomA
 * @param const FBox& roomB
 * @return bool True if the rooms intersect
 */
bool ADungeonGenerator::ChckeRoomIntersection(const FBox& roomA, const FBox& roomB)
{
	return roomA.Intersect(roomB);
}

/*
 * @brief Get all integer points in a box
 * @param const FBox& Box
 * @return TArray<FVector> All integer points in the box
 */
TArray<FVector> ADungeonGenerator::GetAllIntegerPointsInBox(const FBox& Box)
{
	TArray<FVector> Points;

	// Ensure that Min and Max are rounded to the nearest integers
	FVector Min = Box.Min.GridSnap(DungeonUnit);
	FVector Max = Box.Max.GridSnap(DungeonUnit);

	// Iterate over all integer coordinates within the box
	for (int32 X = FMath::FloorToInt(Min.X); X <= FMath::FloorToInt(Max.X); X+=DungeonUnit)
	{
		for (int32 Y = FMath::FloorToInt(Min.Y); Y <= FMath::FloorToInt(Max.Y); Y+=DungeonUnit)
		{
			for (int32 Z = FMath::FloorToInt(Min.Z); Z < FMath::FloorToInt(Max.Z); Z+=DungeonUnit)
			{
				FVector pos = FVector(X, Y, Z);
				Points.Add(pos);
			}
		}
	}

	return Points;
}

/*
 * @brief Kruskal’s Algorithm to find the minimum spanning tree(MST)
 * @param const TArray<FEdge>& edges
 * @param const FVector& startVertex
 * @return TArray<FEdge> The minimum spanning tree
 */
TArray<FEdge> ADungeonGenerator::MinimumSpanningTree(const TArray<FEdge>& edges, const FVector& startVertex)
{
	TSet<FVector> openSet;
	TSet<FVector> closedSet;
	TArray<FEdge> results;

	// Initialize openSet with all vertices in the edges
	for (const FEdge& edge : edges)
	{
		openSet.Add(edge.Vertex[0]);
		openSet.Add(edge.Vertex[1]);
	}

	// Add the start vertex to closedSet
	closedSet.Add(startVertex);

	while (openSet.Num() > 0)
	{
		bool chosen = false;
		FEdge chosenEdge;
		float minWeight = std::numeric_limits<float>::infinity();

		// Find the edge with the minimum weight that connects a closed vertex to an open vertex
		for (const FEdge& edge : edges)
		{
			int closedVertices = 0;
			if (!closedSet.Contains(edge.Vertex[0])) closedVertices++;
			if (!closedSet.Contains(edge.Vertex[1])) closedVertices++;

			if (closedVertices != 1) continue;

			// Calculate the weight of the edge
			float weight = FVector::Distance(edge.Vertex[0], edge.Vertex[1]);
			if (weight < minWeight)
			{
				chosenEdge = edge;
				chosen = true;
				minWeight = weight;
			}
		}

		// If no edge was chosen, break out of the loop
		if (!chosen) break;

		// Add the chosen edge to the results
		results.Add(chosenEdge);
		openSet.Remove(chosenEdge.Vertex[0]);
		openSet.Remove(chosenEdge.Vertex[1]);
		closedSet.Add(chosenEdge.Vertex[0]);
		closedSet.Add(chosenEdge.Vertex[1]);
	}

	return results;
}

/*
 * @brief Add random edges to the MST to create a more complex dungeon
 * @param const TArray<FEdge>& originalEdges
 * @param TArray<FEdge>& mstEdges
 */
TArray<FEdge> ADungeonGenerator::AddRandomEdgesToMST(const TArray<FEdge>& originalEdges, TArray<FEdge>& mstEdges,
	float additionalEdgeProbability)
{
	TArray<FEdge> mazeEdges = mstEdges;

	// Filter out the edges that are not part of the MST
	TArray<FEdge> remainingEdges;
	for (const FEdge& edge : originalEdges)
	{
		if (!mstEdges.Contains(edge))
		{
			remainingEdges.Add(edge);
		}
	}
	
	// Shuffle remaining edges to randomize their order
	int32 numEdges = remainingEdges.Num();
	for (int32 i = numEdges - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i); // Random index from 0 to i
		remainingEdges.Swap(i, j); // Swap elements to shuffle
	}

	// Add random remaining edges to the maze
	for (const FEdge& edge : remainingEdges)
	{
		if (FMath::FRand() < additionalEdgeProbability)
		{
			mazeEdges.Add(edge);
		}
	}

	return mazeEdges;
}

/*
 * @brief Generate rooms using procedural generation
 */
void ADungeonGenerator::GenerateProcGenRooms()
{
	// Get random room properties for new room
	TArray<FVector> locations = TArray<FVector>();
	FVector scale = FVector::OneVector;
	FVector totalScale = FVector::OneVector;
	GetRandomRoomProperties(locations, totalScale);

	// First, check if the room compound fits in the dungeon
	bool canAdd = true;

	const FVector centerRoomLocation = locations[0];
	const int index = FMath::RandRange(0, RoomList.Num() - 1);
	const TSubclassOf<AMainRoom> newRoom = RoomList[index];

	FBox defaultBounds = newRoom->GetDefaultObject<AMainRoom>()->GetComponentsBoundingBox();
	FVector defaultOrigin, defaultExtent;
	defaultOrigin = defaultBounds.GetCenter();
	defaultExtent = DefaultRoomSize * 0.5f;

	FVector centerOrigin = centerRoomLocation + defaultOrigin;
	FVector centerExtent = totalScale * defaultExtent + sizeGap;
	FBox totalBounds = FBox(centerOrigin - centerExtent, centerOrigin + centerExtent);

	// Check if the new room intersects with existing rooms
	for (auto& roomGroup : spawnedRooms)
	{
		for (auto& room : roomGroup)
		{
			const FBox bounds = FBox(
				room->GetActorLocation() - room->Scale * defaultExtent,
				room->GetActorLocation() + room->Scale * defaultExtent
			);

			if (ChckeRoomIntersection(bounds, totalBounds))
			{
				UE_LOG(LogTemp, Warning, TEXT("Room location overlap!"));
				canAdd = false;
				break;
			}
		}
	}

	// Check if the new room is out of bounds (border cells will be ignored too)
	if (totalBounds.Min.X < DungeonUnit || totalBounds.Max.X >= (DungeonSize.X - DungeonUnit)
		|| totalBounds.Min.Y < DungeonUnit || totalBounds.Max.Y >= (DungeonSize.Y - DungeonUnit)
		|| totalBounds.Min.Z < DungeonUnit || totalBounds.Max.Z >= (DungeonSize.Z - DungeonUnit))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Map Size] X: %f, Y: %f, Z: %f"), DungeonSize.X, DungeonSize.Y, DungeonSize.Z);
		UE_LOG(LogTemp, Warning, TEXT("[Mins] X: %f, Y: %f, Z: %f"), totalBounds.Min.X, totalBounds.Min.Y,
		       totalBounds.Min.Z);
		UE_LOG(LogTemp, Warning, TEXT("[Maxs] X: %f, Y: %f, Z: %f"), totalBounds.Max.X, totalBounds.Max.Y,
		       totalBounds.Max.Z);
		canAdd = false;
	}

	// If room location is valid, spawn the room 
	if (canAdd)
	{
		// Update scale Z for the room
		scale = FVector(scale.X, scale.Y, totalScale.Z);

		// Add a new room group
		spawnedRooms.Add(TArray<AMainRoom*>());

		// Increase the ground floor room count
		RoomCountCalculation(centerRoomLocation);

		// Generate rooms in the group
		for (auto& location : locations)
		{
			FVector newOrigin = location + defaultOrigin;
			FVector newExtent = scale * defaultExtent + sizeGap;
			FBox newBounds = FBox(newOrigin - newExtent, newOrigin + newExtent);

			FTransform transform = FTransform(FRotator::ZeroRotator, location, FVector::OneVector);
			AMainRoom* newRoomSpawned = SpawnStructure(transform, newRoom);

			if (newRoomSpawned)
			{
				newRoomSpawned->InitInfo(transform, scale, newBounds);
				spawnedRooms[currentRoomGroupIndex].Add(newRoomSpawned);
				ReplicatedRoomLocations.Add(location);

				if (!floorRoomMap.Contains(location.Z))
				{
					floorRoomMap.Add(location.Z, TArray<AMainRoom*>());
				}
				floorRoomMap[location.Z].Add(newRoomSpawned);

				// Set the structure type of the room in the grid
				if (DefaultRoomSize.X > 1 && DefaultRoomSize.Y > 1 && DefaultRoomSize.Z > 1)
				{
					TArray<FVector> posInRoom = GetAllIntegerPointsInBox(newRoomSpawned->Bounds);
					for (auto& pos : posInRoom)
					{
						grid[pos] = EStructureType::ROOM;
					}
				}
				else
				{
					grid[location] = EStructureType::ROOM;
				}
			}
		}

		// Increase the room group index cuz this group is done
		currentRoomGroupIndex++;
	}
}

/*
 * @brief Generate premade rooms
 */
void ADungeonGenerator::GeneratePremadeRooms()
{
	// Get random room properties for new room
	TArray<FVector> locations = TArray<FVector>();
	FVector scale = FVector::OneVector;
	FVector totalScale = FVector::OneVector;
	GetRandomRoomProperties(locations, totalScale);

	// First, check if the room compound fits in the dungeon
	bool canAdd = true;

	const FVector centerRoomLocation = locations[0];
	const int index = FMath::RandRange(0, RoomList.Num() - 1);
	const TSubclassOf<AMainRoom> newRoom = PremadeRoomList[index];

	FBox defaultBounds = newRoom->GetDefaultObject<AMainRoom>()->Bounds;
	FVector defaultOrigin, defaultExtent;
	defaultOrigin = defaultBounds.GetCenter();
	defaultExtent = defaultBounds.GetExtent();

	FVector centerOrigin = centerRoomLocation + defaultOrigin;
	FVector centerExtent = defaultExtent + sizeGap;
	FBox newBounds = FBox(centerOrigin - centerExtent, centerOrigin + centerExtent);

	// Check if the new room intersects with existing rooms
	for(auto& bound : premadeBounds)
	{
		if(ChckeRoomIntersection(bound, newBounds))
		{
			UE_LOG(LogTemp, Warning, TEXT("Room location overlap!"));
			canAdd = false;
			break;
		}
	}

	// Check if the new room is out of bounds (border cells will be ignored too)
	if (newBounds.Min.X < DungeonUnit*2 || newBounds.Max.X >= (DungeonSize.X - DungeonUnit)
		|| newBounds.Min.Y < DungeonUnit*2 || newBounds.Max.Y >= (DungeonSize.Y - DungeonUnit)
		|| newBounds.Min.Z < DungeonUnit || newBounds.Max.Z >= (DungeonSize.Z - DungeonUnit))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Map Size] X: %f, Y: %f, Z: %f"), DungeonSize.X, DungeonSize.Y, DungeonSize.Z);
		UE_LOG(LogTemp, Warning, TEXT("[Mins] X: %f, Y: %f, Z: %f"), newBounds.Min.X, newBounds.Min.Y,
			   newBounds.Min.Z);
		UE_LOG(LogTemp, Warning, TEXT("[Maxs] X: %f, Y: %f, Z: %f"), newBounds.Max.X, newBounds.Max.Y,
			   newBounds.Max.Z);
		canAdd = false;
	}

	// If room location is valid, spawn the room
	if (canAdd)
	{
		// Add new bounds to the list
		premadeBounds.Add(newBounds);

		// Set tiles in room as non-walkable first
		TArray<FVector> stopInRoom = GetAllIntegerPointsInBox(newBounds);
		for (auto& pos : stopInRoom)
		{
			grid[pos] = EStructureType::STOP;
		}
		
		// Update scale Z for the room
		scale = FVector(scale.X, scale.Y, totalScale.Z);
		
		// Add a new room group
		spawnedRooms.Add(TArray<AMainRoom*>());

		// Increase floor room count
		RoomCountCalculation(centerRoomLocation);
		
		// Generate rooms in the group
		FTransform transform = FTransform(FRotator::ZeroRotator, centerRoomLocation, FVector::OneVector);
		AMainRoom* newRoomSpawned = SpawnStructure(transform, newRoom);
		if (newRoomSpawned)
		{
			newRoomSpawned->InitInfo(transform, scale, newBounds);
			ReplicatedRoomLocations.Add(centerRoomLocation);
			cleanupMap.Add(newRoomSpawned, TArray<AMainRoom*>());
			
			for(auto& innerPos : newRoomSpawned->InnerPaths)
			{
				FVector pathPos = innerPos.GridSnap(DungeonUnit);
				FTransform pathTransform = FTransform(FRotator::ZeroRotator, pathPos, FVector::OneVector);
				AMainRoom* newPathSpawned = SpawnStructure(pathTransform, PathTileInPremadeRoom, false);
				
				FBox defaultPathBounds = PathTileInPremadeRoom->GetDefaultObject<AMainRoom>()->Bounds;
				FVector defaultPathOrigin, defaultPathExtent;
				defaultPathOrigin = defaultBounds.GetCenter();
				defaultPathExtent = DefaultRoomSize * 0.5f;
				
				FVector newPathOrigin = pathPos + defaultPathOrigin;
				FVector newPathExtent = scale * defaultPathExtent + sizeGap;
				FBox nePathBounds = FBox(newPathOrigin - newPathExtent, newPathOrigin + newPathExtent);

				if (newPathSpawned)
				{
					newPathSpawned->InitInfo(pathTransform, scale, nePathBounds);
					spawnedRooms[currentRoomGroupIndex].Add(newPathSpawned);
					cleanupMap[newRoomSpawned].Add(newPathSpawned);

					if (!floorRoomMap.Contains(centerRoomLocation.Z))
					{
						floorRoomMap.Add(centerRoomLocation.Z, TArray<AMainRoom*>());
					}
					floorRoomMap[centerRoomLocation.Z].Add(newPathSpawned);

					// Set the structure type of the inner path in the grid
					grid[pathPos] = EStructureType::ROOM;
				}
			}
		}

		// Increase the room group index cuz this group is done
		currentRoomGroupIndex++;
	}
}

/*
 * @brief Calculate and update the room count
 * @param const FVector& centerRoomLocation
 */
void ADungeonGenerator::RoomCountCalculation(const FVector& centerRoomLocation)
{
	if(currentFloorIndex > floorRoomCount.Num()-1)
		floorRoomCount.Add(0);
		
	if (IsGroundFloor(centerRoomLocation))
	{
		floorRoomCount[GroundFloorIndex]++;
		currentGroundFloorRoomCount++;
	}
	else
	{
		floorRoomCount[currentFloorIndex]++;
	}

	if(IsGroundFloor(centerRoomLocation))
	{
		if(currentGroundFloorRoomCount >= MinGroundFloorRoomCount)
		{
			currentFloorIndex++;
			if(DungeonUnit*(currentFloorIndex+1) > NormalFloorSize.Z)
			{
				currentFloorIndex--;
				freeGenerationMode = true;
			}
		}
	}
	else
	{
		if(floorRoomCount[currentFloorIndex] >= MinRoomCount)
		{
			currentFloorIndex++;
			if(DungeonUnit*(currentFloorIndex+1) > NormalFloorSize.Z)
			{
				currentFloorIndex--;
				freeGenerationMode = true;
			}
		}
	}
}

/*
 * @brief Find possible hallway routes between rooms
 */
void ADungeonGenerator::FindPossibleHallwaysNormal()
{
	TArray<FEdge> uniqueEdges;
	TArray<FVector3d> points = roomVertices;
	TArray<FIntVector4> tetrahedra = delaunay.GetTetrahedra();

	if(tetrahedra.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Tetrahedra is empty or null!"));
		return;
	}

	// Extract edges from each tetrahedron
	for (const FIntVector4& tetrahedron : tetrahedra)
	{
		FVector verts[4] = {
			points[tetrahedron.X],
			points[tetrahedron.Y],
			points[tetrahedron.Z],
			points[tetrahedron.W]
		};

		// Add all 6 unique edges of the tetrahedron
		uniqueEdges.Add(FEdge(verts[0], verts[1]));
		uniqueEdges.Add(FEdge(verts[0], verts[2]));
		uniqueEdges.Add(FEdge(verts[0], verts[3]));
		uniqueEdges.Add(FEdge(verts[1], verts[2]));
		uniqueEdges.Add(FEdge(verts[1], verts[3]));
		uniqueEdges.Add(FEdge(verts[2], verts[3]));
	}

	// Save the MST
	selectedEdges = MinimumSpanningTree(uniqueEdges, points[0]);

	// Add random edges to the MST to create a more complex dungeon
	selectedEdges = AddRandomEdgesToMST(uniqueEdges, selectedEdges, LoopProbability);

	// DEBUG LINES
	// if(DebugMode)
	// {
	// 	for (const FEdge& edge : selectedEdges)
	// 	{
	// 		DrawDebugLine(GetWorld(), edge.Vertex[0], edge.Vertex[1], FColor::Blue, true, -1, 0, 0.15f);
	// 	}
	// }
}

/*
 * @brief Find possible hallway routes between rooms based on the floor
 */
void ADungeonGenerator::FindPossibleHallwaysFloorBased()
{
	for(auto& floor : floorVertexMap)
	{
		TArray<FVector> vertices = floor.Value;
		delaunay.Triangulate(vertices );
		
		TArray<FEdge> uniqueEdges;
		TArray<FVector3d> points = vertices ;
		TArray<FIntVector4> tetrahedra = delaunay.GetTetrahedra();

		if(tetrahedra.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("Tetrahedra is empty or null!"));
			return;
		}

		// Extract edges from each tetrahedron
		for (const FIntVector4& tetrahedron : tetrahedra)
		{
			FVector verts[4] = {
				points[tetrahedron.X],
				points[tetrahedron.Y],
				points[tetrahedron.Z],
				points[tetrahedron.W]
			};

			// Add all 6 unique edges of the tetrahedron
			uniqueEdges.Add(FEdge(verts[0], verts[1]));
			uniqueEdges.Add(FEdge(verts[0], verts[2]));
			uniqueEdges.Add(FEdge(verts[0], verts[3]));
			uniqueEdges.Add(FEdge(verts[1], verts[2]));
			uniqueEdges.Add(FEdge(verts[1], verts[3]));
			uniqueEdges.Add(FEdge(verts[2], verts[3]));
		}

		// Save the MST
		if(!floorEdgeMap.Contains(floor.Key))
		{
			floorEdgeMap.Add(floor.Key, TArray<FEdge>());
		}
		floorEdgeMap[floor.Key] = MinimumSpanningTree(uniqueEdges, points[0]);
		
		
		// Add random edges to the MST to create a more complex dungeon
		floorEdgeMap[floor.Key] = AddRandomEdgesToMST(uniqueEdges, floorEdgeMap[floor.Key], LoopProbability);

		// DEBUG LINES
		// if(DebugMode)
		// {
		// 	for (const FEdge& edge : floorEdgeMap[floor.Key])
		// 	{
		// 		DrawDebugLine(GetWorld(), edge.Vertex[0], edge.Vertex[1], FColor::Blue, true, -1, 0, 0.15f);
		// 	}
		// }
	}
}

/*
 * @brief Generate normal hallways
 */
void ADungeonGenerator::GenerateNormalHallways()
{
	//DEBUG
	int DEBUG_COUNTER = 0;	

	for(auto& edge : selectedEdges)
	{
		FVector& startPos = edge.Vertex[0];
		FVector& endPos = edge.Vertex[1];

		DEBUG_COUNTER++;
		UE_LOG(LogTemp, Verbose, TEXT("EDGES_COUNTER: %d"), DEBUG_COUNTER);
		
		// Get the path between the two rooms
		TArray<FVector> path = pathfinder.FindPath(startPos, endPos, [&](const DungeonNode& a, const DungeonNode& b)
		{
			return CostFunction(a, b, endPos);
		});

		// If the path is valid, set the structure type
		if(path.Num() > 0)
		{
			for(int i = 0; i<path.Num(); ++i)
			{
				FVector current = path[i];

				// If the position on the grid is empty, set it to hallway
				if(grid[current] == EStructureType::NONE)
				{
					grid[current] = EStructureType::HALLWAY;
				}

				if(i>0)
				{
					FVector pre = path[i-1];
					FVector delta = current - pre;
					
					FVector spawnDirection = delta.GetSafeNormal2D();
					float YawRotation = FMath::Atan2(spawnDirection.Y, spawnDirection.X) * (180.0f / PI);

					//if(IsRoomProcGen)
					//{
						// Set door positions of rooms
						if(grid[current] != EStructureType::ROOM && grid[current] != EStructureType::NONE && grid[current] != EStructureType::STOP)
						{
							if(grid[pre] == EStructureType::ROOM)
							{
								for (auto& roomGroup : spawnedRooms)
								{
									for(auto& room : roomGroup)
									{
										if(room->Bounds.GetCenter() == pre)
										{
											// Door is between the current and previous position
											FVector doorPoint = pre + delta * 0.5f;
											room->AddDoorPoint(doorPoint);
											room->IsConnectedToHallway = true;
										}
									}
								}
							}
						}
						else if(grid[current] == EStructureType::ROOM)
						{
							if(grid[pre] != EStructureType::ROOM && grid[pre] != EStructureType::NONE && grid[pre] != EStructureType::STOP)
							{
								for (auto& roomGroup : spawnedRooms)
								{
									for(auto& room : roomGroup)
									{
										if(room->Bounds.GetCenter() == current)
										{
											// Door is between the current and previous position
											FVector doorPoint = pre + delta * 0.5f;
											room->AddDoorPoint(doorPoint);
											room->IsConnectedToHallway = true;
										}
									}
								}
							}
						}
					//}
					
					if(delta.Z != 0)
					{
						int xDir = FMath::Clamp(FMath::RoundToInt(delta.X), -DungeonUnit, DungeonUnit);
						int yDir = FMath::Clamp(FMath::RoundToInt(delta.Y), -DungeonUnit, DungeonUnit);
						FVector verticalOffset = FVector(0, 0, delta.Z);
						FVector horizontalOffset = FVector(xDir, yDir, 0);

						grid[pre + horizontalOffset] = EStructureType::STAIRS;
						grid[pre + horizontalOffset*2] = EStructureType::STAIRS;
						grid[pre + horizontalOffset + verticalOffset] = EStructureType::STAIRS;
						grid[pre + horizontalOffset*2 + verticalOffset] = EStructureType::STAIRS;

						// Spawn stairs
						if(StairsList.Num()>0 && (!DebugMode || (DebugMode && DebugWithModels)))
						{
							AMainRoom* spawnedStair = nullptr;
							// Goes up
							if(delta.Z > 0)
							{
								YawRotation += 90.f;
								FRotator spawnRot = FRotator(0, YawRotation, 0);
								FTransform transform = FTransform(spawnRot, pre + horizontalOffset, FVector::OneVector);
								spawnedStair = SpawnStructure(transform, StairsList[0], false);
								if(!spawnedStair)
									continue;
								
								spawnedStair->InitInfo(transform, FVector::OneVector, spawnedStair->GetComponentsBoundingBox());
							}
							// Goes down
							else if (delta.Z <0)
							{
								YawRotation -= 90.f;
								FRotator spawnRot = FRotator(0, YawRotation, 0);
								FTransform transform = FTransform(spawnRot, pre + horizontalOffset*2 + verticalOffset, FVector::OneVector);
								spawnedStair = SpawnStructure(transform, StairsList[0], false);
								if(!spawnedStair)
									continue;
								
								spawnedStair->InitInfo(transform, FVector::OneVector, spawnedStair->GetComponentsBoundingBox());
							}
						}
					}
				}
			}

			// Spawn hallways
			FVector prePos = path[0];
			for(auto& pos : path)
			{
				FVector curPos = pos;
				if((!DebugMode || (DebugMode && DebugWithModels)))
				{
					if(grid[curPos] == EStructureType::HALLWAY && HallwayList.Num()>0)
					{
						FTransform transform = FTransform(FRotator::ZeroRotator, pos, FVector::OneVector);
						AMainRoom* spawnedHallway = SpawnStructure(transform, HallwayList[0], false);
						hallwaysVertices.Add(pos);
						if(!spawnedHallway)
							continue;
						
						spawnedHallway->InitInfo(transform, FVector::OneVector, spawnedHallway->GetComponentsBoundingBox());
					}
				}

				if(DebugMode)
				{
					DrawDebugLine(GetWorld(), prePos, curPos, FColor::Red, true, -1, 0, 0.15f);
					prePos = curPos;
				}
				
			}
		}
	}
}

/*
 * @brief Generate floor based hallways
 */
void ADungeonGenerator::GenerateFloorBasedHallways()
{
	//DEBUG
	int DEBUG_COUNTER = 0;

	for(auto& floor : floorEdgeMap)
	{
		int stairCount = 0;
		for(auto& edge : floor.Value)
		{
			FVector& startPos = edge.Vertex[0];
			FVector& endPos = edge.Vertex[1];

			TArray<FVector> path;
			if(endPos.Z == startPos.Z)
			{
				// Find path if there is no stairs
				path = pathfinder.FindPath(startPos, endPos, [&](const DungeonNode& a, const DungeonNode& b)
				{
					return CostFunction(a, b, endPos);
				}, false);
			}
			else if(stairCount < MaxStairCaseCount)
			{
				// Find path if there are stairs
				path = pathfinder.FindPath(startPos, endPos, [&](const DungeonNode& a, const DungeonNode& b)
				{
					return CostFunction(a, b, endPos);
				}, true);

				stairCount++;
			}

			if(path.Num() > 0)
			{
				for(int i = 0; i<path.Num(); ++i)
				{
					FVector current = path[i];

					// If the position on the grid is empty, set it to hallway
					if(grid[current] == EStructureType::NONE)
					{
						grid[current] = EStructureType::HALLWAY;
					}

					if(i>0)
					{
						FVector pre = path[i-1];
						FVector delta = current - pre;
						
						FVector spawnDirection = delta.GetSafeNormal2D();
						float YawRotation = FMath::Atan2(spawnDirection.Y, spawnDirection.X) * (180.0f / PI);

						//if(IsRoomProcGen)
						//{
							// Set door positions the room
							if(grid[current] != EStructureType::ROOM && grid[current] != EStructureType::NONE && grid[current] != EStructureType::STOP)
							{
								if(grid[pre] == EStructureType::ROOM)
								{
									for (auto& roomGroup : spawnedRooms)
									{
										for(auto& room : roomGroup)
										{
											FVector roomPos = FVector(room->Bounds.GetCenter().X, room->Bounds.GetCenter().Y, room->Bounds.Min.Z);
											if(roomPos == pre)
											{
												// Door is between the current and previous position
												FVector doorPoint = pre + delta * 0.5f;
												room->AddDoorPoint(doorPoint);
												room->IsConnectedToHallway = true;
											}
										}
									}
								}
							}
							else if(grid[current] == EStructureType::ROOM)
							{
								if(grid[pre] != EStructureType::ROOM && grid[pre] != EStructureType::NONE && grid[pre] != EStructureType::STOP)
								{
									for (auto& roomGroup : spawnedRooms)
									{
										for(auto& room : roomGroup)
										{
											FVector roomPos = FVector(room->Bounds.GetCenter().X, room->Bounds.GetCenter().Y, room->Bounds.Min.Z);
											if(roomPos == current)
											{
												// Door is between the current and previous position
												FVector doorPoint = pre + delta * 0.5f;
												room->AddDoorPoint(doorPoint);
												room->IsConnectedToHallway = true;
											}
										}
									}
								}
							}
						//}
						
						if(delta.Z != 0)
						{
							int xDir = FMath::Clamp(FMath::RoundToInt(delta.X), -DungeonUnit, DungeonUnit);
							int yDir = FMath::Clamp(FMath::RoundToInt(delta.Y), -DungeonUnit, DungeonUnit);
							FVector verticalOffset = FVector(0, 0, delta.Z);
							FVector horizontalOffset = FVector(xDir, yDir, 0);

							grid[pre + horizontalOffset] = EStructureType::STAIRS;
							grid[pre + horizontalOffset*2] = EStructureType::STAIRS;
							grid[pre + horizontalOffset + verticalOffset] = EStructureType::STAIRS;
							grid[pre + horizontalOffset*2 + verticalOffset] = EStructureType::STAIRS;

							// Spawn stairs
							if(StairsList.Num()>0 && (!DebugMode || (DebugMode && DebugWithModels)))
							{
								AMainRoom* spawnedStair = nullptr;
								// Goes up
								if(delta.Z > 0)
								{
									YawRotation += 90.f;
									FRotator spawnRot = FRotator(0, YawRotation, 0);
									FTransform transform = FTransform(spawnRot, pre + horizontalOffset, FVector::OneVector);
									spawnedStair = SpawnStructure(transform, StairsList[0], false);
									if(!spawnedStair)
										continue;
									
									spawnedStair->InitInfo(transform, FVector::OneVector, spawnedStair->GetComponentsBoundingBox());
								}
								// Goes down
								else if (delta.Z <0)
								{
									YawRotation -= 90.f;
									FRotator spawnRot = FRotator(0, YawRotation, 0);
									FTransform transform = FTransform(spawnRot, pre + horizontalOffset*2 + verticalOffset, FVector::OneVector);
									spawnedStair = SpawnStructure(transform, StairsList[0], false);
									if(!spawnedStair)
										continue;
									
									spawnedStair->InitInfo(transform, FVector::OneVector, spawnedStair->GetComponentsBoundingBox());
								}
							}
						}
					}
				}

				// Spawn hallways
				FVector prePos = path[0];
				for(auto& pos : path)
				{
					FVector curPos = pos;
					if((!DebugMode || (DebugMode && DebugWithModels)))
					{
						if(grid[curPos] == EStructureType::HALLWAY && HallwayList.Num()>0)
						{
							FTransform transform = FTransform(FRotator::ZeroRotator, pos, FVector::OneVector);
							AMainRoom* spawnedHallway = SpawnStructure(transform, HallwayList[0], false);
							hallwaysVertices.Add(pos);
							if(!spawnedHallway)
								continue;
							
							spawnedHallway->InitInfo(transform, FVector::OneVector, spawnedHallway->GetComponentsBoundingBox());
						}
					}
					
					if(DebugMode)
					{
						DrawDebugLine(GetWorld(), prePos, curPos, FColor::Red, true, -1, 0, 0.15f);
						prePos = curPos;
					}
				}
			}
		}	
	}
}

/*
 * @brief Clean up the dungeon
 */
void ADungeonGenerator::CleanUpDungeon()
{
	if(DebugMode)
		return;
	
	if(IsRoomProcGen)
	{
		for(auto& roomGroup : spawnedRooms)
		{
			TArray<AMainRoom*> toBeReomved;
			for(auto& room : roomGroup)
			{
				if(!room->IsConnectedToHallway)
				{
					toBeReomved.Add(room);
				}
			}

			for(auto& room : toBeReomved)
			{
				if(ReplicatedRoomLocations.Contains(room->Transform.GetLocation()))
				{
					ReplicatedRoomLocations.Remove(room->Transform.GetLocation());
				}

				roomGroup.Remove(room);
				room->Destroy();
			}
		}
	}
	else
	{
		TArray<AMainRoom*> toBeReomved;
		for(auto& roomGroup: cleanupMap)
		{
			bool connected = false;
			for(auto& innerPath: roomGroup.Value)
			{
				if(innerPath->IsConnectedToHallway)
				{
					connected = true;
					break;
				}
			}

			if(!connected)
			{
				toBeReomved.Add(roomGroup.Key);
				for(auto& innerPath: roomGroup.Value)
				{
					toBeReomved.Add(innerPath);
				}
			}
		}

		for(auto& room : toBeReomved)
		{
			// Remove the main room spawn points from the replicated list
			if(ReplicatedRoomLocations.Contains(room->Transform.GetLocation()))
			{
				ReplicatedRoomLocations.Remove(room->Transform.GetLocation());
			}

			// Remove the room instance from the spawned rooms list
			for(auto& roomGroup: spawnedRooms)
			{
				if(roomGroup.Contains(room))
				{
					roomGroup.Remove(room);
					break;
				}
				room->Destroy();
			}
		}
	}
}

/*
 * @brief Check if a location is occupied
 * @param const FVector& location
 * @param const FQuat& rotation
 * @param const FVector& extent
 * @return bool True if the location is occupied
 */
bool ADungeonGenerator::IsLocationOccupied(const FVector& location, const FQuat& rotation, const FVector& extent) const
{
	FCollisionQueryParams CollisionParams;

	FHitResult HitResult;

	// Perform a sweep using a box or sphere collision shape
	bool bIsOccupied = GetWorld()->SweepSingleByChannel(
		HitResult,
		location,
		location,
		rotation,
		ECC_Visibility,  // Choose an appropriate collision channel
		FCollisionShape::MakeBox(extent),  // or MakeSphere(Extent.X)
		CollisionParams
	);

	// Optionally visualize the collision shape
	DrawDebugBox(GetWorld(), location, extent, rotation, bIsOccupied ? FColor::Red : FColor::Green, false, 2.0f);

	return bIsOccupied;
}

/*
 * @brief Check if current room location is at ground floor
 * @param const FVector& location
 * @return bool True if the location is at ground floor
 */
bool ADungeonGenerator::IsGroundFloor(const FVector& location) const
{
	return location.Z == DungeonUnit * (GroundFloorIndex + 1);
}

/*
 * @brief cost function for the pathfinder
 * @param const DungeonNode& a
 * @param const DungeonNode& b
 * @param const FVector& endPos
 * @return DungeonPathInfo
 */
DungeonPathInfo ADungeonGenerator::CostFunction(const DungeonNode& a, const DungeonNode& b, const FVector& endPos)
{
	DungeonPathInfo info = DungeonPathInfo();

	FVector delta = b.Position - a.Position;

	// Flat path
	if(delta.Z == 0)
	{
		info.Cost = FVector::Distance(b.Position, endPos);

		if(grid[b.Position] == EStructureType::STAIRS || grid[b.Position] == EStructureType::STOP)
			return info;
		else if(grid[b.Position] == EStructureType::ROOM)
			info.Cost += RoomExtraCost;
		else if(grid[b.Position] == EStructureType::NONE)
			info.Cost += NoneExtraCost;

		info.Traversable = true;
	}
	else // Stairs path
	{
		if((grid[a.Position] != EStructureType::NONE && grid[a.Position] != EStructureType::HALLWAY)
			|| (grid[b.Position] != EStructureType::NONE && grid[b.Position] != EStructureType::HALLWAY))
		{
			return info;
		}

		// Base cost + distance
		info.Cost = BaseCost + FVector::Distance(b.Position, endPos) + ChangeFloorExtraCost;

		int xDir = FMath::Clamp(FMath::RoundToInt(delta.X), -DungeonUnit, DungeonUnit);
		int yDir = FMath::Clamp(FMath::RoundToInt(delta.Y), -DungeonUnit, DungeonUnit);
		FVector verticalOffset = FVector(0, 0, delta.Z);
		FVector horizontalOffset = FVector(xDir, yDir, 0);

		// Check if in bounds
		if(!grid.InBounds(a.Position + verticalOffset)
			|| !grid.InBounds(a.Position + horizontalOffset)
			|| !grid.InBounds(a.Position + horizontalOffset + verticalOffset))
		{
			return info;
		}

		// Check if the positions are valid for creating stairs
		if(grid[a.Position + horizontalOffset] != EStructureType::NONE
			|| grid[a.Position + horizontalOffset*2] != EStructureType::NONE
			|| grid[a.Position + horizontalOffset + verticalOffset] != EStructureType::NONE
			|| grid[a.Position + horizontalOffset*2 + verticalOffset] != EStructureType::NONE)
		{
			return info;
		}

		info.Traversable = true;
		info.IsStairs = true;
	}
				
	return info;
}

