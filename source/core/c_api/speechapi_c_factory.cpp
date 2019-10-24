//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//
// speechapi_c_factory.cpp: Definitions for SpeechFactory related C methods
//

#include <string>
#include <vector>

#include "stdafx.h"
#include "service_helpers.h"
#include "site_helpers.h"
#include "handle_helpers.h"
#include "resource_manager.h"
#include "mock_controller.h"
#include "property_id_2_name_map.h"
#include "speechapi_c_speech_config.h"
#include "speechapi_c_auto_detect_source_lang_config.h"
#include "speechapi_c_source_lang_config.h"

using namespace Microsoft::CognitiveServices::Speech;
using namespace Microsoft::CognitiveServices::Speech::Impl;
using namespace std;

static_assert((int)OutputFormat::Simple == (int)SpeechOutputFormat_Simple, "OutputFormat should match between C and C++ layers");
static_assert((int)OutputFormat::Detailed == (int)SpeechOutputFormat_Detailed, "OutputFormat should match between C and C++ layers");


std::shared_ptr<ISpxAudioConfig> AudioConfigFromHandleOrEmptyIfInvalid(SPXAUDIOCONFIGHANDLE haudioConfig)
{
    return audio_config_is_handle_valid(haudioConfig)
        ? CSpxSharedPtrHandleTableManager::GetPtr<ISpxAudioConfig, SPXAUDIOCONFIGHANDLE>(haudioConfig)
        : nullptr;
}

std::shared_ptr<ISpxAutoDetectSourceLangConfig> AutoDetectSourceLangConfigFromHandleOrEmptyIfInvalid(SPXAUTODETECTSOURCELANGCONFIGHANDLE hautoDetectSourceLangConfig)
{
    return auto_detect_source_lang_config_is_handle_valid(hautoDetectSourceLangConfig)
        ? CSpxSharedPtrHandleTableManager::GetPtr<ISpxAutoDetectSourceLangConfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE>(hautoDetectSourceLangConfig)
        : nullptr;
}

std::shared_ptr<ISpxSourceLanguageConfig> SourceLangConfigFromHandleOrEmptyIfInvalid(SPXSOURCELANGCONFIGHANDLE hSourceLangConfig)
{
    return source_lang_config_is_handle_valid(hSourceLangConfig)
        ? CSpxSharedPtrHandleTableManager::GetPtr<ISpxSourceLanguageConfig, SPXSOURCELANGCONFIGHANDLE>(hSourceLangConfig)
        : nullptr;
}

template<typename FactoryMethod>
auto create_from_config(SPXHANDLE hspeechconfig, SPXHANDLE hautoDetectSourceLangConfig,  SPXHANDLE hSourceLangConfig, SPXHANDLE haudioConfig, FactoryMethod fm)
{
    auto factory = SpxCreateObjectWithSite<ISpxSpeechApiFactory>("CSpxSpeechApiFactory", SpxGetRootSite());
    SPX_IFTRUE_THROW_HR(factory == nullptr, SPXERR_RUNTIME_ERROR);

    // get the input parameters from the hspeechconfig
    auto config_handles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();

    auto config = (*config_handles)[hspeechconfig];
    auto config_property_bag = SpxQueryInterface<ISpxNamedProperties>(config);
    auto factory_property_bag = SpxQueryInterface<ISpxNamedProperties>(factory);

    //copy the properties from the speech config into the factory
    if (config_property_bag != nullptr)
    {
        factory_property_bag->Copy(config_property_bag.get());
    }

    auto audio_input = AudioConfigFromHandleOrEmptyIfInvalid(haudioConfig);
    // copy the audio input properties into the factory, if any.
    auto audio_input_properties = SpxQueryInterface<ISpxNamedProperties>(audio_input);
    if (audio_input_properties != nullptr)
    {
        factory_property_bag->Copy(audio_input_properties.get());
    }

    auto auto_detect_source_lang_config = AutoDetectSourceLangConfigFromHandleOrEmptyIfInvalid(hautoDetectSourceLangConfig);
    // copy the auto detect source language config properties into the factory, if any.
    auto auto_detect_source_lang_config_properties = SpxQueryInterface<ISpxNamedProperties>(auto_detect_source_lang_config);
    if (auto_detect_source_lang_config_properties != nullptr)
    {
        if (config_property_bag != nullptr && config_property_bag->HasStringValue(GetPropertyName(PropertyId::SpeechServiceConnection_EndpointId)))
        {
            ThrowInvalidArgumentException("EndpointId on SpeechConfig is unsupported for auto detection source language scenario. "
                "Please set per language endpointId through SourceLanguageConfig and use it to construct AutoDetectSourceLanguageConfig.");
        }
        factory_property_bag->Copy(auto_detect_source_lang_config_properties.get());
    }

     auto source_lang_config = SourceLangConfigFromHandleOrEmptyIfInvalid(hSourceLangConfig);
    // copy the source language config properties into the factory, if any.
    auto source_lang_config_properties = SpxQueryInterface<ISpxNamedProperties>(source_lang_config);
    if (source_lang_config_properties != nullptr)
    {
        factory_property_bag->Copy(source_lang_config_properties.get());
    }

    return ((*factory).*fm)(audio_input);
}

