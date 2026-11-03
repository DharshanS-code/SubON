#include "SubOnWindowsApoConnection.h"

#include <type_traits>

static_assert(sizeof(float) == 4, "SubON APO requires 32-bit IEEE float samples");
static_assert(std::is_standard_layout_v<subon::apo::AudioFormat>,
              "The platform-neutral audio format must remain a simple value type");

// This translation unit intentionally compiles the Windows SDK-facing adapter
// as part of the normal Windows build. Runtime activation is added by the COM
// APO server target; this target catches SDK/header/signature drift early.
namespace subon::apo::windows {
void windowsApoConnectionAdapterCompileProbe() noexcept {
    AudioFormat format{};
    (void)describeFloat32Format(nullptr, format);
}
} // namespace subon::apo::windows
