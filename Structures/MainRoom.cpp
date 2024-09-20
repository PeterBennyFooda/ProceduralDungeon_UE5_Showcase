// Fill out your copyright notice in the Description page of Project Settings.


#include "MainRoom.h"

#include "NavAreas/NavArea_Default.h"

// Sets default values
AMainRoom::AMainRoom()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Enable replication for this actor
	bReplicates = true;

	// Add components
	//navModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifier"));
	//navModifier->SetAreaClass(UNavArea_Default::StaticClass());
}

// Called when the game starts or when spawned
void AMainRoom::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMainRoom::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

/*
 * @brief Set the exit points of the room for spawning the next room
 * @param TArray<UArrowComponent*> Exit points of the room
 */
void AMainRoom::SetExitPoints(TArray<UArrowComponent*> exits)
{
	if(exits.Num() > 0)
	{
		ExitPoints = exits;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Array is null or empty"));
	}
}

/*
 *	@brief Set the door points of the room for spawning the next room
 *	@param TArray<FVector*> doors
 */
void AMainRoom::SetDoorPints(TArray<FVector> doors)
{
	if(doors.Num() > 0)
	{
		DoorPoints = doors;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Array is null or empty"));
	}
}

/*
 * @brief Add a door point to the room
 * @param FVector door
 */
void AMainRoom::AddDoorPoint(FVector door)
{
	if(!DoorPoints.Contains(door))
	{
		DoorPoints.Add(door);
	}
}

/*
 * @brief Get the exit points of the room for spawning the next room
 * @return TArray<UArrowComponent*> ExitPoints
 */
TArray<UArrowComponent*>& AMainRoom::GetExitPoints()
{
	return ExitPoints;
}

/*
 * @brief Get the door points of the room for spawning the next room
 * @return TArray<FVector*> DoorPoints
 */
TArray<FVector>& AMainRoom::GetDoorPoints()
{
	return DoorPoints;
}

/*
 * @brief Get the wall points of the room for spawning the next room
 * @return TArray<UArrowComponent*> WallPoints
 */
TArray<UArrowComponent*>& AMainRoom::GetWallPoints()
{
	return WallPoints;
}

/*
 * @brief Initialize the room information
 * @param FTransform transform of the room
 * @param FVector size of the room
 */
void AMainRoom::InitInfo(const FTransform& transform, const FVector& scale, const FBox& bounds)
{
	Transform = transform;
	Scale = scale;
	Bounds = bounds;

	if(RoomRoot)
	{
		RoomRoot->SetWorldScale3D(scale);
	}
}

