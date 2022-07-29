#include "KeyboardInputManager.h"

KeyboardInputManager::KeyboardInputManager()
{
    // init input
    DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)(&mpInput), nullptr);
    mpInput->CreateDevice(GUID_SysKeyboard, &mpKeyboard, nullptr);
    mpKeyboard->SetDataFormat(&c_dfDIKeyboard);
    mpKeyboard->Acquire();
}

void KeyboardInputManager::processInput()
{
    HRESULT result = mpKeyboard->GetDeviceState(256, mpKeyboardState);

}