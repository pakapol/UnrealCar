// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WheeledVehicle.h"
#include <vector>
#include <zmq.hpp>
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MainSceneActor.generated.h"

UCLASS()
class CAR_API AMainSceneActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMainSceneActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

    void* traj_data_gp;
    // std::vector<AWheeledVehicle*> carActors;
    std::vector<AActor*> carActors;
    std::vector<double> Speeds;
    zmq::socket_t* socket_p;
    int t;
};
