/**************************************************************************/
/*  bluetooth_enumerator_android.cpp                                      */
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

#include "bluetooth_enumerator_android.h"
#include "core/config/engine.h"
#include "core/core_bind.h"
#include "core/variant/typed_array.h"
#include "platform/android/bluetooth_android.h"

BluetoothEnumeratorAndroid::BluetoothEnumeratorAndroid() {
    id = BluetoothAndroid::get_singleton()->register_enumerator(this);
}

BluetoothEnumeratorAndroid::~BluetoothEnumeratorAndroid() {
    BluetoothAndroid::get_singleton()->unregister_enumerator(id);
}

void BluetoothEnumeratorAndroid::initialize() {
    BluetoothEnumerator::_create = BluetoothEnumeratorAndroid::_create;
}

void BluetoothEnumeratorAndroid::deinitialize() {
	// nothing to do here
}

bool BluetoothEnumeratorAndroid::start_scanning() const {
    if (id == -1) {
        print_line("Registration failure");
        return false;
    }
    if (!BluetoothAndroid::is_supported()) {
        return false;
    }
    return BluetoothAndroid::start_scanning(id);
}

bool BluetoothEnumeratorAndroid::stop_scanning() const {
    if (id == -1) {
        print_line("Unregistration failure");
        return false;
    }
    if (!BluetoothAndroid::is_supported()) {
        return false;
    }
    return BluetoothAndroid::stop_scanning(id);
}

void BluetoothEnumeratorAndroid::connect_peer(String p_peer_uuid) {
    BluetoothAndroid::connect_peer(id, p_peer_uuid);
}

void BluetoothEnumeratorAndroid::read_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) {
}

void BluetoothEnumeratorAndroid::write_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value) {
}

void BluetoothEnumeratorAndroid::on_register() const {
	// nothing to do here
}

void BluetoothEnumeratorAndroid::on_unregister() const {
	// nothing to do here
}


