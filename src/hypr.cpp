#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>

#include "server.h"
#include "shared.h"

inline HANDLE PHANDLE = nullptr;

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

inline CFunctionHook* g_pOnKeyboardKey = nullptr;
typedef void (*origOnKeyboardKey)(CInputManager*, const IKeyboard::SKeyEvent&, SP<IKeyboard>);
inline CFunctionHook* g_pOnMouseButton = nullptr;
typedef void (*origOnMouseButton)(CInputManager*, IPointer::SButtonEvent);

void hkOnKeyboardKey(CInputManager* man, const IKeyboard::SKeyEvent& ev, SP<IKeyboard> keeb) {
	// HyprlandAPI::addNotification(PHANDLE, "[hypr_bongocat] keyPressed", CHyprColor{0.2, 0.15, 0.4, 1.0}, 5000);
	if (ev.state != WL_KEYBOARD_KEY_STATE_RELEASED) {
		Packet iut;
		iut = 1;
		broadcast(&iut, sizeof(iut));
	}

    return (*(origOnKeyboardKey)g_pOnKeyboardKey->m_original)(man, ev, keeb);
}

void hkOnMouseButton(CInputManager* man, IPointer::SButtonEvent ev) {
	// HyprlandAPI::addNotification(PHANDLE, "[hypr_bongocat] keyPressed", CHyprColor{0.2, 0.15, 0.4, 1.0}, 5000);
	if (ev.state != WL_POINTER_BUTTON_STATE_RELEASED) {
		Packet iut;
		iut = 2;
		broadcast(&iut, sizeof(iut));
	}

    return (*(origOnMouseButton)g_pOnMouseButton->m_original)(man, ev);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hypr_bongocat] Mismatched headers! Can't proceed.",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hypr_bongocat] Version mismatch");
    }

	server_start();

	{
		static const auto kbkm = HyprlandAPI::findFunctionsByName(PHANDLE, "onKeyboardKey");
		g_pOnKeyboardKey = HyprlandAPI::createFunctionHook(handle, kbkm[0].address, (void*)&hkOnKeyboardKey);
		g_pOnKeyboardKey->hook();
		static const auto ombm = HyprlandAPI::findFunctionsByName(PHANDLE, "onMouseButton");
		g_pOnMouseButton = HyprlandAPI::createFunctionHook(handle, ombm[0].address, (void*)&hkOnMouseButton);
		g_pOnMouseButton->hook();
	}

	// HyprlandAPI::addNotification(PHANDLE, "[hypr_bongocat] hi", CHyprColor{0.2, 0.15, 0.4, 1.0}, 5000);
    return {"hypr_bongocat", "Bongocat linux hyprland plugin", "int4_t", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
	server_shutdown();
}
