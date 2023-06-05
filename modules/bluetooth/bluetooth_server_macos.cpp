/**************************************************************************/
/*  bluetooth_server_macos.cpp                                            */
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

#include "bluetooth_server_macos.h"
#include "bluetooth_advertiser_macos.h"
#include "bluetooth_enumerator_macos.h"
#include "core/config/engine.h"

BluetoothServerMacOS::BluetoothServerMacOS() {
}

bool BluetoothServerMacOS::is_supported() const {
    return true;
}

Ref<BluetoothAdvertiser> BluetoothServerMacOS::new_advertiser() {
    if (Engine::get_singleton()->is_editor_hint() || Engine::get_singleton()->is_project_manager_hint()) {
        return nullptr;
    }
    int count = get_advertiser_count();
    Ref<BluetoothAdvertiserMacOS> advertiser;
    advertiser.instantiate();
    add_advertiser(advertiser);
    return get_advertiser(count);
}

Ref<BluetoothEnumerator> BluetoothServerMacOS::new_enumerator() {
    if (Engine::get_singleton()->is_editor_hint() || Engine::get_singleton()->is_project_manager_hint()) {
        return nullptr;
    }
    int count = get_enumerator_count();
    Ref<BluetoothEnumeratorMacOS> enumerator;
    enumerator.instantiate();
    add_enumerator(enumerator);
    return get_enumerator(count);
}
