// Copyright Epic Games, Inc. All Rights Reserved.

#include "RaidPrototypeCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

//////////////////////////////////////////////////////////////////////////
// ARaidPrototypeCharacter

ARaidPrototypeCharacter::ARaidPrototypeCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ARaidPrototypeCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	// Set current health to be equal to max health
	CurrentHealth = MaxHealth;

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		// Store reference to player controller
		PlayerControllerRef = PlayerController;

		// Set cursor visibility to true
		PlayerControllerRef->SetShowMouseCursor(true);

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ARaidPrototypeCharacter::Tick(float DeltaTime)
{
	// Check is left click is pressed
	if (bIsLeftClickPressed)
	{
		TimeSinceLeftClick += DeltaTime;
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ARaidPrototypeCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARaidPrototypeCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARaidPrototypeCharacter::Look);

		// Left Clicking
		EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Started, this, &ARaidPrototypeCharacter::LeftClickStarted);
		EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Completed, this, &ARaidPrototypeCharacter::LeftClickCompleted);

		// Zooming
		EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ARaidPrototypeCharacter::CameraZoom);
	}
}

void ARaidPrototypeCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ARaidPrototypeCharacter::Look(const FInputActionValue& Value)
{
	if (bIsLeftClickPressed)
	{
		// input is a Vector2D
		FVector2D LookAxisVector = Value.Get<FVector2D>();

		if (Controller != nullptr)
		{
			// add yaw and pitch input to controller
			AddControllerYawInput(LookAxisVector.X);
			AddControllerPitchInput(LookAxisVector.Y);

			// Reset the cursor position
			PlayerControllerRef->SetMouseLocation(CurrentCursorPos.X, CurrentCursorPos.Y);
		}
	}
}

void ARaidPrototypeCharacter::LeftClickStarted()
{
	// Set is left click pressed to true
	bIsLeftClickPressed = true;

	// Reset the left click timer
	TimeSinceLeftClick = 0.0f;

	// Set cursor visibility to false
	PlayerControllerRef->SetShowMouseCursor(false);

	// Set cursor default position
	PlayerControllerRef->GetMousePosition(CurrentCursorPos.X, CurrentCursorPos.Y);
}

void ARaidPrototypeCharacter::LeftClickCompleted()
{
	// Set is left click pressed to false
	bIsLeftClickPressed = false;

	// Set cursor visibility to true
	PlayerControllerRef->SetShowMouseCursor(true);

	// Check if the left click has been held down for less than the left click timer
	if (TimeSinceLeftClick < LeftClickTimer)
	{
		// Check if the boss has been clicked on
		FHitResult CursorHitResult;
		PlayerControllerRef->GetHitResultUnderCursor(ECollisionChannel::ECC_GameTraceChannel1, false, CursorHitResult);

		// Check the actor of the first blocking hit
		AActor* DetectedActor = CursorHitResult.GetActor();

		// Check if the hit result has reference to the boss
		if (ATestBoss* TestBossRef = Cast<ATestBoss>(DetectedActor))
		{
			// Check if the enemy is already selected
			if (CurrentTarget != TestBossRef)
			{
				// Set current target to test boss ref
				CurrentTarget = TestBossRef;
			}
		}
		else
		{
			// Reset current target
			CurrentTarget = nullptr;
		}
	}
}

void ARaidPrototypeCharacter::CameraZoom(const FInputActionValue& Value)
{
	// Input is a float
	float ZoomValue = -Value.Get<float>();

	// Check that the zoom has a value and the controller reference is valid
	if (ZoomValue == 0.0f || !Controller)
	{
		return;
	}

	// Set the target arm length
	const float NewTargetArmLength = CameraBoom->TargetArmLength + ZoomValue * ZoomStep;
	CameraBoom->TargetArmLength = FMath::Clamp(NewTargetArmLength, MinZoomLength, MaxZoomLength);
}


