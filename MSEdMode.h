#pragma once


#include "Editor.h"
#include "EditorModeRegistry.h"
#include "EdMode.h"
#include "Toolkits/BaseToolkit.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#include "SceneView.h"
#include "SceneManagement.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "UnrealEd.h"
#include "Toolkits/ToolkitManager.h"

#include "SlateBasics.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/SPanel.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "EditorViewportClient.h"

#include "PropertyEditorModule.h"
#include "LevelEditor.h"

#include "IDetailsView.h"

#include "PropertyCustomizationHelpers.h"

/*
#Note:
that editor mode should be registered in module's startup like the following:
	FEditorModeRegistry::Get().RegisterMode<FMultiSelectionEdMode>("FMultiSelectionEdMode", LOCTEXT("FMultiSelectionEdMode", "FMultiSelectionEdMode"), FSlateIcon(), true);
and also unregister in module's shutdown:
	FEditorModeRegistry::Get().UnregisterMode("FMultiSelectionEdMode");

*/
//////////////////////////////////////////////////////////////////////////
class FMultiSelectionToolkit : public FModeToolkit 
{

public:
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost) override
	{
		TSharedPtr<SClassPropertyEntryBox> classWidget =
			SNew(SClassPropertyEntryBox).ShowTreeView(true).AllowNone(false).MetaClass(AActor::StaticClass())
			.SelectedClass(this, &FMultiSelectionToolkit::GetSelectedClass)
			.OnSetClass(FOnSetClass::CreateSP(this, &FMultiSelectionToolkit::SetSelectedClass));
		
		Content = SNew(SBorder).HAlign(HAlign_Left).VAlign(VAlign_Top)[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox) 
					+ SHorizontalBox::Slot().HAlign(HAlign_Left)[
						SNew(STextBlock).Text(FText::FromString(FString("super class: ")))
					]
					+ SHorizontalBox::Slot().HAlign(HAlign_Right)[
						classWidget.ToSharedRef()
					]
				]
			+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(FString(TEXT("hold and release V to select multiple Actor"))))
				]
		];

		FModeToolkit::Init(InitToolkitHost);
	}




	virtual TSharedPtr<SWidget> GetInlineContent() const override
	{
		return Content;
	}
	virtual FText GetBaseToolkitName() const override
	{
		return FText::FromString(FString("Multi Selection"));
	}
	virtual FEdMode* GetEditorMode() const override
	{
		return GLevelEditorModeTools().GetActiveMode("FMultiSelectionEdMode");
	}
	virtual FName GetToolkitFName() const override
	{
		return FName("Multi Selection");
	}

	TSharedPtr<SWidget>	Content;

	void SetSelectedClass(const UClass* cls) { SelectedClass = cls; }
	const UClass* GetSelectedClass() const { return SelectedClass; }

	const UClass*	SelectedClass = AActor::StaticClass();


};


//////////////////////////////////////////////////////////////////////////
class FMultiSelectionEdMode : public FEdMode
{
public:
	FIntPoint	SelectionStart;
	FIntPoint	SelectionEnd;

	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override
	{
		FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);

		if (Viewport->KeyState(EKeys::V))
		{
			FIntPoint min = SelectionStart.ComponentMin(SelectionEnd);
			FIntPoint max = SelectionStart.ComponentMax(SelectionEnd);

			Canvas->DrawTile(min.X, min.Y, max.X - min.X, max.Y - min.Y, 0, 0, 1, 1, FLinearColor(1, 1, 0.9, 0.5));
		}
	}
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override
	{
		ViewportClient->Invalidate();

		return FEdMode::MouseMove(ViewportClient, Viewport, x, y);
	}
	virtual bool CapturedMouseMove(FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY) override
	{
		return FEdMode::CapturedMouseMove(InViewportClient, InViewport, InMouseX, InMouseY);
	}
	FMultiSelectionToolkit* GetToolkit() const
	{
		return (FMultiSelectionToolkit*)(Toolkit.Get());
	}
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override
	{
		if (Key == EKeys::V)
		{
			Viewport->GetMousePos(SelectionEnd);

			if (Event == IE_Pressed)
			{
				Viewport->GetMousePos(SelectionStart);
			}
			if (Event == IE_Released)
			{
				
				//the min max of selection rect on viewport
				FIntPoint min = SelectionStart.ComponentMin(SelectionEnd);
				FIntPoint max = SelectionStart.ComponentMax(SelectionEnd);

				TArray<AActor*>	actorsExtractedFromHitProxy;
				TArray<HHitProxy*> selectionHits;
				FIntPoint selectionSize = max - min;

				auto LAddHitActor = [&](HHitProxy* hitProxy) {
					if (auto hActor = HitProxyCast<HActor>(hitProxy))
					{
						if (this->GetToolkit()->SelectedClass)
						{
							if (hActor->Actor && hActor->Actor->IsA(this->GetToolkit()->SelectedClass))
								actorsExtractedFromHitProxy.AddUnique(hActor->Actor);
						}
						else
						{
							actorsExtractedFromHitProxy.AddUnique(hActor->Actor);
						}
					}
				};


				//#Note 	Viewport->GetHitProxy(X, Y) had so much overhead I use the following one instead
				Viewport->GetHitProxyMap(FIntRect(min, max), selectionHits);

				for (HHitProxy* hitProxy : selectionHits)
				{
					LAddHitActor(hitProxy);
				}

				if (!IsShiftDown(Viewport))
				{
					GUnrealEd->SelectNone(true, true);
				}
				for(AActor* actor : actorsExtractedFromHitProxy)
				{
					GUnrealEd->SelectActor(actor, !actor->IsSelected(), false);
				}
			}
		}
		return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
	}
	virtual bool InputAxis(FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime) override
	{
		return FEdMode::InputAxis(InViewportClient, Viewport, ControllerId, Key, Delta, DeltaTime);
	}
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override
	{
		return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
	}

	virtual void Enter() override
	{
		FEdMode::Enter();

		if (!Toolkit.IsValid() && UsesToolkits())
		{
			Toolkit = MakeShareable(new FMultiSelectionToolkit);
			Toolkit->Init(Owner->GetToolkitHost());
		}
	}
	virtual void Exit() override
	{
		if (Toolkit.IsValid())
		{
			if (Toolkit.IsValid())
			{
				FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
				Toolkit = nullptr;
			}
		}

		FEdMode::Exit();
	}
	bool UsesToolkits() const override { return true; }
};

