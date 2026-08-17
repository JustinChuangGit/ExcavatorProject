#include "ExcavatorROSModule.h"

#include "Modules/ModuleManager.h"
#include "WebSocketsModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogExcavatorROS, Log, All);

void FExcavatorROSModule::StartupModule()
{
    FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets");
    UE_LOG(LogExcavatorROS, Log, TEXT("ExcavatorROS module started"));
}

void FExcavatorROSModule::ShutdownModule()
{
    UE_LOG(LogExcavatorROS, Log, TEXT("ExcavatorROS module stopped"));
}

IMPLEMENT_MODULE(FExcavatorROSModule, ExcavatorROS);
