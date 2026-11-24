#pragma once

#include <cstdint>
#include <optional>

#include <openvr.h>

namespace f4cf::vrcf
{
    enum VRButtonId : std::uint8_t
    {
        k_EButton_System = 0,
        k_EButton_ApplicationMenu = 1,
        k_EButton_Grip = 2,
        k_EButton_DPad_Left = 3,
        k_EButton_DPad_Up = 4,
        k_EButton_DPad_Right = 5,
        k_EButton_DPad_Down = 6,
        k_EButton_A = 7,

        k_EButton_ProximitySensor = 31,

        k_EButton_Axis0 = 32,
        k_EButton_Axis1 = 33,
        k_EButton_Axis2 = 34,
        k_EButton_Axis3 = 35,
        k_EButton_Axis4 = 36,

        // aliases for well known controllers
        k_EButton_SteamVR_Touchpad = k_EButton_Axis0,
        k_EButton_SteamVR_Trigger = k_EButton_Axis1,

        k_EButton_Dashboard_Back = k_EButton_Grip,

        k_EButton_Knuckles_A = k_EButton_Grip,
        k_EButton_Knuckles_B = k_EButton_ApplicationMenu,
        k_EButton_Knuckles_JoyStick = k_EButton_Axis3,

        k_EButton_Max = 64
    };

    inline uint64_t ButtonMaskFromId(const VRButtonId id)
    {
        return 1ull << id;
    }

    // TODO: remove this after migrating all code from getControllerState_DEPRECATED
    enum class TrackerType : std::uint8_t
    {
        HMD,
        Left,
        Right,
        Vive
    };

    /**
     * A controller by the player's handedness (Primary, Offhand) or by its side (Right, Left). Any is either
     * controller, for a binding alone: VRControllersManager::check resolves it to each hand, and every other
     * call that takes a hand needs one controller.
     */
    enum class Hand : std::uint8_t
    {
        Primary,
        Offhand,
        Right,
        Left,
        Any,
    };

    /**
     * The direction of the controller thumbstick input. Right to Down are the one way an axis is pushed, all a
     * pushed direction is ever read as; Any, UpDown and RightLeft are what a binding accepts when it asks for
     * the stick to be moved rather than pushed one way: every direction, or the two of one axis.
     */
    enum class Direction : std::uint8_t
    {
        Right,
        Left,
        Up,
        Down,
        Any,
        UpDown,
        RightLeft,
    };

    /**
     * The analog axis of the controller.
     */
    enum class Axis : std::uint8_t
    {
        Thumbstick = 0,
        Trigger,
        Grip,
        Unknown2,
        Unknown3,
    };

    /**
     * How a button (or axis) must be activated for an InputBinding to be considered "triggered".
     * Each value maps to one of the VRControllersManager check methods; see check().
     */
    enum class ActivationType : std::uint8_t
    {
        Disabled, // never triggers - the binding is turned off (config "none" / "off" / "disabled" / empty value)
        Touch, // isTouching - button is currently touched (capacitive), not necessarily pressed
        Press, // isPressed - button edge-pressed this frame (debounced)
        HoldDown, // isPressHeldDown - button held down, every frame; uses `duration` as the minimum hold
        Release, // isReleased - button edge-released this frame; uses `duration` as the maximum hold to still count (0 = any)
        Tap, // isTap - quick press-and-release (held < 0.3s); the "Tap" gesture
        LongPress, // isLongPressed - button held longer than `duration`
        DoublePress, // isDoublePressed - two presses within `duration`
        AxisDirection, // isAxisPressed - `axis` pushed past `threshold` in `direction` (or any of the ones it accepts)
    };

    /**
     * Optional modifier button that must be held down for a binding to trigger (a chord).
     * `hand` is optional: when unset the modifier is checked on the binding's own hand; set it to
     * require the modifier on a specific hand (e.g. binding on the primary hand, modifier on the offhand).
     */
    struct InputModifier
    {
        vr::EVRButtonId button = vr::k_EButton_Grip;
        std::optional<Hand> hand;

        bool operator==(const InputModifier&) const = default;
    };

    /**
     * A fully-described, config-loadable controller input binding.
     * Bundles the hand, the button/axis, and how it must be activated so a single VRControllersManager::check()
     * call can evaluate any binding loaded from config. The meaning of the timing/sensitivity fields depends on
     * `type` (see ActivationType). For button types `button` is used; for AxisDirection `axis` + `direction` are used.
     */
    struct InputBinding
    {
        Hand hand = Hand::Primary;
        ActivationType type = ActivationType::Press;

        // Used by all button activation types (everything except AxisDirection).
        vr::EVRButtonId button = vr::k_EButton_SteamVR_Trigger;

        // Used by AxisDirection only.
        Axis axis = Axis::Thumbstick;
        Direction direction = Direction::Up;

        // Optional modifier button that must be held down for the binding to trigger (see InputModifier).
        std::optional<InputModifier> modifier;

        // Timing / sensitivity - meaning depends on `type` (see ActivationType). 0 means "use the per-type default".
        float duration = 0.0f;
        float threshold = 0.85f; // AxisDirection only
        float cooldown = 0.15f; // AxisDirection only
        // AxisDirection only: a held push repeats faster the longer it is held (see AxisRepeat); false keeps it at `cooldown`
        bool accelerate = true;

        // When a consumer applies suppression for this binding (e.g. WandActivationSphere while the bound
        // hand is inside its zone), hide the physical button from the game. Off by default.
        bool suppress = false;

        /**
         * True when a binding can actually fire (i.e. it is not the "none"/disabled binding).
         */
        bool isEnabled() const
        {
            return type != ActivationType::Disabled;
        }

        /**
         * The same binding on `hand`: what a binding of any hand is on each of its hands. A modifier with no
         * hand of its own goes with it.
         */
        InputBinding on(const Hand onHand) const
        {
            InputBinding binding = *this;
            binding.hand = onHand;
            return binding;
        }

        /**
         * Value equality across all fields (hand, activation type, button/axis, modifier, and timing).
         */
        bool operator==(const InputBinding&) const = default;
    };
}
