//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "K2Node_MixerStickEvent.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintEditorUtils.h"
#include "GraphEditorSettings.h"
#include "KismetCompiler.h"
#include "MixerDynamicDelegateBinding.h"
#include "MixerInteractivitySettings.h"

#define LOCTEXT_NAMESPACE "MixerInteractivityEditor"

void UK2Node_MixerStickEvent::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	check(Settings);
	if (!Settings->CachedSticks.Contains(StickId))
	{
		MessageLog.Warning(*FText::Format(LOCTEXT("MixerStickNode_UnknownStickWarning", "Mixer Stick Event specifies invalid stick id '{0}' for @@"), FText::FromName(StickId)).ToString(), this);
	}
}

void UK2Node_MixerStickEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	auto CustomizeMixerNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, FName StickName)
	{
		UK2Node_MixerStickEvent* MixerNode = CastChecked<UK2Node_MixerStickEvent>(NewNode);
		MixerNode->StickId = StickName;
		MixerNode->CustomFunctionName = FName(*FString::Printf(TEXT("MixerStickEvt_%s"), *StickName.ToString()));
		MixerNode->EventReference.SetExternalDelegateMember(FName(TEXT("MixerStickEventDynamicDelegate__DelegateSignature")));
	};

	UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
		for (FName CachedStickName : Settings->CachedSticks)
		{
			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			check(NodeSpawner != nullptr);

			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeMixerNodeLambda, CachedStickName);
			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
		}
	}
}

FText UK2Node_MixerStickEvent::GetMenuCategory() const
{
	return LOCTEXT("MixerStickNode_MenuCategory", "{MixerInteractivity}|Stick Events");
}

FText UK2Node_MixerStickEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("MixerStickNode_Title", "{0} (Mixer stick)"), FText::FromName(StickId)), this);
	}
	return CachedNodeTitle;
}

FText UK2Node_MixerStickEvent::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		CachedTooltip.SetCachedText(FText::Format(LOCTEXT("MixerStickNode_Tooltip", "Events for when the {0} stick is moved on Mixer."), FText::FromName(StickId)), this);
	}
	return CachedTooltip;
}

FSlateIcon UK2Node_MixerStickEvent::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon("EditorStyle", "GraphEditor.PadEvent_16x");
}

UClass* UK2Node_MixerStickEvent::GetDynamicBindingClass() const
{
	return UMixerDelegateBinding::StaticClass();
}

void UK2Node_MixerStickEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	FMixerStickEventBinding BindingInfo;
	BindingInfo.TargetFunctionName = CustomFunctionName;
	BindingInfo.StickId = StickId;

	UMixerDelegateBinding* MixerBindingObject =  CastChecked<UMixerDelegateBinding>(BindingObject);
	MixerBindingObject->AddStickBinding(BindingInfo);
}

#undef LOCTEXT_NAMESPACE