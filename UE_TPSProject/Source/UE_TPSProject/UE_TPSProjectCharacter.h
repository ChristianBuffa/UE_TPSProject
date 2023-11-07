// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/Character.h"
#include "UE_TPSProject/HealthComponent.h"
#include "Logging/LogMacros.h"
#include "UE_TPSProjectCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

USTRUCT(BlueprintType)
struct FWeaponSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh *WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int MagCapacity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool IsAutomatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Rate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Range;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HitRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Offset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UParticleSystem* HitEFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USoundBase* SoundEFX;
	
	//FWeaponSlot();
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGameStateCharacter);
DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AUE_TPSProjectCharacter : public ACharacter
{
	GENERATED_BODY()
	
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UChildActorComponent* WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true") )
	UStaticMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	UHealthComponent* HealthComponent;

public:
	AUE_TPSProjectCharacter();

	/** Broadcasted when character land on ground */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterLanding;

	/** Broadcasted when character jump */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterJumping;
	
	/** Broadcasted when character crouching */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterCrouch;
	
	/** Broadcasted when character stop crouching */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterUncrouch;

	/** Broadcasted when character start aiming */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterAim;

	/** Broadcsted when character stop aiming */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterStopAim;

	/** Broadcsted when character reloading the weapon */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterStartReload;

	/** Broadcsted when character trace line from weapon */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterTraceLine;

	/** Broadcsted when character starts sprinting */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterStartSprint;

	/** Broadcsted when character stops sprinting */
	UPROPERTY(BlueprintAssignable)
	FGameStateCharacter OnCharacterEndSprint;
	
	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	/** Walk speed while sprinting */
	UPROPERTY(EditAnywhere, Category = "Sprint")
	float MaxSpeedSprinting = 700.0f;
	
	/** Walk speed while aiming. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float MaxSpeedAiming = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aim")
	float ActualEight = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Timeline")
	UCurveFloat* MovementCurve;

	UPROPERTY(EditAnywhere, Category = "Timeline")
	UCurveVector* OffsetCurve;

	UPROPERTY(EditAnywhere, Category = "Timeline")
	UCurveFloat* CrouchCurve;
	
	/** This array contains all the character weapons*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
	TArray<FWeaponSlot> Arsenal;

private:
	int ActiveWeapon;

	int ActiveThrowable;

	int MagBullets;

	float MaxSpeedWalkingOrig;

	float FireTime;

	float CheckCoverRadius;

	bool bIsAiming;
	
	bool bCanTakeCover = false;
	
	bool bIsInCover = false;

	bool bIsFiring = false;

	bool bIsReloading = false;

	bool bIsUsingArch = false;

	bool bIsUsingWeapon = false;

	bool bCanMove = false;

	bool bIsSprinting = false;
	
	/** Timeline use for aiming: change the visual from 360 to right shoulder*/
	FTimeline AimTimeline;

	/** Timeline used to crouch character*/
	FTimeline CrouchTimeline;

	/** This variable stores the allowed movement while player is covering*/
	FVector CoverDirectionMovement;

	UFUNCTION()
	void HandleProgressArmLength(float Length);
	
	UFUNCTION()
	void HandleProgressCameraOffset(FVector Offset);

	UFUNCTION()
	void HandleProgressCrouch(float Height);

	UFUNCTION()
	void StopCharacter();
	
	void EnablePlayerInput(bool Enabled);

	void EnableMovement(bool Enabled);

	// Mechanic: Fire with weapon
	void FireFromWeapon();
	void AutomaticFire(float DeltaTime);


protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	// Mechanic: Movement and rotation
	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);
	
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

	// Mechanic: Crouch
	void CrouchCharacter();
	void StopCrouchCharacter();

	// Mechanic: Sprint
	void StartSprint();
	void EndSprint();

	// Mechanic: Aim
	void AimInWeapon();
	void AimOutWeapon();
	void AimIn();
	void AimOut();

	// Mechanic: Reload
	void ReloadWeapon();

public:
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnJumped_Implementation();

	/** Called when the player try to fire with weapon */
	void Fire();
	/** Called when the player stop firing with weapon */
	void StopFire();

	UFUNCTION(BlueprintCallable, Category = "Reload")
	void EndReload();

	UFUNCTION(BlueprintCallable, Category = "TPS")
	bool IsAimingWithWeapon();

	UFUNCTION(BlueprintCallable, Category = "TPS")
	bool IsAimingWithArch();

	UFUNCTION(BlueprintCallable, Category = "Reload")
	int MagCounter();

	UFUNCTION(BlueprintCallable, Category = "TPS")
	FWeaponSlot RetrieveActiveWeapon();

	UFUNCTION(BlueprintCallable, Category = "Health")
	FORCEINLINE class UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

