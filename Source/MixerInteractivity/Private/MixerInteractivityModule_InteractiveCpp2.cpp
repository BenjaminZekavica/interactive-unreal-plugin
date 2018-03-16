//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "MixerInteractivityModule_InteractiveCpp2.h"

#if MIXER_BACKEND_INTERACTIVE_CPP_2

#include "MixerInteractivitySettings.h"
#include "MixerInteractivityUserSettings.h"
#include "MixerInteractivityLog.h"
#include "StringConv.h"

IMPLEMENT_MODULE(FMixerInteractivityModule_InteractiveCpp2, MixerInteractivity);

namespace
{
	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, FText& Result)
	{
		size_t RequiredSize = 0;
		TArray<char> Utf8String;
		if (mixer::interactive_control_get_property_string(Session, ControlName, PropertyName, nullptr, &RequiredSize) != mixer::MIXER_ERROR_BUFFER_SIZE)
		{
			return false;
		}

		Utf8String.AddUninitialized(RequiredSize);
		if (mixer::interactive_control_get_property_string(Session, ControlName, PropertyName, Utf8String.GetData(), &RequiredSize) != mixer::MIXER_OK)
		{
			return false;
		}

		Result = FText::FromString(UTF8_TO_TCHAR(Utf8String.GetData()));
		return true;
	}

	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, float& Result)
	{
		return mixer::interactive_control_get_property_float(Session, ControlName, PropertyName, &Result) == mixer::MIXER_OK;
	}

	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, bool& Result)
	{
		return mixer::interactive_control_get_property_bool(Session, ControlName, PropertyName, &Result) == mixer::MIXER_OK;
	}

	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, int64& Result)
	{
		return mixer::interactive_control_get_property_int64(Session, ControlName, PropertyName, &Result) == mixer::MIXER_OK;
	}

	bool GetControlPropertyHelper(mixer::interactive_session Session, const char* ControlName, const char *PropertyName, uint32& Result)
	{
		int SignedResult;
		if (mixer::interactive_control_get_property_int(Session, ControlName, PropertyName, &SignedResult) != mixer::MIXER_OK)
		{
			return false;
		}

		Result = static_cast<uint32>(SignedResult);
		return true;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::StartInteractivity()
{
	if (InteractiveSession != nullptr)
	{
		if (mixer::interactive_set_ready(InteractiveSession, true) == mixer::MIXER_OK)
		{
			SetInteractivityState(EMixerInteractivityState::Interactivity_Starting);
		}
	}
}

void FMixerInteractivityModule_InteractiveCpp2::StopInteractivity()
{
	if (InteractiveSession != nullptr)
	{
		if (mixer::interactive_set_ready(InteractiveSession, false) == mixer::MIXER_OK)
		{
			SetInteractivityState(EMixerInteractivityState::Interactivity_Stopping);
		}
	}
}

void FMixerInteractivityModule_InteractiveCpp2::SetCurrentScene(FName Scene, FName GroupName)
{
	if (InteractiveSession != nullptr)
	{
		mixer::interactive_group_set_scene(InteractiveSession, GroupName != NAME_None ? GroupName.GetPlainANSIString() : "default", Scene.GetPlainANSIString());
	}
}

FName FMixerInteractivityModule_InteractiveCpp2::GetCurrentScene(FName GroupName)
{
	FGetCurrentSceneEnumContext Context;
	Context.GroupName = GroupName != NAME_None ? GroupName : FName("default");
	Context.OutSceneName = NAME_None;
	if (InteractiveSession != nullptr)
	{
		mixer::interactive_set_session_context(InteractiveSession, &Context);
		mixer::interactive_get_groups(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetCurrentScene);
		mixer::interactive_set_session_context(InteractiveSession, nullptr);
	}
	return Context.OutSceneName;
}

void FMixerInteractivityModule_InteractiveCpp2::TriggerButtonCooldown(FName Button, FTimespan CooldownTime)
{
	if (InteractiveSession != nullptr)
	{
		mixer::interactive_control_trigger_cooldown(InteractiveSession, Button.GetPlainANSIString(), static_cast<uint32>(CooldownTime.GetTotalMilliseconds()));
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::CreateGroup(FName GroupName, FName InitialScene)
{
	if (InteractiveSession == nullptr)
	{
		return false;
	}

	return mixer::interactive_create_group(InteractiveSession, GroupName.GetPlainANSIString(), InitialScene != NAME_None ? InitialScene.GetPlainANSIString() : "default") == mixer::MIXER_OK;
}

bool FMixerInteractivityModule_InteractiveCpp2::GetParticipantsInGroup(FName GroupName, TArray<TSharedPtr<const FMixerRemoteUser>>& OutParticipants)
{
	if (InteractiveSession == nullptr)
	{
		return false;
	}

	FGetParticipantsInGroupEnumContext Context;
	Context.MixerModule = this;
	Context.GroupName = GroupName != NAME_None ? GroupName : FName("default");
	Context.OutParticipants = &OutParticipants;
	mixer::interactive_set_session_context(InteractiveSession, &Context);
	int32 EnumResult = mixer::interactive_get_participants(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetParticipantsInGroup);
	mixer::interactive_set_session_context(InteractiveSession, nullptr);
	return EnumResult == mixer::MIXER_OK;
}

bool FMixerInteractivityModule_InteractiveCpp2::MoveParticipantToGroup(FName GroupName, uint32 ParticipantId)
{
	if (InteractiveSession == nullptr)
	{
		return false;
	}

	TSharedPtr<FMixerRemoteUserCached>* Participant = RemoteParticipantCacheByUint.Find(ParticipantId);
	if (Participant == nullptr)
	{
		return false;
	}

	return mixer::interactive_set_participant_group(
		InteractiveSession,
		TCHAR_TO_UTF8(*(*Participant)->SessionGuid.ToString(EGuidFormats::DigitsWithHyphens).ToLower()),
		GroupName.GetPlainANSIString()) == mixer::MIXER_OK;
}

void FMixerInteractivityModule_InteractiveCpp2::CaptureSparkTransaction(const FString& TransactionId)
{
	if (InteractiveSession != nullptr)
	{
		mixer::interactive_capture_transaction(InteractiveSession, TCHAR_TO_UTF8(*TransactionId));
	}
}

void FMixerInteractivityModule_InteractiveCpp2::CallRemoteMethod(const FString& MethodName, const TSharedRef<FJsonObject> MethodParams)
{
	if (InteractiveSession != nullptr)
	{
		FString SerializedParams;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&SerializedParams, 0);
		FJsonSerializer::Serialize(MethodParams, Writer);

		uint32 MessageId = 0;
		mixer::interactive_send_method(InteractiveSession, TCHAR_TO_UTF8(*MethodName), TCHAR_TO_UTF8(*SerializedParams), true, &MessageId);
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::StartInteractiveConnection()
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
	{
		UE_LOG(LogMixerInteractivity, Warning, TEXT("StartInteractiveConnection failed - plugin state %d."),
			static_cast<int32>(GetInteractiveConnectionAuthState()));
		return false;
	}

	SetInteractiveConnectionAuthState(EMixerLoginState::Logging_In);

	const UMixerInteractivitySettings* Settings = GetDefault<UMixerInteractivitySettings>();
	const UMixerInteractivityUserSettings* UserSettings = GetDefault<UMixerInteractivityUserSettings>();

	int32 ConnectResult = mixer::interactive_connect(
							TCHAR_TO_UTF8(*UserSettings->GetAuthZHeaderValue()),
							TCHAR_TO_UTF8(*FString::FromInt(Settings->GameVersionId)),
							TCHAR_TO_UTF8(*Settings->ShareCode),
							true,
							&InteractiveSession);
	if (ConnectResult != mixer::MIXER_OK)
	{
		return false;
	}

	bPerParticipantStateCaching = Settings->bPerParticipantStateCaching;

	mixer::interactive_reg_error_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionError);
	mixer::interactive_reg_state_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionStateChanged);
	mixer::interactive_reg_button_input_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionButtonInput);
	mixer::interactive_reg_coordinate_input_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionCoordinateInput);
	mixer::interactive_reg_participants_changed_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnSessionParticipantsChanged);
	mixer::interactive_reg_unhandled_method_handler(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnUnhandledMethod);

	mixer::interactive_get_scenes(InteractiveSession, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateScenesForInit);

	SetInteractiveConnectionAuthState(EMixerLoginState::Logged_In);

	return true;
}

