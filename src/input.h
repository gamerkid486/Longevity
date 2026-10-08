#pragma once

namespace input {
    // Hooks IDirectInputDevice8::GetDeviceState/GetDeviceData (through a throwaway device's vtable,
    // A and W) so mouse flicks reach duel::OnMouseMove and the camera can be held while guarding.
    // Uses no game addresses, so it works on every Skyrim version.
    bool Install();
}
