// UWP UI HELPER
// Copyright (c) 2023 Bashar Astifan.
// Email: bashar@astifan.online
// Telegram: @basharastifan
// GitHub: https://github.com/basharast/UWP2Win32

// Functions:
// SendKeyToTextEdit(vKey, flags, state) [state->KEY_DOWN or KEY_UP]
// ShowInputKeyboard()
// HideInputKeyboard()
// IsCapsLockOn()
// IsShiftOnHold()
// IsCtrlOnHold()

#include "UIHelpers.h"
#include "NKCodeFromWindowsSystem.h"
#include "StorageExtensions.h"

using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::ViewManagement;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::UI::Notifications;

#pragma region  Input
// Input Management
UI::TextEdit* globalTextEdit = nullptr; // TextEdit to be invoked by keydown event

void SendKeyToTextEdit(int vKey, int flags, int state) {
	if (globalTextEdit != nullptr) {
		
		auto virtualKey = (VirtualKey)vKey;
		
		int keyCode;
		
		auto keyText = convert(virtualKey.ToString());
		if (keyText.size() > 1) {
			// Try to parse some known codes
			std::map<int, std::string> keysMap = {
				{ 188, "," },
			    { 190, "." },
			    { 191, "/" },
			    { 219, "[" },
			    { 221, "]" },
			    { 220, "\\" },
			    { 186, ";" },
			    { 222, "'" },
			    { 189, "-" },
			    { 187, "=" },
			    { 192, "`" },
			    { (int)VirtualKey::Divide, "/" },
			    { (int)VirtualKey::Multiply, "*" },
			    { (int)VirtualKey::Separator, "," },
			    { (int)VirtualKey::Add, "+" },
			    { (int)VirtualKey::Subtract, "-" },
			    { (int)VirtualKey::Decimal, "." },
			    { (int)VirtualKey::Space, " " },
			};

			if (IsShiftOnHold()) {
				// Reinitial map with new values when shift on hold
				keysMap = {
					{ 188, "<" },
					{ 190, ">" },
			        { 191, "?" },
			        { 219, "{" },
			        { 221, "}" },
			        { 220, "|" },
			        { 186, ":" },
			        { 222, "\"" },
			        { 189, "_" },
			        { 187, "+" },
			        { 192, "~" },
			        { (int)VirtualKey::Number0, ")" },
			        { (int)VirtualKey::Number1, "!" },
			        { (int)VirtualKey::Number2, "@" },
			        { (int)VirtualKey::Number3, "#" },
			        { (int)VirtualKey::Number4, "$" },
			        { (int)VirtualKey::Number5, "%" },
			        { (int)VirtualKey::Number6, "^" },
			        { (int)VirtualKey::Number7, "&" },
			        { (int)VirtualKey::Number8, "*" },
			        { (int)VirtualKey::Number9, "(" },
			        { (int)VirtualKey::Divide, "/" },
			        { (int)VirtualKey::Multiply, "*" },
			        { (int)VirtualKey::Separator, "," },
			        { (int)VirtualKey::Add, "+" },
			        { (int)VirtualKey::Subtract, "-" },
			        { (int)VirtualKey::Decimal, "." },
			        { (int)VirtualKey::Space, " " },
				};
			}
			auto keyIter = keysMap.find(vKey);
			if (keyIter != keysMap.end()) {
				keyText = keyIter->second;
			}
		}

		replace(keyText, "NumberPad", "");
		replace(keyText, "Number", "");
		if (keyText.size() == 1) {
			flags |= KEY_CHAR;
			auto capsLockState = CoreApplication::MainView->CoreWindow->GetKeyState(VirtualKey::CapitalLock);
			if (!IsCapsLockOn()) {
				// Transform text to lowercase
				tolower(keyText);
			}
			if (IsShiftOnHold()) {
				if (!IsCapsLockOn()) {
					// Transform text to uppercase
					toupper(keyText);
				}
				else {
					// Transform text to lowercase
					tolower(keyText);
				}
			}
		}

		auto iter = virtualKeyCodeToNKCode.find(virtualKey);
		if (iter != virtualKeyCodeToNKCode.end()) {
			keyCode = iter->second;

			std::list<int> nonCharList = { NKCODE_CTRL_LEFT , NKCODE_CTRL_RIGHT ,NKCODE_DPAD_LEFT ,NKCODE_DPAD_RIGHT ,NKCODE_MOVE_HOME ,NKCODE_PAGE_UP ,NKCODE_MOVE_END ,NKCODE_PAGE_DOWN ,NKCODE_FORWARD_DEL ,NKCODE_DEL ,NKCODE_ENTER ,NKCODE_NUMPAD_ENTER ,NKCODE_BACK ,NKCODE_ESCAPE };
			bool found = findInList(nonCharList, keyCode);
			if (found) {
				// Ignore, it will be handled by 'NativeKey()'
				return;
			}
		}
		else {
			keyCode = keyText[0];
		}

		auto oldView = UI::GetFocusedView();
		UI::SetFocusedView(globalTextEdit);
		
		KeyInput fakeKey{ DEVICE_ID_KEYBOARD, keyCode, state | flags};
		// Pass char as is
		fakeKey.keyChar = (char*)keyText.c_str();
		globalTextEdit->Key(fakeKey);
		UI::SetFocusedView(oldView);
	}
}
#pragma endregion

#pragma region Input Keyboard
void ShowInputKeyboard() {
	InputPane::GetForCurrentView()->TryShow();
}

void HideInputKeyboard() {
	InputPane::GetForCurrentView()->TryHide();
}
#pragma endregion

#pragma region Keys Status
bool IsCapsLockOn() {
	auto capsLockState = CoreApplication::MainView->CoreWindow->GetKeyState(VirtualKey::CapitalLock);
	return (capsLockState == CoreVirtualKeyStates::Locked);
}
bool IsShiftOnHold() {
	auto shiftState = CoreApplication::MainView->CoreWindow->GetKeyState(VirtualKey::Shift);
	return (shiftState == CoreVirtualKeyStates::Down);
}
bool IsCtrlOnHold() {
	auto ctrlState = CoreApplication::MainView->CoreWindow->GetKeyState(VirtualKey::Control);
	return (ctrlState == CoreVirtualKeyStates::Down);
}
#pragma endregion

#pragma region Notifications
void ShowToastNotification(std::string title, std::string message) {
	ToastNotifier^ toastNotifier = ToastNotificationManager::CreateToastNotifier();
	XmlDocument^ toastXml = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);
	XmlNodeList^ toastNodeList = toastXml->GetElementsByTagName("text");
	toastNodeList->Item(0)->AppendChild(toastXml->CreateTextNode(convert(title)));
	toastNodeList->Item(1)->AppendChild(toastXml->CreateTextNode(convert(message)));
	IXmlNode^ toastNode = toastXml->SelectSingleNode("/toast");
	XmlElement^ audio = toastXml->CreateElement("audio");
	audio->SetAttribute("src", "ms-winsoundevent:Notification.SMS");
	ToastNotification^ toast = ref new ToastNotification(toastXml);
	toastNotifier->Show(toast);
}
#pragma endregion


