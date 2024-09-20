// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NavModifierComponent.h"
#include "GameFramework/Actor.h"
#include "MainRoom.generated.h"

UCLASS()
class NETWORKINGPROTOTYPE_API AMainRoom : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMainRoom();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UNavModifierComponent* navModifier;
		
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable)
	void SetExitPoints(TArray<UArrowComponent*> exits);

	UFUNCTION(BlueprintCallable)
	void SetDoorPints(TArray<FVector> doors);

	UFUNCTION(BlueprintCallable)
	void AddDoorPoint(FVector door);
	
	UFUNCTION(BlueprintCallable)
	TArray<UArrowComponent*>& GetExitPoints();

	UFUNCTION(BlueprintCallable)
	TArray<FVector>& GetDoorPoints();

	UFUNCTION(BlueprintCallable)
	TArray<UArrowComponent*>& GetWallPoints();

	UFUNCTION(BlueprintCallable)
	void InitInfo(const FTransform& transform, const FVector& size, const FBox& bounds);
	

	// ====== Properties ======
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TArray<UArrowComponent*> ExitPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TArray<UArrowComponent*> WallPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"))
	TArray<FVector> InnerPaths;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TArray<FVector> DoorPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"))
	USceneComponent* RoomRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"))
	FBox Bounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	FVector Scale;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	bool IsConnectedToHallway = false;
};
