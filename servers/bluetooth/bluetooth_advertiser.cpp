/**************************************************************************/
/*  bluetooth_advertiser.cpp                                              */
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

#include "bluetooth_advertiser.h"

void BluetoothAdvertiser::_bind_methods() {
	// The setters prefixed with _ are only exposed so we can have feeds through GDExtension!
	// They should not be called by the end user.

	ClassDB::bind_method(D_METHOD("get_id"), &BluetoothAdvertiser::get_id);

	ClassDB::bind_method(D_METHOD("is_active"), &BluetoothAdvertiser::is_active);
	ClassDB::bind_method(D_METHOD("set_active", "active"), &BluetoothAdvertiser::set_active);

	ClassDB::bind_method(D_METHOD("get_name"), &BluetoothAdvertiser::get_name);
	ClassDB::bind_method(D_METHOD("_set_name", "name"), &BluetoothAdvertiser::set_name);

	ADD_GROUP("Advertiser", "advertiser_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "advertiser_is_active"), "set_active", "is_active");
}

int BluetoothAdvertiser::get_id() const {
	return id;
}

bool BluetoothAdvertiser::is_active() const {
	return active;
}

void BluetoothAdvertiser::set_active(bool p_is_active) {
	if (p_is_active == active) {
		// all good
	} else if (p_is_active) {
		if (start_advertising()) {
			print_line("Activate " + name);
			active = true;
		}
	} else {
		// just deactivate it
		stop_advertising();
		print_line("Deactivate " + name);
		active = false;
	}
}

String BluetoothAdvertiser::get_name() const {
	return name;
}

void BluetoothAdvertiser::set_name(String p_name) {
	name = p_name;
}

BluetoothAdvertiser::BluetoothAdvertiser() {
	// initialize us
	id = BluetoothServer::get_singleton()->get_free_advertiser_id();
	name = "???";
	active = false;
}

BluetoothAdvertiser::BluetoothAdvertiser(String p_name) {
	// initialize us
	id = BluetoothServer::get_singleton()->get_free_advertiser_id();
	name = p_name;
	active = false;
}

BluetoothAdvertiser::~BluetoothAdvertiser() {
}

bool BluetoothAdvertiser::start_advertising() {
	// nothing to do here
	return true;
}

void BluetoothAdvertiser::stop_advertising() {
	// nothing to do here
}
