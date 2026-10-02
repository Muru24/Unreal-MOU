// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/InGameMenuWidget.h"
#include "UI/SettingsMenuWidget.h"
#include "TeamProject_MOUPlayerController.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Animation/WidgetAnimation.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

void UInGameMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (!bEventsBound)
	{
		if (Button_Resume)
		{
			Button_Resume->OnClicked.AddDynamic(this, &UInGameMenuWidget::OnResumeClicked);
		}
		if (Button_Settings)
		{
			Button_Settings->OnClicked.AddDynamic(this, &UInGameMenuWidget::OnSettingsClicked);
		}
		if (Button_ReturnToLobby)
		{
			Button_ReturnToLobby->OnClicked.AddDynamic(this, &UInGameMenuWidget::OnReturnToLobbyClicked);
		}
		if (Button_QuitDesktop)
		{
			Button_QuitDesktop->OnClicked.AddDynamic(this, &UInGameMenuWidget::OnQuitDesktopClicked);
		}

		if (SettingsMenuWidget)
		{
			SettingsMenuWidget->OnSettingsMenuClosed.AddDynamic(this, &UInGameMenuWidget::OnSettingsClosed);
		}

		bEventsBound = true;
	}

	StopAllAnimations();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettingsHideTimerHandle);
	}

	if (Panel_MainMenu)
	{
		Panel_MainMenu->SetVisibility(ESlateVisibility::Visible);
	}

	if (SettingsMenuWidget)
	{
		SettingsMenuWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
		SettingsMenuWidget->SetRenderTranslation(FVector2D::ZeroVector);
		SettingsMenuWidget->SetRenderOpacity(1.0f);
		SettingsMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	bIsShowingSettings = false;
	if (WidgetSwitcher_Menu)
	{
		WidgetSwitcher_Menu->SetActiveWidgetIndex(0);
	}

	if (Anim_MenuSlideIn)
	{
		PlayAnimation(Anim_MenuSlideIn);
	}

	BP_OnMenuOpen();
}

void UInGameMenuWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettingsHideTimerHandle);
	}

	StopAllAnimations();
	bIsShowingSettings = false;

	Super::NativeDestruct();
}

FReply UInGameMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleBackOrEscape();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UInGameMenuWidget::ResumeGame()
{
	// 블루프린트 닫기 이벤트 통지
	BP_OnMenuClose();

	// 슬라이드 아웃 애니메이션이 유효하면 재생 후 닫기 (최대 0.5초 대기 안전장치)
	if (Anim_MenuSlideOut && Anim_MenuSlideOut->GetEndTime() > 0.05f)
	{
		PlayAnimation(Anim_MenuSlideOut);
		const float AnimLength = FMath::Min(Anim_MenuSlideOut->GetEndTime(), 0.5f);
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UInGameMenuWidget::FinishCloseMenu, AnimLength, false);
	}
	else
	{
		FinishCloseMenu();
	}
}

void UInGameMenuWidget::FinishCloseMenu()
{
	if (ATeamProject_MOUPlayerController* MOU_PC = Cast<ATeamProject_MOUPlayerController>(GetOwningPlayer()))
	{
		MOU_PC->CloseInGameMenu();
	}
}

void UInGameMenuWidget::OpenSettings()
{
	bIsShowingSettings = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettingsHideTimerHandle);
	}

	StopAllAnimations();

	if (Panel_MainMenu)
	{
		Panel_MainMenu->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (WidgetSwitcher_Menu)
	{
		WidgetSwitcher_Menu->SetActiveWidgetIndex(1);
	}

	if (SettingsMenuWidget)
	{
		SettingsMenuWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
		SettingsMenuWidget->SetRenderTranslation(FVector2D::ZeroVector);
		SettingsMenuWidget->SetRenderOpacity(1.0f);
		SettingsMenuWidget->SetVisibility(ESlateVisibility::Visible);
	}

	if (Anim_SettingsSlideIn)
	{
		PlayAnimation(Anim_SettingsSlideIn);
	}

	BP_OnSettingsOpen();
}

void UInGameMenuWidget::OnSettingsClosed()
{
	bIsShowingSettings = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettingsHideTimerHandle);
	}

	StopAllAnimations();

	if (Anim_SettingsSlideOut && Anim_SettingsSlideOut->GetEndTime() > 0.05f)
	{
		PlayAnimation(Anim_SettingsSlideOut);
		const float AnimLength = FMath::Min(Anim_SettingsSlideOut->GetEndTime(), 0.5f);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(SettingsHideTimerHandle, [this]()
			{
				if (!bIsShowingSettings)
				{
					if (SettingsMenuWidget)
					{
						SettingsMenuWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
						SettingsMenuWidget->SetRenderTranslation(FVector2D::ZeroVector);
						SettingsMenuWidget->SetRenderOpacity(1.0f);
						SettingsMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
					}
					if (WidgetSwitcher_Menu)
					{
						WidgetSwitcher_Menu->SetActiveWidgetIndex(0);
					}
					if (Panel_MainMenu)
					{
						Panel_MainMenu->SetVisibility(ESlateVisibility::Visible);
					}
				}
			}, AnimLength, false);
		}
		else
		{
			if (SettingsMenuWidget)
			{
				SettingsMenuWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
				SettingsMenuWidget->SetRenderTranslation(FVector2D::ZeroVector);
				SettingsMenuWidget->SetRenderOpacity(1.0f);
				SettingsMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
			if (WidgetSwitcher_Menu)
			{
				WidgetSwitcher_Menu->SetActiveWidgetIndex(0);
			}
			if (Panel_MainMenu)
			{
				Panel_MainMenu->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
	else
	{
		if (SettingsMenuWidget)
		{
			SettingsMenuWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
			SettingsMenuWidget->SetRenderTranslation(FVector2D::ZeroVector);
			SettingsMenuWidget->SetRenderOpacity(1.0f);
			SettingsMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (WidgetSwitcher_Menu)
		{
			WidgetSwitcher_Menu->SetActiveWidgetIndex(0);
		}
		if (Panel_MainMenu)
		{
			Panel_MainMenu->SetVisibility(ESlateVisibility::Visible);
		}
	}

	BP_OnSettingsClose();
}

void UInGameMenuWidget::ReturnToLobby()
{
	if (ATeamProject_MOUPlayerController* MOU_PC = Cast<ATeamProject_MOUPlayerController>(GetOwningPlayer()))
	{
		MOU_PC->ReturnToLobby();
	}
}

void UInGameMenuWidget::QuitToDesktop()
{
	if (ATeamProject_MOUPlayerController* MOU_PC = Cast<ATeamProject_MOUPlayerController>(GetOwningPlayer()))
	{
		MOU_PC->QuitToDesktop();
	}
}

bool UInGameMenuWidget::HandleBackOrEscape()
{
	if (bIsShowingSettings)
	{
		OnSettingsClosed();
		return true;
	}

	ResumeGame();
	return true;
}

void UInGameMenuWidget::OnResumeClicked()
{
	ResumeGame();
}

void UInGameMenuWidget::OnSettingsClicked()
{
	if (bIsShowingSettings)
	{
		OnSettingsClosed();
	}
	else
	{
		OpenSettings();
	}
}

void UInGameMenuWidget::OnReturnToLobbyClicked()
{
	ReturnToLobby();
}

void UInGameMenuWidget::OnQuitDesktopClicked()
{
	QuitToDesktop();
}
