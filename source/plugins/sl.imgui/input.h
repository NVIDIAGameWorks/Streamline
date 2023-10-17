#pragma once

#include <inttypes.h>

namespace sl
{
namespace input
{

using KeyFlags = uint32_t;

// Same mappings as in GLFW
constexpr KeyFlags kKeyFlagShift = 0x01;
constexpr KeyFlags kKeyFlagControl = 0x02;
constexpr KeyFlags kKeyFlagAlt = 0x04;

// Same mappings as in GLFW
enum class KeyEvent : uint32_t
{
    eKeyRelease,
    eKeyPress,
    eKeyRepeat,
    eChar,

    eCount
};

enum class KeyValue : uint32_t
{
    eUnknown,
    eA,
    eB,
    eC,
    eD,
    eE,
    eF,
    eG,
    eH,
    eI,
    eJ,
    eK,
    eL,
    eM,
    eN,
    eO,
    eP,
    eQ,
    eR,
    eS,
    eT,
    eU,
    eV,
    eW,
    eX,
    eY,
    eZ,
    eSpace,
    eApostrophe,
    eComma,
    eMinus,
    ePeriod,
    eSlash,
    eKey0,
    eKey1,
    eKey2,
    eKey3,
    eKey4,
    eKey5,
    eKey6,
    eKey7,
    eKey8,
    eKey9,
    eSemicolon,
    eEqual,
    eLeftBracket,
    eBackslash,
    eRightBracket,
    eGraveAccent,
    eEscape,
    eTab,
    eEnter,
    eBackspace,
    eInsert,
    eDel,
    eRight,
    eLeft,
    eDown,
    eUp,
    ePageUp,
    ePageDown,
    eHome,
    eEnd,
    eCapsLock,
    eScrollLock,
    eNumLock,
    ePrintScreen,
    ePause,
    eF1,
    eF2,
    eF3,
    eF4,
    eF5,
    eF6,
    eF7,
    eF8,
    eF9,
    eF10,
    eF11,
    eF12,
    eNumpad0,
    eNumpad1,
    eNumpad2,
    eNumpad3,
    eNumpad4,
    eNumpad5,
    eNumpad6,
    eNumpad7,
    eNumpad8,
    eNumpad9,
    eNumpadDel,
    eNumpadDivide,
    eNumpadMultiply,
    eNumpadSubtract,
    eNumpadAdd,
    eNumpadEnter,
    eNumpadEqual,
    eLeftShift,
    eLeftControl,
    eLeftAlt,
    eRightShift,
    eRightControl,
    eRightAlt,

    eCount
};

struct KeyboardEvent
{
    union
    {
        KeyValue key;
        char character[MB_LEN_MAX];
    };
    KeyEvent event;
    KeyFlags flags;
};

enum class MouseEventType : uint32_t
{
    eLeftButtonDown,
    eLeftButtonUp,
    eRightButtonDown,
    eRightButtonUp,
    eMiddleButtonDown,
    eMiddleButtonUp,
    eMove,
    eScroll,

    eCount
};

struct MouseEvent
{
    MouseEventType type;
    union
    {
        type::Float2 coords;
        type::Float2 scrollDelta;
    };
    KeyFlags flags;
};

enum class JoystickInput : uint32_t
{
    eLeftStickRight,
    eLeftStickLeft,
    eLeftStickUp,
    eLeftStickDown,
    eRightStickRight,
    eRightStickLeft,
    eRightStickUp,
    eRightStickDown,
    eLeftTrigger,
    eRightTrigger,
    eA,
    eB,
    eX,
    eY,
    eLeftShoulder,
    eRightShoulder,
    eMenu1,
    eMenu2,
    eLeftStick,
    eRightStick,
    eDpadUp,
    eDpadRight,
    eDpadDown,
    eDpadLeft,

    eCount
};

enum class JoystickDevice : uint32_t
{
    eJoystick1,
    eJoystick2,
    eJoystick3,
    eJoystick4,

    eCount
};
struct JoystickEvent
{
    float value;
    JoystickDevice device;
    JoystickInput input;
};

}
}