SPXAPI recognizer_create_speech_recognizer_from_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, SPXHANDLE_INVALID, SPXHANDLE_INVALID, haudioInput, &ISpxSpeechApiFactory::CreateSpeechRecognizerFromConfig);

        // track the reco handle
        auto recohandles  = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_speech_recognizer_from_auto_detect_source_lang_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUTODETECTSOURCELANGCONFIGHANDLE hautoDetectSourceLangConfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !auto_detect_source_lang_config_is_handle_valid(hautoDetectSourceLangConfig));
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, hautoDetectSourceLangConfig, SPXHANDLE_INVALID, haudioInput, &ISpxSpeechApiFactory::CreateSpeechRecognizerFromConfig);
        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_speech_recognizer_from_source_lang_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXSOURCELANGCONFIGHANDLE hSourceLangConfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !source_lang_config_is_handle_valid(hSourceLangConfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, SPXHANDLE_INVALID, hSourceLangConfig, haudioInput, &ISpxSpeechApiFactory::CreateSpeechRecognizerFromConfig);
        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI dialog_service_connector_create_dialog_service_connector_from_config(SPXRECOHANDLE* ph_dialog_service_connector, SPXSPEECHCONFIGHANDLE h_dialog_service_config, SPXAUDIOCONFIGHANDLE h_audio_input)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, ph_dialog_service_connector == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(h_dialog_service_config));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *ph_dialog_service_connector = SPXHANDLE_INVALID;

        // Enable keyword verification for dialog service connector by default
        auto config_handles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto config = (*config_handles)[h_dialog_service_config];
        auto config_property_bag = SpxQueryInterface<ISpxNamedProperties>(config);
        auto enableKeywordVerification = config_property_bag->GetStringValue(KeywordConfig_EnableKeywordVerification, "true");
        config_property_bag->SetStringValue(KeywordConfig_EnableKeywordVerification, enableKeywordVerification.c_str());

        auto connector = create_from_config(h_dialog_service_config, SPXHANDLE_INVALID, SPXHANDLE_INVALID, h_audio_input, &ISpxSpeechApiFactory::CreateDialogServiceConnectorFromConfig);

        // track the handle
        auto handles = CSpxSharedPtrHandleTableManager::Get<ISpxDialogServiceConnector, SPXRECOHANDLE>();
        *ph_dialog_service_connector = handles->TrackHandle(connector);

    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_translation_recognizer_from_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;

        std::shared_ptr<ISpxRecognizer> recognizer;

        // create a factory
        auto factory = SpxCreateObjectWithSite<ISpxSpeechApiFactory>("CSpxSpeechApiFactory", SpxGetRootSite());

        auto confighandles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto speechconfig = (*confighandles)[hspeechconfig];

        //copy the properties from the speech config into the factory
        auto speechconfig_propertybag = SpxQueryInterface<ISpxNamedProperties>(speechconfig);
        auto fbag = SpxQueryInterface<ISpxNamedProperties>(factory);
        fbag->Copy(speechconfig_propertybag.get());

        auto audioInput = AudioConfigFromHandleOrEmptyIfInvalid(haudioInput);
        // copy the audio input properties into the factory, if any.
        auto audioInput_propertybag = SpxQueryInterface<ISpxNamedProperties>(audioInput);
        if (audioInput_propertybag != nullptr)
        {
            fbag->Copy(audioInput_propertybag.get());
        }
        recognizer = factory->CreateTranslationRecognizerFromConfig(audioInput);

        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_intent_recognizer_from_config(SPXRECOHANDLE* phreco, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUDIOCONFIGHANDLE haudioInput)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;
        auto recognizer = create_from_config(hspeechconfig, SPXHANDLE_INVALID, SPXHANDLE_INVALID, haudioInput, &ISpxSpeechApiFactory::CreateIntentRecognizerFromConfig);

        // track the reco handle
        auto recohandles = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = recohandles->TrackHandle(recognizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI synthesizer_create_speech_synthesizer_from_config(SPXSYNTHHANDLE* phsynth, SPXSPEECHCONFIGHANDLE hspeechconfig, SPXAUDIOCONFIGHANDLE haudioconfig)
{
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phsynth == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));

    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phsynth = SPXHANDLE_INVALID;

        // get the speech synthesis related parameters from the hspeechconfig
        auto confighandles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto speechconfig = (*confighandles)[hspeechconfig];
        auto speechconfig_propertybag = SpxQueryInterface<ISpxNamedProperties>(speechconfig);
        auto factory = SpxCreateObjectWithSite<ISpxSpeechSynthesisApiFactory>("CSpxSpeechSynthesisApiFactory", SpxGetRootSite());
        SPX_IFTRUE_THROW_HR(factory == nullptr, SPXERR_RUNTIME_ERROR);

        // copy the properties from the speech config into the factory
        auto fbag = SpxQueryInterface<ISpxNamedProperties>(factory);
        fbag->Copy(speechconfig_propertybag.get());

        auto audioOutput = AudioConfigFromHandleOrEmptyIfInvalid(haudioconfig);

        auto synthesizer = factory->CreateSpeechSynthesizerFromConfig(audioOutput);

        // track the synth handle
        auto synthhandles = CSpxSharedPtrHandleTableManager::Get<ISpxSynthesizer, SPXSYNTHHANDLE>();
        *phsynth = synthhandles->TrackHandle(synthesizer);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}


