/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "register_types.h"

#include "bluetooth_advertiser.h"
#include "bluetooth_enumerator.h"

#if defined(MACOS_ENABLED)
#include "bluetooth_advertiser_macos.h"
#include "bluetooth_enumerator_macos.h"
#endif
/*#if defined(ANDROID_ENABLED)
#include "bluetooth_advertiser_android.h"
#include "bluetooth_enumerator_android.h"
#endif*/

#include "core/config/engine.h"
#include "core/os/os.h"

void initialize_bluetooth_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		ClassDB::register_custom_instance_class<BluetoothAdvertiser>();
		ClassDB::register_custom_instance_class<BluetoothEnumerator>();
	}
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE || Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#if defined(MACOS_ENABLED)
	BluetoothAdvertiserMacOS::initialize();
	BluetoothEnumeratorMacOS::initialize();
#endif
/*#if defined(ANDROID_ENABLED)
	BluetoothAdvertiserAndroid::initialize();
	BluetoothEnumeratorAndroid::initialize();
#endif*/
    print_line(String((std::string("Bluetooth module enabled: ")+std::to_string(OS::get_singleton()->has_feature("bluetooth_module"))).c_str()));
}

void uninitialize_bluetooth_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE || Engine::get_singleton()->is_editor_hint()) {
		return;
	}
#if defined(MACOS_ENABLED)
	BluetoothAdvertiserMacOS::deinitialize();
	BluetoothEnumeratorMacOS::deinitialize();
#endif
/*#if defined(ANDROID_ENABLED)
	BluetoothAdvertiserAndroid::deinitialize();
	BluetoothEnumeratorAndroid::deinitialize();
#endif*/
}
