// Astral Wilds - scripted playtest bot (development builds only).
//
// Console command `Astral.AutoPlaytest` plays the bonding loop in a real,
// rendered game session by feeding *simulated key/mouse events* through the
// PlayerController - so the Input Mapping Contexts, modifiers, the Mage's
// bindings and the debug HUD are all exercised exactly as with a human:
//   1. turns the camera toward the nearest receptive wild Astral and holds W
//   2. presses E when in reach (Interact -> Resonance Weave)
//   3. holds Left Mouse (Channel), moves the mouse to keep the reticle on the
//      Resonance Point (WeaveAlignment via Mouse2D), taps Space on each pulse
//   4. logs the outcome and quits.
// Screenshots of each stage land in Saved/AutoPlaytest/. It plays "well" by
// reading the same state the HUD shows; it says nothing about game feel.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "AstralMageCharacter.h"
#include "AstralCharacter.h"
#include "AstralWildSpawner.h"
#include "Astral_WildsCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "AstralSpeciesData.h"
#include "AstralResonanceWeaveComponent.h"
#include "AstralLocomotionAnimInstance.h"
#include "AstralWildlifeController.h"
#include "AstralMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogAstralAutoPlaytest, Display, All);

namespace AstralAutoPlaytest
{
	enum class EStage { WaitForPawn, Approach, Settle, Interact, Weave, Result, Done };

	struct FRun
	{
		TWeakObjectPtr<UWorld> World;
		EStage Stage = EStage::WaitForPawn;
		float StageTime = 0.f;
		float TotalTime = 0.f;
		float WeaveTime = 0.f;
		bool bHoldingW = false;
		bool bHoldingLMB = false;
		bool bSpaceDown = false;
		bool bAnsweredThisPulse = false;
		int32 PulsesSeen = 0;
		int32 PartyBefore = 0;
		bool bPulseShotTaken = false;
		bool bMidShotTaken = false;
		TWeakObjectPtr<AAstralCharacter> Target;
		FTSTicker::FDelegateHandle Handle;
	};

	TUniquePtr<FRun> GRun;

