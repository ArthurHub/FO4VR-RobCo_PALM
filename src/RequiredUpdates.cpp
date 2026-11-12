#include "PCH.h"
#include "RequiredUpdates.h"
#include <RPS/Runtime/CallerModule.h>
#include <RobCoPALM/Menu.h>
#include <atomic>

namespace wheel::required_updates {
namespace {
namespace caller_module=RPS::Runtime::CallerModule;
}
void noteDiscoveryRefusal(const palm::api::Result result,const palm::api::InterfaceId id,const std::uint32_t requestedMajor,const void* callerAddress) noexcept {
    using palm::api::Result;
    bool consumerOlder=false;
    switch(result) {
    case Result::UnsupportedMajor:
    case Result::UnknownInterface:consumerOlder=requestedMajor<palm::api::menu::kMajor;break;
    case Result::UnsupportedMinor:
    case Result::TableTooSmall:break;
    default:return; // not a version refusal
    }
    if(!caller_module::firstFromCaller(callerAddress))return;
    try {
        const auto dll=caller_module::moduleNameForLog(callerAddress);
        if(consumerOlder)
            spdlog::error("{} asked for interface {} major {}: it was built for an older RobCo PALM and gets no PALM features until it is updated.",
                dll,static_cast<std::uint32_t>(id),requestedMajor);
        else
            spdlog::error("{} asked for interface {} major {} (result {}): it needs a newer RobCo PALM and gets no PALM features until RobCo PALM is updated.",
                dll,static_cast<std::uint32_t>(id),requestedMajor,static_cast<std::uint32_t>(result));
    }catch(...){}
}
void noteRetiredExport(const char* exportName,const void* callerAddress) noexcept {
    if(!caller_module::firstFromCaller(callerAddress))return;
    try {
        spdlog::error("{} called {}, RobCo PALM's retired pre-2.0 API: it was built for an older RobCo PALM and gets no PALM features until it is updated.",
            caller_module::moduleNameForLog(callerAddress),exportName?exportName:"a retired export");
    }catch(...){}
}
void noteRpsUiWithoutDiscovery() noexcept {
    static std::atomic_flag reported=ATOMIC_FLAG_INIT;
    if(reported.test_and_set())return;
    try {
        spdlog::error("RPS_UI_Framework.dll does not export its 2.0 discovery: it is older than PALM supports. PALM's RPS UI features stay off until it is updated.");
    }catch(...){}
}
}
