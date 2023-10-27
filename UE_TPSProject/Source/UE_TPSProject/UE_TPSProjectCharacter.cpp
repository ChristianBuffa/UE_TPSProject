// Fill out your copyright notice in the Description page of Project Settings.

#include "UE_TPSProjectCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"

//////////////////////////////////////////////////////////////////////////
// ATP_ThirdPersonCharacter

AUE_TPSProjectCharacter::AUE_TPSProjectCharacter() {
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	
	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(52.0f);

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	
	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
		
	// Add a mesh for the weapon
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, "hand_rSocket");
	
	//Set other variabled
	ActiveWeapon = 0;
	ActiveThrowable = 0;
	CheckCoverRadius = 60.0f;
	ActualEight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	bIsAiming = false;
}

void AUE_TPSProjectCharacter::BeginPlay() {
	Super::BeginPlay();
	
	bCanMove = true;
	MagBullets = Arsenal[ActiveWeapon].MagCapacity;
	FireTime = 0.0f;
	FVector WeaponLocation = GetMesh()->GetSocketLocation("hand_rSocket");
	FRotator WeaponRotaion = GetMesh()->GetSocketRotation("hand_rSocket");
	
	MaxSpeedWalkingOrig = GetCharacterMovement()->MaxWalkSpeed;
	if (MovementCurve && OffsetCurve) {
		FOnTimelineFloat ProgressFunctionLength;
		ProgressFunctionLength.BindUFunction(this, "HandleProgressArmLength");
		AimTimeline.AddInterpFloat(MovementCurve, ProgressFunctionLength);

		FOnTimelineVector ProgressFunctionOffset;
		ProgressFunctionOffset.BindUFunction(this, "HandleProgressCameraOffset");
		AimTimeline.AddInterpVector(OffsetCurve, ProgressFunctionOffset);
	}

	if (CrouchCurve) {
		FOnTimelineFloat ProgressFunctionCrouch;
		ProgressFunctionCrouch.BindUFunction(this, "HandleProgressCrouch");
		CrouchTimeline.AddInterpFloat(CrouchCurve, ProgressFunctionCrouch);
	}
}

void AUE_TPSProjectCharacter::OnConstruction(const FTransform & Transform) {
	if (Arsenal.Num() > 0) {
		WeaponMesh->SetStaticMesh(Arsenal[0].WeaponMesh);
	}
}


void AUE_TPSProjectCharacter::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	AimTimeline.TickTimeline(DeltaTime);
	CrouchTimeline.TickTimeline(DeltaTime);
	
	AutomaticFire(DeltaTime);
}

//////////////////////////////////////////////////////////////////////////
// Timeline management

void AUE_TPSProjectCharacter::HandleProgressArmLength(float Length) {
	CameraBoom->TargetArmLength = Length;
}

void AUE_TPSProjectCharacter::HandleProgressCameraOffset(FVector Offset) {
	CameraBoom->SocketOffset = Offset;
}

void AUE_TPSProjectCharacter::HandleProgressCrouch(float Height) {
	ActualEight = Height;
}

//////////////////////////////////////////////////////////////////////////
// Input

void AUE_TPSProjectCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) {
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AUE_TPSProjectCharacter::JumpCharacter);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AUE_TPSProjectCharacter::StopJumpingCharacter);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &AUE_TPSProjectCharacter::CrouchCharacter);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &AUE_TPSProjectCharacter::StopCrouchCharacter);
	
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &AUE_TPSProjectCharacter::AimInWeapon);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &AUE_TPSProjectCharacter::AimOutWeapon);

	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AUE_TPSProjectCharacter::Fire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AUE_TPSProjectCharacter::StopFire);
	
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AUE_TPSProjectCharacter::ReloadWeapon);

	PlayerInputComponent->BindAxis("MoveForward", this, &AUE_TPSProjectCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AUE_TPSProjectCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AUE_TPSProjectCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AUE_TPSProjectCharacter::LookUpAtRate);
}

//////////////////////////////////////////////////////////////////////////
// Movement and rotation

void AUE_TPSProjectCharacter::TurnAtRate(float Rate) {
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AUE_TPSProjectCharacter::LookUpAtRate(float Rate) {
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}


void AUE_TPSProjectCharacter::MoveForward(float Value) {
	if ((Controller != NULL) && (Value != 0.0f) & !bIsInCover && bCanMove) {
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AUE_TPSProjectCharacter::MoveRight(float Value) {
	if ((Controller != NULL) && (Value != 0.0f) && bCanMove) {
		if (!bIsInCover) {
			// find out which way is right
			const FRotator Rotation = Controller->GetControlRotation();
			const FRotator YawRotation(0, Rotation.Yaw, 0);

			// get right vector 
			const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
			// add movement in that direction
			AddMovementInput(Direction, Value);
		} 
	}
}

//////////////////////////////////////////////////////////////////////////
// Mechanic: Aim

void AUE_TPSProjectCharacter::AimInWeapon() {
	if (!bIsUsingArch) {
		bIsUsingWeapon = true;
		AimIn();
	}
}

void AUE_TPSProjectCharacter::AimOutWeapon() {
	bIsUsingWeapon = false;
	AimOut();
}

void AUE_TPSProjectCharacter::AimIn() {
	GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Green, TEXT("Aim In"));
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = MaxSpeedAiming;
	bIsAiming = true;
	if (bIsInCover) {
		GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Green, TEXT("Aim from cover"));
		StopCrouchCharacter();
	}
	AimTimeline.Play();
	OnCharacterAim.Broadcast();
}

void AUE_TPSProjectCharacter::AimOut() {
	GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Green, TEXT("Aim Out"));
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = MaxSpeedWalkingOrig;
	bIsAiming = false;
	if (bIsInCover) {
		GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Green, TEXT("Stop aim from cover"));
		CrouchCharacter();
	}
	AimTimeline.Reverse();
	OnCharacterStopAim.Broadcast();
}

