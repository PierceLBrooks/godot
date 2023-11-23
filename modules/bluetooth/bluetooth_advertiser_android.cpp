/**************************************************************************/
/*  bluetooth_advertiser_android.cpp                                      */
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

#include "bluetooth_advertiser_android.h"
#include "core/config/engine.h"
#include "core/core_bind.h"
#include "core/variant/typed_array.h"
#include "platform/android/bluetooth_android.h"

BluetoothAdvertiserAndroid::BluetoothAdvertiserAndroid() {
    id = BluetoothAndroid::get_singleton()->register_advertiser(this);
}

BluetoothAdvertiserAndroid::~BluetoothAdvertiserAndroid() {
    BluetoothAndroid::get_singleton()->unregister_advertiser(id);
}

void BluetoothAdvertiserAndroid::initialize() {
    BluetoothAdvertiser::_create = BluetoothAdvertiserAndroid::_create;
}

void BluetoothAdvertiserAndroid::deinitialize() {
	// nothing to do here
}

void BluetoothAdvertiserAndroid::respond_characteristic_read_request(String p_characteristic_uuid, String p_response, int p_request) const {
}

void BluetoothAdvertiserAndroid::respond_characteristic_write_request(String p_characteristic_uuid, String p_response, int p_request) const {
}

bool BluetoothAdvertiserAndroid::start_advertising() const {
    if (id == -1) {
        print_line("Registration failure");
        return false;
    }
    if (!BluetoothAndroid::is_supported()) {
        return false;
    }
    return BluetoothAndroid::start_advertising(id);
}

bool BluetoothAdvertiserAndroid::stop_advertising() const {
    if (id == -1) {
        print_line("Unregistration failure");
        return false;
    }
    if (!BluetoothAndroid::is_supported()) {
        return false;
    }
    return BluetoothAndroid::stop_advertising(id);
}

void BluetoothAdvertiserAndroid::on_register() const {
	// nothing to do here
}

void BluetoothAdvertiserAndroid::on_unregister() const {
	// nothing to do here
}