	void Key(APlayerController* PC, const FKey& InKey, EInputEvent Event, float Amount = 1.f)
	{
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, Event, Amount));
	}

	void Shot(const TCHAR* Name)
	{
		const FString Path = FPaths::ProjectSavedDir() / TEXT("AutoPlaytest") / FString(Name) + TEXT(".png");
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[AutoPlaytest] screenshot %s"), *Path);
	}

	AAstralCharacter* FindNearestReceptive(UWorld* World, const FVector& From)
	{
		AAstralCharacter* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<AAstralCharacter> It(World); It; ++It)
		{
			if (It->IsReceptive())
			{
				const float D = FVector::DistSquared(From, It->GetActorLocation());
				if (D < BestDistSq)
				{
					BestDistSq = D;
					Best = *It;
				}
			}
		}
		return Best;
	}

	void Enter(FRun& Run, EStage Stage)
	{
		Run.Stage = Stage;
		Run.StageTime = 0.f;
	}

	void Finish(FRun& Run, UWorld* World, const FString& Summary)
	{
		UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[AutoPlaytest] RESULT: %s"), *Summary);
		Enter(Run, EStage::Done);
		if (World && World->GetFirstPlayerController())
		{
			World->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
		}
	}

	bool Tick(float Dt)
	{
		FRun& Run = *GRun;
		UWorld* World = Run.World.Get();
		if (!World || Run.Stage == EStage::Done)
		{
			return false;
		}
		Run.StageTime += Dt;
		Run.TotalTime += Dt;

		APlayerController* PC = World->GetFirstPlayerController();
		AAstralMageCharacter* Mage = PC ? Cast<AAstralMageCharacter>(PC->GetPawn()) : nullptr;
		if (Run.TotalTime > 90.f)
		{
			Finish(Run, World, TEXT("TIMEOUT after 90s"));
			return false;
		}

		switch (Run.Stage)
		{
		case EStage::WaitForPawn:
			if (Mage && Run.StageTime > 2.f)
			{
				Run.PartyBefore = Mage->GetParty().Num();
				Run.Target = FindNearestReceptive(World, Mage->GetActorLocation());
				if (!Run.Target.IsValid())
				{
					Finish(Run, World, TEXT("FAIL - no receptive wild Astral in the level"));
					return false;
				}
				UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[AutoPlaytest] Mage %s at %s; target %s at %s (%.0f cm)"),
					*Mage->GetClass()->GetName(), *Mage->GetActorLocation().ToCompactString(),
					*Run.Target->GetName(), *Run.Target->GetActorLocation().ToCompactString(),
					FVector::Dist(Mage->GetActorLocation(), Run.Target->GetActorLocation()));
				for (TActorIterator<AAstralCharacter> It(World); It; ++It)
				{
					UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[AutoPlaytest]   wild Astral %s at %s, state %s, %.0f cm away"),
						It->SpeciesData ? *It->SpeciesData->SpeciesName.ToString() : TEXT("?"), *It->GetActorLocation().ToCompactString(),
						*UEnum::GetDisplayValueAsText(It->WildState).ToString(), FVector::Dist(Mage->GetActorLocation(), It->GetActorLocation()));
				}
				Shot(TEXT("01_start"));
				Enter(Run, EStage::Approach);
			}
			break;

		case EStage::Approach:
		{
			if (!Mage || !Run.Target.IsValid())
			{
				Finish(Run, World, TEXT("FAIL - lost Mage or target while approaching"));
				return false;
			}
			const FVector ToTarget = Run.Target->GetActorLocation() - Mage->GetActorLocation();
			// Face the camera at the target; W then walks "forward" relative to it.
			// Other Astrals close ahead (an Aggressive and a Territorial one
			// chasing the Mage can box it in) push the heading sideways, so the
			// bot walks round them like a player would instead of into them.
			FVector Heading = ToTarget.GetSafeNormal2D();
			for (TActorIterator<AAstralCharacter> It(World); It; ++It)
			{
				if (*It == Run.Target.Get())
				{
					continue;
				}
				const FVector Away = (Mage->GetActorLocation() - It->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
				const float Dist = Away.Size();
				const FVector Dir = ToTarget.GetSafeNormal2D();
				if (Dist < 300.f && FVector::DotProduct(Dir, -Away) > 0.f)
				{
					const FVector Side = FVector(-Dir.Y, Dir.X, 0.f) * FMath::Sign(FVector::DotProduct(FVector(-Dir.Y, Dir.X, 0.f), Away) + KINDA_SMALL_NUMBER);
					Heading += Side * 1.5f * (300.f - Dist) / 300.f;
				}
			}
			PC->SetControlRotation(FRotator(-15.f, Heading.Rotation().Yaw, 0.f));
			const bool bInReach = Mage->GetInteractableWildAstral() == Run.Target.Get();
			if (ToTarget.Size2D() < 200.f)
			{
				if (Run.bHoldingW) { Key(PC, EKeys::W, IE_Released, 0.f); Run.bHoldingW = false; }
				UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[AutoPlaytest] walked to target in %.1fs (dist %.0f cm, in reach: %s)"),
					Run.StageTime, ToTarget.Size2D(), bInReach ? TEXT("yes") : TEXT("no"));
				Enter(Run, EStage::Settle);
			}
			else if (!Run.bHoldingW)
			{
				Key(PC, EKeys::W, IE_Pressed);
				Run.bHoldingW = true;
			}
			if (Run.StageTime > 20.f)
			{
				Finish(Run, World, TEXT("FAIL - could not reach the target in 20s (W/Move input not moving the Mage?)"));
				return false;
			}
			break;
		}

		case EStage::Settle:
			if (Run.StageTime > 0.6f && Run.StageTime - Dt <= 0.6f)
			{
				Shot(TEXT("02_prompt"));
			}
			if (Run.StageTime > 0.9f)
			{
				Key(PC, EKeys::E, IE_Pressed);
				Enter(Run, EStage::Interact);
			}
			break;

		case EStage::Interact:
			if (Run.StageTime > 0.05f && Run.StageTime - Dt <= 0.05f)
			{
				Key(PC, EKeys::E, IE_Released, 0.f);
			}
			if (Mage && Mage->IsWeavingResonance())
			{
				UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[AutoPlaytest] E began a Resonance Weave"));
				Key(PC, EKeys::LeftMouseButton, IE_Pressed);
				Run.bHoldingLMB = true;
				Enter(Run, EStage::Weave);
			}
			else if (Run.StageTime > 1.f)
			{
				Finish(Run, World, TEXT("FAIL - pressing E did not begin a weave"));
				return false;
			}
			break;

		case EStage::Weave:
		{
			UAstralResonanceWeaveComponent* Weave = Mage ? Mage->GetResonanceWeave() : nullptr;
			if (!Weave || !Weave->IsWeaveActive())
			{
				Enter(Run, EStage::Result);
				break;
			}
			if (FMath::FloorToInt(Run.WeaveTime * 4.f) != FMath::FloorToInt((Run.WeaveTime + Dt) * 4.f))
			{
				UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[AutoPlaytest]   t=%.2f point=%s reticle=%s stability=%.0f%% channeling=%d pulse=%d"),
					Run.WeaveTime, *Weave->GetResonancePoint().ToString(), *Weave->GetAlignmentReticle().ToString(),
					Weave->GetStabilityFraction() * 100.f, Weave->IsChanneling(), Weave->IsAwaitingPulseResponse());
			}
			Run.WeaveTime += Dt;
			if (Run.StageTime > 0.3f && Run.StageTime - Dt <= 0.3f)
			{
				Shot(TEXT("03_weave"));
			}

			// Track: move the mouse so the reticle heads for the Resonance Point.
			// IMC_ResonanceWeave scales Mouse2D by 0.005, so 1 unit of reticle = 200 counts.
			const FVector2D Error = Weave->GetResonancePoint() - Weave->GetAlignmentReticle();
			const FVector2D Counts = (Error * 200.f).ClampAxes(-40.f, 40.f);
			Key(PC, EKeys::MouseX, IE_Axis, Counts.X);
			Key(PC, EKeys::MouseY, IE_Axis, Counts.Y);

			// Harmonize: tap Space once per pulse, a beat after it fires.
			if (Run.bSpaceDown)
			{
				Key(PC, EKeys::SpaceBar, IE_Released, 0.f);
				Run.bSpaceDown = false;
			}
			if (Weave->IsAwaitingPulseResponse())
			{
				if (!Run.bPulseShotTaken)
				{
					Shot(TEXT("04_pulse"));
					Run.bPulseShotTaken = true;
				}
				else if (!Run.bAnsweredThisPulse && Weave->GetPulseWindowFraction() < 0.6f)
				{
					Key(PC, EKeys::SpaceBar, IE_Pressed);
					Run.bSpaceDown = true;
					Run.bAnsweredThisPulse = true;
					++Run.PulsesSeen;
				}
			}
			else
			{
				Run.bAnsweredThisPulse = false;
			}

			if (!Run.bMidShotTaken && Weave->GetStabilityFraction() > 0.5f)
			{
				Shot(TEXT("05_channeling"));
				Run.bMidShotTaken = true;
			}
			if (Run.StageTime > 60.f)
			{
				Finish(Run, World, FString::Printf(TEXT("FAIL - weave still running after 60s (stability %.0f%%)"), Weave->GetStabilityFraction() * 100.f));
				return false;
			}
			break;
		}

		case EStage::Result:
			if (Run.bHoldingLMB)
			{
				Key(PC, EKeys::LeftMouseButton, IE_Released, 0.f);
				Run.bHoldingLMB = false;
			}
			if (Run.StageTime > 0.4f && Run.StageTime - Dt <= 0.4f)
			{
				Shot(TEXT("06_result"));
			}
			if (Run.StageTime > 1.5f)
			{
				const int32 PartyAfter = Mage ? Mage->GetParty().Num() : -1;
				const bool bBonded = PartyAfter == Run.PartyBefore + 1;
				Finish(Run, World, FString::Printf(TEXT("%s - weave lasted %.1fs, %d pulse(s) answered, party %d -> %d, target %s"),
					bBonded ? TEXT("PASS (bonded)") : TEXT("weave ended without a bond"),
					Run.WeaveTime, Run.PulsesSeen, Run.PartyBefore, PartyAfter,
					Run.Target.IsValid() ? TEXT("still in world") : TEXT("removed from world")));
				return false;
			}
			break;

		default:
			return false;
		}
		return true;
	}

	FAutoConsoleCommandWithWorld GCommand(
		TEXT("Astral.AutoPlaytest"),
		TEXT("Scripted bot: walks to the nearest receptive wild Astral, bonds with it via simulated input, screenshots each stage to Saved/AutoPlaytest, then quits."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (GRun.IsValid() && GRun->Stage != EStage::Done)
			{
				UE_LOG(LogAstralAutoPlaytest, Warning, TEXT("[AutoPlaytest] already running"));
				return;
			}
			GRun = MakeUnique<FRun>();
			GRun->World = World;
			GRun->Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
			UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[AutoPlaytest] started in %s"), *GetNameSafe(World));
		}));
	// Astral.LineupTest: spawns one of each species the level's spawner knows,
	// in a row along +X, all facing +X (actor yaw 0) with AI removed, and
	// views them from the side (camera looking along +Y, so +X is screen
	// LEFT). A correctly oriented model's head points left. Screenshot goes
	// to Saved/AutoPlaytest/lineup.png, then the game quits.
	FAutoConsoleCommandWithWorld GLineupCommand(
		TEXT("Astral.LineupTest"),
		TEXT("Spawns every species facing +X for a side-view orientation screenshot, then quits."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			TArray<UAstralSpeciesData*> Species;
			for (TActorIterator<AAstralWildSpawner> It(World); It; ++It)
			{
				for (UAstralSpeciesData* S : It->PossibleSpecies)
				{
					if (S && !Species.Contains(S))
					{
						Species.Add(S);
					}
				}
				It->SpawnCount = 0; // keep the wild ones out of the shot
			}
			for (TActorIterator<AAstralCharacter> It(World); It; ++It)
			{
				It->SetActorHiddenInGame(true);
			}
			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Mage = PC ? PC->GetPawn() : nullptr;
			if (!Mage)
			{
				return;
			}
			// One close-up per species: camera 250cm from the slot, looking +Y,
			// so +X (the actor's facing) is screen LEFT.
			const FVector Origin = Mage->GetActorLocation();
			const FVector Slot = Origin + FVector(0.f, 150.f, 0.f);
			Mage->SetActorLocation(Origin + FVector(0.f, -100.f, 0.f));
			Mage->SetActorHiddenInGame(true);
			if (AAstral_WildsCharacter* Char = Cast<AAstral_WildsCharacter>(Mage))
			{
				Char->GetCameraBoom()->TargetArmLength = 150.f;
			}
			PC->SetControlRotation(FRotator(-5.f, 90.f, 0.f));
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World), Species, Slot, Index = -1, T = 0.f, Current = TWeakObjectPtr<AAstralCharacter>()](float Dt) mutable
			{
				UWorld* W = WeakWorld.Get();
				if (!W)
				{
					return false;
				}
				T += Dt;
				if (T < (Index < 0 ? 6.f : 3.5f))
				{
					return true; // let the previous shot land / textures stream
				}
				T = 0.f;
				if (Current.IsValid())
				{
					Current->Destroy();
				}
				if (++Index >= Species.Num())
				{
					if (APlayerController* P = W->GetFirstPlayerController())
					{
						P->ConsoleCommand(TEXT("quit"));
					}
					return false;
				}
				const FTransform Xf(FRotator::ZeroRotator, Slot);
				AAstralCharacter* A = W->SpawnActorDeferred<AAstralCharacter>(AAstralCharacter::StaticClass(), Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				A->SpeciesData = Species[Index];
				A->AutoPossessAI = EAutoPossessAI::Disabled;
				A->FinishSpawning(Xf);
				Current = A;
				const FString Name = FString::Printf(TEXT("lineup_%s"), *Species[Index]->SpeciesName.ToString());
				// Delay the shot a moment so the new mesh is on screen.
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Name](float) { Shot(*Name); return false; }), 2.5f);
				return true;
			}));
		}));

	// Astral.MotionCapture [Species] [Seconds] [Archetype|Alone|Circuit]: for judging locomotion by eye.
	// Spawns one wild Astral of the species (default Mossling) next to the
	// Mage with its AI running, hides everything else, follows it with a
	// side camera that keeps a fixed world direction (so turns show), and
	// screenshots every other frame to Saved/AutoPlaytest/motion/NNN.png
	// while logging speed, yaw and blend weights per frame. Run with
	// "-benchmark -fps=30" for a fixed 30Hz step (15 shots per second).
	FAutoConsoleCommandWithWorldAndArgs GMotionCaptureCommand(
		TEXT("Astral.MotionCapture"),
		TEXT("Films one wandering Astral as a frame sequence for animation review, then quits."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FString Want = Args.Num() > 0 ? Args[0] : TEXT("Mossling");
			const float Seconds = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 8.f;
			UAstralSpeciesData* Species = nullptr;
			for (TActorIterator<AAstralWildSpawner> It(World); It; ++It)
			{
				for (UAstralSpeciesData* S : It->PossibleSpecies)
				{
					if (S && S->SpeciesName.ToString() == Want)
					{
						Species = S;
					}
				}
				It->SpawnCount = 0;
			}
			for (TActorIterator<AAstralCharacter> It(World); It; ++It)
			{
				It->Destroy();
			}
			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Mage = PC ? PC->GetPawn() : nullptr;
			if (!Species || !Mage)
			{
				UE_LOG(LogAstralAutoPlaytest, Error, TEXT("[MotionCapture] no species '%s' or no player"), *Want);
				return;
			}
			// Optional third arg: an AI archetype (e.g. Docile) overrides the
			// species' on a transient copy, or "Alone" parks the (hidden) Mage
			// 50m away, frozen, so the Astral does what it does with nobody
			// around (e.g. a Territorial patrol rather than a chase).
			// "Circuit" turns the AI off and runs the Astral at flee speed round
			// a course with 90 and 135 deg corners, for judging turns at speed.
			// "CircuitPivot" is the same course with the arc turning off (the old pivot turns), for comparison.
			// "Ramps" walks it down the central block's ramp to the ground and back up
			// (foot IK on slopes), at a walk.
			const bool bRamps = Args.Num() > 2 && Args[2] == TEXT("Ramps");
			const bool bPivot = Args.Num() > 2 && Args[2] == TEXT("CircuitPivot");
			const bool bCircuit = (Args.Num() > 2 && Args[2] == TEXT("Circuit")) || bPivot || bRamps;
			const bool bAlone = (Args.Num() > 2 && Args[2] == TEXT("Alone")) || bCircuit;
			// Optional fourth arg "Face": a close camera in front of the head
			// (eyes, blinks, gaze) instead of the side view.
			const bool bFace = Args.Num() > 3 && Args[3] == TEXT("Face");
			if (Args.Num() > 2 && !bAlone)
			{
				const int64 Arch = StaticEnum<EAstralAIArchetype>()->GetValueByNameString(Args[2]);
				if (Arch == INDEX_NONE)
				{
					UE_LOG(LogAstralAutoPlaytest, Error, TEXT("[MotionCapture] unknown archetype '%s'"), *Args[2]);
					return;
				}
				Species = DuplicateObject<UAstralSpeciesData>(Species, GetTransientPackage());
				Species->AIArchetype = static_cast<EAstralAIArchetype>(Arch);
			}
			Mage->SetActorHiddenInGame(true);
			const FTransform Xf(FRotator(0.f, 90.f, 0.f), Mage->GetActorLocation() + FVector(0.f, 250.f, 0.f));
			AAstralCharacter* A = World->SpawnActorDeferred<AAstralCharacter>(AAstralCharacter::StaticClass(), Xf, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
			A->SpeciesData = Species;
			A->FinishSpawning(Xf);
			if (bAlone)
			{
				if (ACharacter* MageChar = Cast<ACharacter>(Mage))
				{
					MageChar->GetCharacterMovement()->DisableMovement();
				}
				Mage->SetActorLocation(Mage->GetActorLocation() + FVector(5000.f, 0.f, 0.f));
			}
			if (bCircuit)
			{
				if (AAstralWildlifeController* AI = Cast<AAstralWildlifeController>(A->GetController()))
				{
					AI->bUseNativeBehavior = false;
					AI->StopMovement();
				}
				A->GetCharacterMovement()->MaxWalkSpeed = bRamps ? 140.f : 450.f;
				if (bPivot)
				{
					if (UAstralMovementComponent* Move = Cast<UAstralMovementComponent>(A->GetCharacterMovement()))
					{
						Move->MaxSteerAngle = 180.f;
						Move->MaxTurnAcceleration = 1.e7f;
					}
				}
			}
			ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
			PC->SetViewTarget(Cam);
			if (bFace)
			{
				// Zoomed in: at 90 deg the face was ~60px across, too small to judge lids.
				Cam->GetCameraComponent()->SetFieldOfView(30.f);
			}

			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World), Weak = TWeakObjectPtr<AAstralCharacter>(A), WeakCam = TWeakObjectPtr<ACameraActor>(Cam), Seconds, T = 0.f, Frame = 0, Shots = 0, LastYaw = 0.f, CamYaw = 90.f, bCircuit, bRamps, bFace, Origin = Xf.GetLocation(), Waypoint = -1](float Dt) mutable
			{
				UWorld* W = WeakWorld.Get();
				AAstralCharacter* Astral = Weak.Get();
				ACameraActor* C = WeakCam.Get();
				if (!W || !Astral || !C)
				{
					return false;
				}
				T += Dt;
				// Side view: camera off the Astral's right flank, following its
				// heading slowly so turns still show as the body swinging round.
				const FVector At = Astral->GetActorLocation();
				CamYaw += FMath::FindDeltaAngleDegrees(CamYaw, Astral->GetActorRotation().Yaw) * FMath::Min(1.f, Dt * 0.8f);
				const FRotator Look(-8.f, CamYaw - 90.f, 0.f);
				C->SetActorLocationAndRotation(At - Look.Vector() * 340.f, Look);
				if (bFace && Astral->GetMesh()->GetBoneIndex(TEXT("head")) != INDEX_NONE)
				{
					const FVector HeadAt = Astral->GetMesh()->GetBoneLocation(TEXT("head"));
					const FRotator FaceLook(-5.f, CamYaw + 180.f, 0.f);
					// Back off for big Astrals (Ironbur's head filled the frame at 150).
					const float FaceDist = FMath::Max(150.f, Astral->GetMesh()->Bounds.SphereRadius * 1.2f);
					C->SetActorLocationAndRotation(HeadAt - FaceLook.Vector() * FaceDist, FaceLook);
				}
				if (bCircuit)
				{
					// A loop on the flat ground ring round Lvl_ThirdPerson's central block
					// (11.5m out from the spawn on it): four 90 deg corners, then a
					// hairpin back along the last side. Head for the next point 2.5m
					// early, so the path's braking zone never kicks in.
					static const FVector Course[] = { { 1150.f, -1150.f, 0.f }, { 1150.f, 1150.f, 0.f }, { -1150.f, 1150.f, 0.f }, { -1150.f, -1150.f, 0.f }, { 0.f, -1150.f, 0.f }, { -1150.f, -1150.f, 0.f } };
					static const FVector RampCourse[] = { { 0.f, -1650.f, 0.f }, { 0.f, -300.f, 0.f } };
					// Snapped onto the navmesh.
					auto Corner = [&](int32 I)
					{
						const FVector C = bRamps ? RampCourse[I % UE_ARRAY_COUNT(RampCourse)] : Course[I % UE_ARRAY_COUNT(Course)];
						FVector P = Origin + C;
						FNavLocation Nav;
						UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(W);
						if (NavSys && NavSys->ProjectPointToNavigation(P, Nav, FVector(300.f, 300.f, 500.f)))
						{
							P = Nav.Location;
						}
						return P;
					};
					const FVector Goal = Corner(FMath::Max(Waypoint, 0));
					AAIController* AI = Cast<AAIController>(Astral->GetController());
					const bool bArrived = Waypoint < 0 || FVector::Dist2D(At, Goal) < 250.f;
					if (AI && (bArrived || AI->GetMoveStatus() == EPathFollowingStatus::Idle))
					{
						Waypoint += bArrived ? 1 : 0;
						const EPathFollowingRequestResult::Type R = AI->MoveToLocation(Corner(Waypoint), 50.f);
						UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[MotionCapture] circuit waypoint %d at %s (origin %s): move %s"), Waypoint, *Corner(Waypoint).ToString(), *Origin.ToString(), R == EPathFollowingRequestResult::Failed ? TEXT("FAILED") : TEXT("ok"));
					}
				}
				if (T < 1.5f)
				{
					return true;   // settle: mesh streams in, AI picks a goal
				}
				const float Yaw = Astral->GetActorRotation().Yaw;
				const UAstralLocomotionAnimInstance* Anim = Cast<UAstralLocomotionAnimInstance>(Astral->GetMesh()->GetAnimInstance());
				UE_LOG(LogAstralAutoPlaytest, Display, TEXT("[MotionCapture] f=%d t=%.3f speed=%.1f yaw=%.1f yawrate=%.1f vyaw=%.1f move=%.2f run=%.2f fly=%.2f flap=%.2f brake=%.2f wingz=%.1f vz=%.0f drop=%.1f foot=%.1f tail=%.1f look=%.0f/%.0f blink=%.2f"),
					Frame, T, Astral->GetVelocity().Size2D(), Yaw, Dt > 0.f ? FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / Dt : 0.f, Astral->GetVelocity().Rotation().Yaw,
					Anim ? Anim->GetMoveAlpha() : -1.f, Anim ? Anim->GetRunAlpha() : -1.f, Anim ? Anim->GetFlightAlpha() : -1.f, Anim ? Anim->GetFlapAlpha() : -1.f, Anim ? Anim->GetBrakeAlpha() : -1.f,
					Astral->GetMesh()->GetBoneIndex(TEXT("wing_l_hand")) != INDEX_NONE ? (Astral->GetMesh()->GetBoneLocation(TEXT("wing_l_hand"), EBoneSpaces::ComponentSpace).Z) : -999.f, Astral->GetVelocity().Z,
					Anim ? Anim->GetPelvisDrop() : 0.f, Anim ? Anim->GetMaxFootOffset() : 0.f, Anim ? Anim->GetTailYaw() : 0.f, Anim ? Anim->GetLookYaw() : 0.f, Anim ? Anim->GetLookPitch() : 0.f, Anim ? Anim->GetBlink() : 0.f);
				LastYaw = Yaw;
				if (Frame++ % 2 == 0)
				{
					Shot(*FString::Printf(TEXT("motion/%03d"), Shots++));
				}
				if (T > 1.5f + Seconds)
				{
					if (APlayerController* P = W->GetFirstPlayerController())
					{
						P->ConsoleCommand(TEXT("quit"));
					}
					return false;
				}
				return true;
			}));
		}));
}


#endif // !UE_BUILD_SHIPPING
