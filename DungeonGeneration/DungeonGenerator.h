// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CompGeom/Delaunay3.h"

#include "Grid3D.h"
#include "NetworkingPrototype/Structures/MainRoom.h"
#include "NetworkingPrototype/Structures/Hallway.h"
#include "NetworkingPrototype/Structures/Stairs.h"
#include "NetworkingPrototype/Structures/PremadeRoom.h"
#include "NavigationSystem.h"
#include "BasicDoor.h"
#include "DungeonPathfinder3D.h"
#include "DungeonGenerator.generated.h"

UENUM(BlueprintType)
enum class EDungenDebugType : uint8
{
	NONE	UMETA(DisplayName="None"),
	ROOM	UMETA(DisplayName="Room"),
	HALLWAY	UMETA(DisplayName="Hallway"),
	STAIRS	UMETA(DisplayName="Stairs"),
	ALL		UMETA(DisplayName="All")
};

UENUM(BlueprintType)
enum class EStructureType : uint8
{
	NONE	UMETA(DisplayName="None"),
	STOP	UMETA(DisplayName="Stop"),
	ROOM	UMETA(DisplayName="Room"),
	GROUND	UMETA(DisplayName="Ground"),
	HALLWAY	UMETA(DisplayName="Hallway"),
	STAIRS	UMETA(DisplayName="Stairs")
};

