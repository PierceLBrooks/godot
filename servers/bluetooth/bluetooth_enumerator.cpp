/**************************************************************************/
/*  bluetooth_enumerator.cpp                                              */
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

#include "bluetooth_enumerator.h"
#include "core/variant/typed_array.h"
#include "core/core_bind.h"
#include "core/os/thread.h"
#include "core/object/script_language.h"

#include <chrono>

void BluetoothEnumerator::_bind_methods() {
	// The setters prefixed with _ are only exposed so we can have enumerators through GDExtension!
	// They should not be called by the end user.

	ClassDB::bind_method(D_METHOD("get_id"), &BluetoothEnumerator::get_id);

	ClassDB::bind_method(D_METHOD("is_active"), &BluetoothEnumerator::is_active);
	ClassDB::bind_method(D_METHOD("set_active", "active"), &BluetoothEnumerator::set_active);

	ADD_GROUP("Enumerator", "enumerator_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enumerator_is_active"), "set_active", "is_active");
}

int BluetoothEnumerator::get_id() const {
	return id;
}

bool BluetoothEnumerator::is_active() const {
	return active;
}

void BluetoothEnumerator::set_active(bool p_is_active) {
	if (p_is_active == active) {
		// all good
	} else if (p_is_active) {
		active = true;
		if (start_scanning()) {
			print_line("Scan");
		} else {
			print_line("Scan failure");
			active = false;
		}
	} else {
		// just deactivate it
		if (stop_scanning()) {
			print_line("Unscan");
		} else {
			print_line("Unscan failure");
		}
		active = false;
	}
	//std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(1000.0));
}

BluetoothEnumerator::BluetoothEnumerator(int p_id) {
	// initialize us
	id = p_id;
	active = false;
}

BluetoothEnumerator::BluetoothEnumerator() {
	// initialize us
	id = BluetoothServer::get_singleton()->get_free_enumerator_id();
	active = false;
}

BluetoothEnumerator::~BluetoothEnumerator() {
	// nothing to do here
}

bool BluetoothEnumerator::start_scanning() const {
	// nothing to do here
	return false;
}

bool BluetoothEnumerator::stop_scanning() const {
	// nothing to do here
	return false;
}

void BluetoothEnumerator::on_register() const {
	// nothing to do here
}

void BluetoothEnumerator::on_unregister() const {
	// nothing to do here
}

bool BluetoothEnumerator::can_emit_signal(const StringName &p_name) const {
	List<Connection> conns;
	get_signal_connection_list(p_name, &conns);
	if (conns.size() > 0) {
		return true;
	}
	return false;
}

Ref<BluetoothEnumerator>* BluetoothEnumerator::null_enumerator = new Ref<BluetoothEnumerator>(new BluetoothEnumerator(-1));
