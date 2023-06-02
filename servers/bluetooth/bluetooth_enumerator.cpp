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

	ClassDB::bind_method(D_METHOD("get_sought_service", "index"), &BluetoothEnumerator::get_sought_service);
	ClassDB::bind_method(D_METHOD("get_sought_service_count"), &BluetoothEnumerator::get_sought_service_count);
	ClassDB::bind_method(D_METHOD("sought_services"), &BluetoothEnumerator::get_sought_services);

	ClassDB::bind_method(D_METHOD("add_sought_service", "service_uuid"), &BluetoothEnumerator::add_sought_service);
	ClassDB::bind_method(D_METHOD("remove_sought_service", "index"), &BluetoothEnumerator::remove_sought_service);

	ClassDB::bind_method(D_METHOD("has_sought_service", "service_uuid"), &BluetoothEnumerator::has_sought_service);

	ADD_GROUP("Enumerator", "enumerator_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enumerator_is_active"), "set_active", "is_active");

	ADD_SIGNAL(MethodInfo("bluetooth_service_enumeration_started", PropertyInfo(Variant::INT, "id")));
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
		if (start_scanning()) {
			print_line("Scan");
			active = true;
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

void BluetoothEnumerator::add_sought_service(String p_service_uuid) {
	if (has_sought_service(p_service_uuid)) {
		return;
	}

	// add our service
	sought_services.push_back(p_service_uuid);
}

void BluetoothEnumerator::remove_sought_service(int p_index) {
	ERR_FAIL_INDEX(p_index, sought_services.size());

	sought_services.remove_at(p_index);
}


int BluetoothEnumerator::get_sought_service_count() const {
	return sought_services.size();
}

String BluetoothEnumerator::get_sought_service(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, get_sought_service_count(), "");

	return sought_services[p_index];
}

TypedArray<String> BluetoothEnumerator::get_sought_services() const {
	TypedArray<String> return_characteristics;
	int count = get_sought_service_count();
	return_characteristics.resize(count);

	for (int i = 0; i < count; i++) {
		return_characteristics[i] = get_sought_service(i);
	}

	return return_characteristics;
}

bool BluetoothEnumerator::has_sought_service(String p_service_uuid) const {
	for (int i = 0; i < sought_services.size(); i++) {
		if (get_sought_service(i) == p_service_uuid) {
			return true;
		}
	}
	return false;
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

bool BluetoothEnumerator::on_start() const {
	if (can_emit_signal(SNAME("bluetooth_service_enumeration_started"))) {
		Ref<BluetoothEnumerator>* reference = new Ref<BluetoothEnumerator>(const_cast<BluetoothEnumerator*>(this));
		if (reference->is_valid()) {
			Thread thread;
			thread.start([](void *p_udata) {
				Ref<BluetoothEnumerator>* enumerator = static_cast<Ref<BluetoothEnumerator>*>(p_udata);
				if (enumerator->is_valid()) {
					(*enumerator)->emit_signal(SNAME("bluetooth_service_enumeration_started"), (*enumerator)->get_id());
				}
			}, reference);
			thread.wait_to_finish();
		}
		delete reference;
		return true;
	}
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