UCLASS()
class NETWORKINGPROTOTYPE_API ADungeonGenerator : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADungeonGenerator();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Helper function to get random room properties
	void GetRandomRoomProperties(TArray<FVector>& locations, FVector& scale);

	// Helper function to get a random number within an interval
	int GetRandomNumberWithInterval(int min, int max) const;

	// Helper function to check if new room location intersects with existing rooms
	bool ChckeRoomIntersection(const FBox& roomA, const FBox& roomB);

	// Get all integer points in a box
	TArray<FVector> GetAllIntegerPointsInBox(const FBox& Box);

	// Kruskal’s Algorithm to find the minimum spanning tree(MST)
	TArray<FEdge> MinimumSpanningTree(const TArray<FEdge>& edges, const FVector& startVertex);

	// Add random edges to the MST to create a more complex dungeon
	TArray<FEdge> AddRandomEdgesToMST(const TArray<FEdge>& originalEdges, TArray<FEdge>& mstEdges, float additionalEdgeProbability);

	// Generate rooms
	void GenerateProcGenRooms();
	void GeneratePremadeRooms();
	void RoomCountCalculation(const FVector& centerRoomLocation);
		
	// Generate the Optimal path between rooms based on the floor
	void FindPossibleHallwaysNormal();
	void FindPossibleHallwaysFloorBased();
	void GenerateNormalHallways();
	void GenerateFloorBasedHallways();

	// Clean up the dungeon
	void CleanUpDungeon();

	// Update Navmesh
	void UpdateNavMesh(AMainRoom* room);

	// Check if location is occupied
	bool IsLocationOccupied(const FVector& location, const FQuat& rotation, const FVector& Extent) const;

	// Check if current generation step is at ground floor
	bool IsGroundFloor(const FVector& location) const;

	// Cost function
	DungeonPathInfo CostFunction(const DungeonNode& a, const DungeonNode& b, const FVector& endPos); 

	Grid3D<EStructureType> grid;

	// rooms
	TArray<TArray<AMainRoom*>> spawnedRooms;
	TArray<FBox> premadeBounds;

	TMap<int, TArray<AMainRoom*>> floorRoomMap;
	TMap<int, TArray<FVector>> floorVertexMap;
	TMap<int, TArray<FVector>> floorStairVertexMap;
	TMap<int, TArray<FEdge>> floorEdgeMap;
	TMap<AMainRoom*, TArray<AMainRoom*>> cleanupMap;

	// doors
	TMap<FVector, ABasicDoor*> spawnedDoors;
	
	// algorithms
	UE::Geometry::FDelaunay3 delaunay;
	DungeonPathfinder3D pathfinder;
	
	TArray<FVector> roomVertices;
	TArray<FVector> hallwaysVertices;
	TArray<FEdge> selectedEdges;

	// room count
	int currentGroundFloorRoomCount = 0;
	TArray<int> floorRoomCount;
	int currentFloorIndex = 0;
	bool freeGenerationMode = false;

	// Navmesh
	UNavigationSystemV1* navSystem = nullptr;

	// Others
	int currentRoomGroupIndex = 0;
	FVector sizeGap = FVector(0.0f, 0.0f, 0.0f);
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Spawn the a room at the transform location
	UFUNCTION(BlueprintCallable)
	AMainRoom* SpawnStructure(FTransform transform, TSubclassOf<AMainRoom> room, bool checkCollision);

	// Spawn door at the transform location
	UFUNCTION(BlueprintCallable)
	ABasicDoor* SpawnDoor(FTransform transform, TSubclassOf<ABasicDoor> door);

	// Generate the dungeon
	UFUNCTION(BlueprintCallable)
	void GenerateRooms(FTransform startingPoint, int roomSpawnSteps);

	// Triangulate the rooms
	UFUNCTION(BlueprintCallable)
	void Triangulate();

	// Find all possible connections between rooms
	UFUNCTION(BlueprintCallable)
	void FindPossibleHallways();

	// Generate the optimal path between rooms
	UFUNCTION(BlueprintCallable)
	void GenerateHallways();

	// Generate the courtyard
	UFUNCTION(BlueprintCallable)
	void GenerateCourtyard();

	UFUNCTION(BlueprintCallable)
	void GenerateCeilings();

	// Generate the walls 
	UFUNCTION(BlueprintCallable)
	void GenerateWalls();

	UFUNCTION(BlueprintCallable)
	void GenerateDungeon(FTransform startingPoint, int roomCount);

	UFUNCTION(BlueprintCallable)
	FVector GetRandomRoomLocation();

	UFUNCTION(BlueprintCallable)
	int GetCurrentFloorNumber(const FVector& location) const;

	
	// ====== Properties ======
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	int DungeonUnit = 5;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	FVector DungeonSize = FVector(30.0f, 30.0f, 5.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	FVector NormalFloorSize = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	bool IsRoomProcGen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	FVector MaxRoomScale = FVector(2.0f, 2.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	FVector MinRoomScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	FVector DefaultRoomSize = FVector(150.0f, 150.0f, 150.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	float SpawnOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Basic")
	float LoopProbability = 0.125f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true", EditCondition = IsDungeonFloorBased), Category="Advanced")
	bool IsGroundFloorCourtyard = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	int GroundFloorIndex = 2;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	int MinGroundFloorRoomCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	int MinRoomCount = 4;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	int MaxDoorCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	int MaxStairCaseCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	bool IsDungeonFloorBased = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true", EditCondition = IsDungeonFloorBased), Category="Advanced")
	bool ShouldGenerateBuilding = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	float BaseCost = 100.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	float RoomExtraCost = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	float NoneExtraCost = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Advanced")
	float ChangeFloorExtraCost = 200.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Prefab")
	TSubclassOf<AMainRoom> EntranceRoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Prefab")
	TArray<TSubclassOf<AMainRoom>> RoomList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Prefab")
	TArray<TSubclassOf<APremadeRoom>> PremadeRoomList;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Prefab")
	TSubclassOf<AMainRoom> PathTileInPremadeRoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Prefab")
	TArray<TSubclassOf<AMainRoom>> WallList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Prefab")
	TArray<TSubclassOf<ABasicDoor>> DoorList;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Prefab")
	TArray<TSubclassOf<AStairs>> StairsList;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Prefab")
	TArray<TSubclassOf<AHallway>> HallwayList;

	// ====== Debug Properties ======
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Debug")
	bool DebugMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Debug")
	EDungenDebugType DebugType = EDungenDebugType::ALL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"), Category="Debug")
	bool DebugWithModels = false;

	// ====== Networking ======
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TArray<FVector> ReplicatedRoomLocations;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	bool IsGenerated = false;
};