SPXAPI conversation_create_from_config(SPXCONVERSATIONHANDLE* pconversation, SPXSPEECHCONFIGHANDLE hspeechconfig, const char* id)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, pconversation == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, !speech_config_is_handle_valid(hspeechconfig));
    // id is optional
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, id == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *pconversation = SPXHANDLE_INVALID;

        // get the input parameters from the hspeechconfig
        auto confighandles = CSpxSharedPtrHandleTableManager::Get<ISpxSpeechConfig, SPXSPEECHCONFIGHANDLE>();
        auto speechconfig = (*confighandles)[hspeechconfig];
        auto speechconfig_propertybag = SpxQueryInterface<ISpxNamedProperties>(speechconfig);
        auto factory = SpxCreateObjectWithSite<ISpxSpeechApiFactory>("CSpxSpeechApiFactory", SpxGetRootSite());
        SPX_IFTRUE_THROW_HR(factory == nullptr, SPXERR_RUNTIME_ERROR);

        //copy the properties from the speech config into the factory
        auto fbag = SpxQueryInterface<ISpxNamedProperties>(factory);
        if (speechconfig_propertybag != nullptr)
        {
            fbag->Copy(speechconfig_propertybag.get());
        }

        auto conversation = factory->CreateConversationFromConfig(id);

        // track the conversation handle
        auto coversationhandles = CSpxSharedPtrHandleTableManager::Get<ISpxConversation, SPXCONVERSATIONHANDLE>();
        *pconversation = coversationhandles->TrackHandle(conversation);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_create_conversation_transcriber_from_config(SPXRECOHANDLE* phreco, SPXAUDIOCONFIGHANDLE haudioinput)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, phreco == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        *phreco = SPXHANDLE_INVALID;

        auto conversation_transcriber = SpxCreateObject<ISpxRecognizer>("CSpxConversationTranscriber", SpxGetRootSite());

        // copy the audio input properties into the conversation transcriber
        auto audioInput = AudioConfigFromHandleOrEmptyIfInvalid(haudioinput);
        auto audioInput_propertybag = SpxQueryInterface<ISpxNamedProperties>(audioInput);
        auto propertybag_cts = SpxQueryInterface<ISpxNamedProperties>(conversation_transcriber);
        if (audioInput_propertybag != nullptr)
        {
            propertybag_cts->Copy(audioInput_propertybag.get());
        }

        auto conversation_transcriber_init = SpxQueryInterface<ISpxConversationTranscriber>(conversation_transcriber);
        SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, conversation_transcriber_init == nullptr);
        conversation_transcriber_init->Init(audioInput);

        // track the conversation transcriber handle
        auto transcribers = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        *phreco = transcribers->TrackHandle(conversation_transcriber);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_join_conversation(SPXCONVERSATIONHANDLE hconv, SPXRECOHANDLE hreco)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hreco == nullptr);
    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hconv == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto convhandles = CSpxSharedPtrHandleTableManager::Get<ISpxConversation, SPXCONVERSATIONHANDLE>();
        auto conversation = (*convhandles)[hconv];

        auto transcribers = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        auto conversation_transcriber = (*transcribers)[hreco];

        auto factory = SpxQueryService<ISpxSpeechApiFactory>(conversation);
        SPX_IFTRUE_THROW_HR(factory == nullptr, SPXERR_RUNTIME_ERROR);

        auto session = SpxQueryService<ISpxSession>(conversation);
        SPX_IFTRUE_THROW_HR(session == nullptr, SPXERR_RUNTIME_ERROR);

        auto session_as_site = SpxQueryInterface<ISpxGenericSite>(session); //ISpxRecognizerSite

        auto conversation_transcriber_set_site = SpxQueryInterface<ISpxObjectWithSite>(conversation_transcriber);
        conversation_transcriber_set_site->SetSite(session_as_site);

        // hook audio input to session
        auto audio_input = SpxQueryInterface<ISpxGetAudioConfig>(conversation_transcriber);
        factory->InitSessionFromAudioInputConfig(session, audio_input->GetAudioConfig());

        // hook conversation to conversation transcriber, so that I can get the participant list later
        auto transcriber_ptr = SpxQueryInterface<ISpxConversationTranscriber>(conversation_transcriber);
        transcriber_ptr->JoinConversation(conversation);

        // hook the transcriber to session
        session->AddRecognizer(conversation_transcriber);
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}

SPXAPI recognizer_leave_conversation(SPXRECOHANDLE hreco)
{
    SPX_DBG_TRACE_SCOPE(__FUNCTION__, __FUNCTION__);

    SPX_RETURN_HR_IF(SPXERR_INVALID_ARG, hreco == nullptr);

    SPXAPI_INIT_HR_TRY(hr)
    {
        auto transcribers = CSpxSharedPtrHandleTableManager::Get<ISpxRecognizer, SPXRECOHANDLE>();
        auto conversation_transcriber = (*transcribers)[hreco];

        auto cts = SpxQueryInterface<ISpxConversationTranscriber>(conversation_transcriber);
        // leave conversation, set site to null
        cts->LeaveConversation();
    }
    SPXAPI_CATCH_AND_RETURN_HR(hr);
}