//////////////////////////////////////////////////////////////////////////
// Mechanic: Jump

void AUE_TPSProjectCharacter::JumpCharacter() {
	if (CanJump()) {
		Jump();
		GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Green, TEXT("jumping"));
	}
}

void AUE_TPSProjectCharacter::StopJumpingCharacter() {
	StopJumping();
}

void AUE_TPSProjectCharacter::Landed(const FHitResult& Hit) {
	GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Green, TEXT("landed"));
	OnCharacterLanding.Broadcast();
}

/** This is a UE4 function of AActor class*/
void AUE_TPSProjectCharacter::OnJumped_Implementation() { 
	GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Green, TEXT("On jumped"));
	OnCharacterJumping.Broadcast();
}

//////////////////////////////////////////////////////////////////////////
// Mechanic: Crouch

void AUE_TPSProjectCharacter::CrouchCharacter() {
	
	if (CanCrouch()) {
		GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Green, TEXT("Crouch in"));
		Crouch();
		CrouchTimeline.Play();
		OnCharacterCrouch.Broadcast();
	}
}

void AUE_TPSProjectCharacter::StopCrouchCharacter() {
	GEngine->AddOnScreenDebugMessage(-1, 0.2f, FColor::Green, TEXT("Uncrouch"));
	if (GetCharacterMovement()->IsCrouching()) {
		UnCrouch();
		CrouchTimeline.Reverse();
		OnCharacterUncrouch.Broadcast();
	}
}

//////////////////////////////////////////////////////////////////////////
// Mechanic: Fire with weapon

void AUE_TPSProjectCharacter::FireFromWeapon() {
	if(bIsReloading){
		return;
	}
	
	FCollisionQueryParams Params;
	// Ignore the player's pawn
	Params.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

	// The hit result gets populated by the line trace
	FHitResult Hit;

	float WeaponRange = Arsenal[ActiveWeapon].Range;

	FVector Start = FollowCamera->GetComponentLocation();
	FVector End = Start + (FollowCamera->GetComponentRotation().Vector() * WeaponRange);

	if (!bIsAiming) {
		float WeaponOffset = Arsenal[ActiveWeapon].Offset;
		GEngine->AddOnScreenDebugMessage(-1, 5.2f, FColor::Emerald, TEXT("Not aiming!"));
		Start = WeaponMesh->GetComponentLocation() + (WeaponMesh->GetForwardVector() * WeaponOffset);
		End = Start + (WeaponMesh->GetComponentRotation().Vector() * WeaponRange);
	}

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Params);
	OnCharacterTraceLine.Broadcast();

	if (bHit) {
		DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 3.0f);
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Arsenal[ActiveWeapon].HitEFX, Hit.ImpactPoint);
	}

	UGameplayStatics::PlaySound2D(this, Arsenal[ActiveWeapon].SoundEFX, 1.0f, 1.0f, 0);
}

void AUE_TPSProjectCharacter::AutomaticFire(float DeltaTime) {
	if (Arsenal[ActiveWeapon].IsAutomatic && bIsFiring) {
		FireTime += DeltaTime;
		if (FireTime >= Arsenal[ActiveWeapon].Rate) {
			FireTime = 0.0f;
			if (MagBullets > 0) {
				FireFromWeapon();
				MagBullets--;
			} else {
				StopFire();
				ReloadWeapon();
			}
		}
	}
}

void AUE_TPSProjectCharacter::Fire() {
	if (!bIsReloading) {
		bIsFiring = true;
		FireTime = 0.0f;
		if (MagBullets > 0) {
			FireFromWeapon();
			MagBullets--;
		} else {
			StopFire();
			ReloadWeapon();
		}
	}
}


void AUE_TPSProjectCharacter::StopFire() {
	bIsFiring = false;
	FireTime = 0.0f;
}

FWeaponSlot AUE_TPSProjectCharacter::RetrieveActiveWeapon() {
	return Arsenal[ActiveWeapon];
}


//////////////////////////////////////////////////////////////////////////
// Mechanic: Reload

void AUE_TPSProjectCharacter::ReloadWeapon() {
	if(bIsReloading || MagBullets >= Arsenal[ActiveWeapon].MagCapacity){
		return;
	}
	
	if (!bIsUsingArch) {
		GEngine->AddOnScreenDebugMessage(-1, 5.2f, FColor::Red, TEXT("Start Reload!"));
		bIsReloading = true;
		OnCharacterStartReload.Broadcast();
	}
}

void AUE_TPSProjectCharacter::EndReload() {
	GEngine->AddOnScreenDebugMessage(-1, 5.2f, FColor::Orange, TEXT("End Reload!"));
	bIsReloading = false;
	MagBullets = Arsenal[ActiveWeapon].MagCapacity;
}

int AUE_TPSProjectCharacter::MagCounter() {
	return MagBullets;
}

//////////////////////////////////////////////////////////////////////////
// Utilities

void AUE_TPSProjectCharacter::EnablePlayerInput(bool Enabled) {
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

	if (Enabled) {
		EnableInput(PlayerController);
	} else {
		DisableInput(PlayerController);
	}
}

void AUE_TPSProjectCharacter::EnableMovement(bool Enabled) {
	bCanMove = Enabled;
}

bool AUE_TPSProjectCharacter::IsAimingWithWeapon() {
	return bIsAiming && bIsUsingWeapon;
}

bool AUE_TPSProjectCharacter::IsAimingWithArch() {
	return bIsAiming && bIsUsingArch;
}