void FMixerInteractivityModule_InteractiveCpp2::StopInteractiveConnection()
{
	if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
	{
		SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		mixer::interactive_disconnect(InteractiveSession);
		InteractiveSession = nullptr;
		bPerParticipantStateCaching = false;
		RemoteParticipantCacheByGuid.Empty();
		RemoteParticipantCacheByUint.Empty();
		ButtonCache.Empty();
		StickCache.Empty();
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::HandleSingleControlUpdate(FName ControlId, const TSharedRef<FJsonObject> ControlData)
{
	FMixerButtonPropertiesCached* ButtonProps = ButtonCache.Find(ControlId);
	if (ButtonProps != nullptr)
	{
		double Cooldown = 0.0f;
		if (ControlData->TryGetNumberField(TEXT("cooldown"), Cooldown))
		{
			uint64 TimeNowInMixerUnits = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
			if (Cooldown > TimeNowInMixerUnits)
			{
				ButtonProps->State.RemainingCooldown = FTimespan::FromMilliseconds(static_cast<double>(static_cast<uint64>(Cooldown) - TimeNowInMixerUnits));
			}
			else
			{
				ButtonProps->State.RemainingCooldown = FTimespan::Zero();
			}
		}

		FString Text;
		if (ControlData->TryGetStringField(TEXT("text"), Text))
		{
			ButtonProps->Desc.ButtonText = FText::FromString(Text);
		}

		FString Tooltip;
		if (ControlData->TryGetStringField(TEXT("tooltip"), Tooltip))
		{
			ButtonProps->Desc.HelpText = FText::FromString(Tooltip);
		}

		uint32 Cost;
		if (ControlData->TryGetNumberField(TEXT("cost"), Cost))
		{
			ButtonProps->Desc.SparkCost = Cost;
		}

		bool bDisabled;
		if (ControlData->TryGetBoolField(TEXT("disabled"), bDisabled))
		{
			ButtonProps->State.Enabled = !bDisabled;
		}

		double Progress;
		if (ControlData->TryGetNumberField(TEXT("progress"), Progress))
		{
			ButtonProps->State.Progress = Progress;
		}

		return true;
	}

	FMixerStickPropertiesCached* StickProps = StickCache.Find(ControlId);
	if (StickProps != nullptr)
	{
		bool bDisabled;
		if (ControlData->TryGetBoolField(TEXT("disabled"), bDisabled))
		{
			StickProps->State.Enabled = !bDisabled;
		}

		return true;
	}

	return false;
}

bool FMixerInteractivityModule_InteractiveCpp2::Tick(float DeltaTime)
{
	FMixerInteractivityModule::Tick(DeltaTime);

	mixer::interactive_run(InteractiveSession, 10);

	FTimespan CooldownDecrement = FTimespan::FromSeconds(DeltaTime);
	for (TMap<FName, FMixerButtonPropertiesCached>::TIterator It(ButtonCache); It; ++It)
	{
		It->Value.State.RemainingCooldown = FMath::Max(It->Value.State.RemainingCooldown - CooldownDecrement, FTimespan::Zero());

		It->Value.State.DownCount = 0;
		It->Value.State.UpCount = 0;

		// Leave PressCount alone
	}

	return true;
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionStateChanged(void* Context, mixer::interactive_session Session, mixer::interactive_state PreviousState, mixer::interactive_state NewState)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());
	switch (NewState)
	{
	case mixer::not_ready:
		InteractiveModule.SetInteractivityState(EMixerInteractivityState::Not_Interactive);
		break;

	case mixer::ready:
		InteractiveModule.SetInteractivityState(EMixerInteractivityState::Interactive);
		break;

	case mixer::disconnected:
	default:
		InteractiveModule.SetInteractivityState(EMixerInteractivityState::Not_Interactive);
		InteractiveModule.SetInteractiveConnectionAuthState(EMixerLoginState::Not_Logged_In);
		break;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionError(void* Context, mixer::interactive_session Session, int ErrorCode, const char* ErrorMessage, size_t ErrorMessageLength)
{
	UE_LOG(LogMixerInteractivity, Error, TEXT("Session error %d: %hs"), ErrorCode, ErrorMessage);
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionButtonInput(void* Context, mixer::interactive_session Session, const mixer::interactive_button_input* Input)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());

	FGuid ParticipantGuid;
	if (!FGuid::Parse(Input->participantId, ParticipantGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Participant id %hs was not in the expected format (guid)"), Input->participantId);
		return;
	}

	TSharedPtr<FMixerRemoteUserCached> ButtonUser = InteractiveModule.RemoteParticipantCacheByGuid.FindChecked(ParticipantGuid);

	FMixerButtonPropertiesCached& CachedProps = InteractiveModule.ButtonCache.FindChecked(FName(Input->control.id));

	FMixerButtonEventDetails ButtonEventDetails;
	ButtonEventDetails.Pressed = Input->action == mixer::down;
	ButtonEventDetails.SparkCost = CachedProps.Desc.SparkCost;
	if (ButtonEventDetails.Pressed)
	{
		CachedProps.State.DownCount += 1;
		if (InteractiveModule.bPerParticipantStateCaching)
		{
			CachedProps.HoldingParticipants.Add(ButtonUser->Id);
			CachedProps.State.PressCount = CachedProps.HoldingParticipants.Num();
		}
	}
	else
	{
		CachedProps.State.UpCount += 1;
		if (InteractiveModule.bPerParticipantStateCaching)
		{
			CachedProps.HoldingParticipants.Remove(ButtonUser->Id);
			CachedProps.State.PressCount = CachedProps.HoldingParticipants.Num();
		}
	}

	InteractiveModule.OnButtonEvent().Broadcast(Input->control.id, ButtonUser, ButtonEventDetails);
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionCoordinateInput(void* Context, mixer::interactive_session Session, const mixer::interactive_coordinate_input* Input)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());

	FGuid ParticipantGuid;
	if (!FGuid::Parse(Input->participantId, ParticipantGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Participant id %hs was not in the expected format (guid)"), Input->participantId);
		return;
	}

	TSharedPtr<FMixerRemoteUserCached> StickUser = InteractiveModule.RemoteParticipantCacheByGuid.FindChecked(ParticipantGuid);
	if (InteractiveModule.bPerParticipantStateCaching)
	{
		FMixerStickPropertiesCached* CachedProps = InteractiveModule.StickCache.Find(FName(Input->control.id));
		if (CachedProps != nullptr)
		{
			if (Input->x != 0 || Input->y != 0)
			{
				CachedProps->State.Axes *= CachedProps->PerParticipantStickValue.Num();
				FVector2D& PerUserStickValue = CachedProps->PerParticipantStickValue.FindOrAdd(StickUser->Id);
				CachedProps->State.Axes -= PerUserStickValue;
				PerUserStickValue = FVector2D(Input->x, Input->y);
				CachedProps->State.Axes += PerUserStickValue;
				CachedProps->State.Axes /= CachedProps->PerParticipantStickValue.Num();
			}
			else
			{
				FVector2D* OldPerUserStickValue = CachedProps->PerParticipantStickValue.Find(StickUser->Id);
				if (OldPerUserStickValue != nullptr)
				{
					CachedProps->State.Axes *= CachedProps->PerParticipantStickValue.Num();
					CachedProps->State.Axes -= *OldPerUserStickValue;
					CachedProps->PerParticipantStickValue.Remove(StickUser->Id);
					if (CachedProps->PerParticipantStickValue.Num() > 0)
					{
						CachedProps->State.Axes /= CachedProps->PerParticipantStickValue.Num();
					}
					else
					{
						CachedProps->State.Axes = FVector2D(0, 0);
					}
				}
			}
		}
	}

	InteractiveModule.OnStickEvent().Broadcast(Input->control.id, StickUser, FVector2D(Input->x, Input->y));
}

void FMixerInteractivityModule_InteractiveCpp2::OnSessionParticipantsChanged(void* Context, mixer::interactive_session Session, mixer::participant_action Action, const mixer::interactive_participant* Participant)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());

	FGuid SessionGuid;
	if (!FGuid::Parse(Participant->id, SessionGuid))
	{
		UE_LOG(LogMixerInteractivity, Error, TEXT("Participant session id %hs was not in the expected format (guid)"), Participant->id);
		return;
	}

	switch (Action)
	{
	case mixer::participant_join:
		{
			check(!InteractiveModule.RemoteParticipantCacheByGuid.Contains(SessionGuid));

			TSharedPtr<FMixerRemoteUserCached> CachedParticipant = MakeShared<FMixerRemoteUserCached>();
			CachedParticipant->Id = Participant->userId;
			CachedParticipant->SessionGuid = SessionGuid;
			CachedParticipant->Name = UTF8_TO_TCHAR(Participant->userName);
			CachedParticipant->Level = Participant->level;
			CachedParticipant->Group = Participant->groupId;
			CachedParticipant->InputEnabled = !Participant->disabled;
			// Timestamps are in ms since January 1 1970
			CachedParticipant->ConnectedAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->connectedAtMs / 1000.0));
			CachedParticipant->InputAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->lastInputAtMs / 1000.0));

			InteractiveModule.RemoteParticipantCacheByGuid.Add(CachedParticipant->SessionGuid, CachedParticipant);
			InteractiveModule.RemoteParticipantCacheByUint.Add(CachedParticipant->Id, CachedParticipant);
	}
		break;

	case mixer::participant_leave:
		{
			TSharedPtr<FMixerRemoteUser> RemovedUser = InteractiveModule.RemoteParticipantCacheByGuid.FindAndRemoveChecked(SessionGuid);
			InteractiveModule.RemoteParticipantCacheByUint.FindAndRemoveChecked(RemovedUser->Id);
		}
		break;

	case mixer::participant_update:
		{
			TSharedPtr<FMixerRemoteUser> CachedParticipant = InteractiveModule.RemoteParticipantCacheByGuid.FindChecked(SessionGuid);
			check(CachedParticipant.IsValid());
			check(CachedParticipant->Id == Participant->userId);
			CachedParticipant->Name = UTF8_TO_TCHAR(Participant->userName);
			CachedParticipant->Level = Participant->level;
			CachedParticipant->Group = Participant->groupId;
			CachedParticipant->InputAt = FDateTime::FromUnixTimestamp(static_cast<int64>(Participant->lastInputAtMs / 1000.0));
			CachedParticipant->InputEnabled = !Participant->disabled;
	}
		break;

	default:
		UE_LOG(LogMixerInteractivity, Error, TEXT("Unknown participant change type %d"), static_cast<int32>(Action));
		break;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnUnhandledMethod(void* Context, mixer::interactive_session Session, const char* MethodJson, size_t MethodJsonLength)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(FString(UTF8_TO_TCHAR(MethodJson)));
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
	{
		FString Method;
		if (JsonObject->TryGetStringField(TEXT("method"), Method))
		{
			const TSharedPtr<FJsonObject> *ParamsObject;
			if (JsonObject->TryGetObjectField(TEXT("params"), ParamsObject))
			{
				if (Method == TEXT("giveInput"))
				{
					InteractiveModule.HandleCustomControlInputMessage(ParamsObject->Get());
				}
				else if (Method == TEXT("onControlUpdate"))
				{
					InteractiveModule.HandleControlUpdateMessage(ParamsObject->Get());
				}
				else
				{
					InteractiveModule.OnCustomMethodCall().Broadcast(*Method, ParamsObject->ToSharedRef());
				}
			}
		}
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::GetButtonDescription(FName Button, FMixerButtonDescription& OutDesc)
{
	FMixerButtonPropertiesCached* CachedProps = ButtonCache.Find(Button);
	if (CachedProps != nullptr)
	{
		OutDesc = CachedProps->Desc;
		return true;
	}
	else
	{
		return false;
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::GetButtonState(FName Button, FMixerButtonState& OutState)
{
	FMixerButtonPropertiesCached* CachedProps = ButtonCache.Find(Button);
	if (CachedProps != nullptr)
	{
		OutState = CachedProps->State;
		if (!bPerParticipantStateCaching)
		{
			OutState.PressCount = 0;
		}
		return true;
	}
	else
	{
		return false;
	}

	return true;
}

bool FMixerInteractivityModule_InteractiveCpp2::GetButtonState(FName Button, uint32 ParticipantId, FMixerButtonState& OutState)
{
	if (bPerParticipantStateCaching)
	{
		FMixerButtonPropertiesCached* CachedProps = ButtonCache.Find(Button);
		if (CachedProps != nullptr)
		{
			OutState = CachedProps->State;

			// Even with per-participant tracking on we don't maintain these.  
			OutState.DownCount = 0;
			OutState.UpCount = 0;
			OutState.PressCount = CachedProps->HoldingParticipants.Contains(ParticipantId) ? 1 : 0;

			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Polling per-participant button state requires that per-participant state caching is enabled."));
		}
		return false;
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::GetStickDescription(FName Stick, FMixerStickDescription& OutDesc)
{
	// No supported properties
	return false;
}

bool FMixerInteractivityModule_InteractiveCpp2::GetStickState(FName Stick, FMixerStickState& OutState)
{
	if (bPerParticipantStateCaching)
	{
		FMixerStickPropertiesCached* CachedProps = StickCache.Find(Stick);
		if (CachedProps != nullptr)
		{
			OutState = CachedProps->State;

			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Polling aggregate stick state requires that per-participant state caching is enabled."));
		}
		return false;
	}
}

bool FMixerInteractivityModule_InteractiveCpp2::GetStickState(FName Stick, uint32 ParticipantId, FMixerStickState& OutState)
{
	if (bPerParticipantStateCaching)
	{
		FMixerStickPropertiesCached* CachedProps = StickCache.Find(Stick);
		if (CachedProps != nullptr)
		{
			OutState.Enabled = CachedProps->State.Enabled;

			FVector2D* PerParticipantState = CachedProps->PerParticipantStickValue.Find(ParticipantId);
			OutState.Axes = (PerParticipantState != nullptr) ? *PerParticipantState : FVector2D(0, 0);
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (GetInteractiveConnectionAuthState() != EMixerLoginState::Not_Logged_In)
		{
			UE_LOG(LogMixerInteractivity, Error, TEXT("Polling per-participant stick state requires that per-participant state caching is enabled."));
		}
		return false;
	}
}

TSharedPtr<const FMixerRemoteUser> FMixerInteractivityModule_InteractiveCpp2::GetParticipant(uint32 ParticipantId)
{
	TSharedPtr<FMixerRemoteUserCached>* FoundUser = RemoteParticipantCacheByUint.Find(ParticipantId);
	return FoundUser ? *FoundUser : TSharedPtr<const FMixerRemoteUser>();
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetCurrentScene(void* Context, mixer::interactive_session Session, mixer::interactive_group* Group)
{
	FGetCurrentSceneEnumContext* GetSceneContext = static_cast<FGetCurrentSceneEnumContext*>(Context);
	if (GetSceneContext->GroupName == Group->id)
	{
		GetSceneContext->OutSceneName = Group->sceneId;
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateForGetParticipantsInGroup(void* Context, mixer::interactive_session Session, mixer::interactive_participant* Participant)
{
	FGetParticipantsInGroupEnumContext* GetParticipantsContext = static_cast<FGetParticipantsInGroupEnumContext*>(Context);
	if (GetParticipantsContext->GroupName == Participant->groupId)
	{
		TSharedPtr<FMixerRemoteUserCached>* User = GetParticipantsContext->MixerModule->RemoteParticipantCacheByUint.Find(Participant->userId);
		if (User != nullptr)
		{
			GetParticipantsContext->OutParticipants->Add(*User);
		}
	}
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateScenesForInit(void* Context, mixer::interactive_session Session, mixer::interactive_scene* Scene)
{
	mixer::interactive_scene_get_controls(Session, Scene->id, &FMixerInteractivityModule_InteractiveCpp2::OnEnumerateControlsForInit);
}

void FMixerInteractivityModule_InteractiveCpp2::OnEnumerateControlsForInit(void* Context, mixer::interactive_session Session, mixer::interactive_control* Control)
{
	FMixerInteractivityModule_InteractiveCpp2& InteractiveModule = static_cast<FMixerInteractivityModule_InteractiveCpp2&>(IMixerInteractivityModule::Get());

	if (FPlatformString::Strcmp(Control->kind, "button") == 0)
	{
		FMixerButtonPropertiesCached& CachedProps = InteractiveModule.ButtonCache.Add(FName(Control->id));

		GetControlPropertyHelper(Session, Control->id, "cost", CachedProps.Desc.SparkCost);
		GetControlPropertyHelper(Session, Control->id, "text", CachedProps.Desc.ButtonText);
		GetControlPropertyHelper(Session, Control->id, "tooltip", CachedProps.Desc.HelpText);
		GetControlPropertyHelper(Session, Control->id, "tooltip", CachedProps.Desc.HelpText);

		CachedProps.State.DownCount = 0;
		CachedProps.State.UpCount = 0;
		CachedProps.State.PressCount = 0;
		CachedProps.State.Enabled = true;
	}
	else if (FPlatformString::Strcmp(Control->kind, "joystick") == 0)
	{
		FMixerStickPropertiesCached& CachedProps = InteractiveModule.StickCache.Add(FName(Control->id));
		CachedProps.State.Enabled = true;
	}
}

#endif

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
int32 MixerInteractiveCpp2LinkerHelper;