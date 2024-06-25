// Fill out your copyright notice in the Description page of Project Settings.


#include "TestBoss.h"

// Sets default values
ATestBoss::ATestBoss()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ATestBoss::BeginPlay()
{
	Super::BeginPlay();
	
	// Set current health to be equal to max health
	CurrentHealth = MaxHealth;
}

// Called every frame
void ATestBoss::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

