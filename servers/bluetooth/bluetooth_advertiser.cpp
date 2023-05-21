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
#include "core/variant/typed_array.h"
#include "core/os/thread.h"
#include "core/object/script_language.h"

#include <chrono>

void BluetoothAdvertiser::_bind_methods() {
	// The setters prefixed with _ are only exposed so we can have advertisers through GDExtension!
	// They should not be called by the end user.

	ClassDB::bind_method(D_METHOD("get_id"), &BluetoothAdvertiser::get_id);

	ClassDB::bind_method(D_METHOD("is_active"), &BluetoothAdvertiser::is_active);
	ClassDB::bind_method(D_METHOD("set_active", "active"), &BluetoothAdvertiser::set_active);

	ClassDB::bind_method(D_METHOD("get_service_uuid"), &BluetoothAdvertiser::get_service_uuid);
	ClassDB::bind_method(D_METHOD("set_service_uuid", "service_uuid"), &BluetoothAdvertiser::set_service_uuid);

	ClassDB::bind_method(D_METHOD("get_characteristic", "index"), &BluetoothAdvertiser::get_characteristic);
	ClassDB::bind_method(D_METHOD("get_characteristic_count"), &BluetoothAdvertiser::get_characteristic_count);
	ClassDB::bind_method(D_METHOD("characteristics"), &BluetoothAdvertiser::get_characteristics);

	ClassDB::bind_method(D_METHOD("add_characteristic", "characteristic_uuid"), &BluetoothAdvertiser::add_characteristic);
	ClassDB::bind_method(D_METHOD("remove_characteristic", "index"), &BluetoothAdvertiser::remove_characteristic);

	ClassDB::bind_method(D_METHOD("respond_characteristic_read_request", "characteristic_uuid", "response", "request"), &BluetoothAdvertiser::respond_characteristic_read_request);

	ADD_GROUP("Advertiser", "advertiser_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "advertiser_is_active"), "set_active", "is_active");

	ADD_SIGNAL(MethodInfo("bluetooth_service_advertisement_started", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "uuid")));
	ADD_SIGNAL(MethodInfo("bluetooth_service_advertisement_stopped", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "uuid")));
	ADD_SIGNAL(MethodInfo("bluetooth_service_characteristic_read", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "uuid"), PropertyInfo(Variant::INT, "request")));
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
		active = true;
		if (start_advertising()) {
			print_line("Advertise " + service_uuid);
		} else {
			print_line("Advertise failure "+ service_uuid);
			active = false;
		}
	} else {
		// just deactivate it
		if (stop_advertising()) {
			print_line("Deadvertise " + service_uuid);
		} else {
			print_line("Deadvertise failure " + service_uuid);
		}
		active = false;
	}
	//std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(1000.0));
}

String BluetoothAdvertiser::get_service_uuid() const {
	return service_uuid;
}

void BluetoothAdvertiser::set_service_uuid(String p_service_uuid) {
	service_uuid = p_service_uuid;
}

void BluetoothAdvertiser::add_characteristic(String p_characteristic_uuid) {
	if (has_characteristic(p_characteristic_uuid)) {
		return;
	}

	// add our characteristic
	Ref<BluetoothAdvertiser::BluetoothAdvertiserCharacteristic> characteristic;
	characteristic.instantiate();
	characteristic->uuid = p_characteristic_uuid;
	characteristics.push_back(characteristic);
}

void BluetoothAdvertiser::remove_characteristic(int p_index) {
	ERR_FAIL_INDEX(p_index, characteristics.size());

	characteristics.remove_at(p_index);
}

Ref<BluetoothAdvertiser::BluetoothAdvertiserCharacteristic> BluetoothAdvertiser::get_characteristic_by_uuid(String p_characteristic_uuid) const {
	for (int i = 0; i < characteristics.size(); i++) {
		Ref<BluetoothAdvertiser::BluetoothAdvertiserCharacteristic> characteristic = characteristics[i];
		if (characteristic->uuid == p_characteristic_uuid) {
			return characteristic;
		}
	}
	return nullptr;
}

String BluetoothAdvertiser::get_characteristic(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, get_characteristic_count(), "");

	return characteristics[p_index]->uuid;
}

int BluetoothAdvertiser::get_characteristic_count() const {
	return characteristics.size();
}

TypedArray<String> BluetoothAdvertiser::get_characteristics() const {
	TypedArray<String> return_characteristics;
	int count = get_characteristic_count();
	return_characteristics.resize(count);

	for (int i = 0; i < characteristics.size(); i++) {
		return_characteristics[i] = get_characteristic(i);
	}

	return return_characteristics;
}

bool BluetoothAdvertiser::has_characteristic(String p_characteristic_uuid) const {
	for (int i = 0; i < characteristics.size(); i++) {
		if (get_characteristic(i) == p_characteristic_uuid) {
			return true;
		}
	}
	return false;
}

BluetoothAdvertiser::BluetoothAdvertiserCharacteristic::BluetoothAdvertiserCharacteristic() {
	// initialize us
	uuid = "";
	permission = false;
	readRequest = -1;
}

BluetoothAdvertiser::BluetoothAdvertiserCharacteristic::BluetoothAdvertiserCharacteristic(String p_characteristic_uuid) {
	// initialize us
	uuid = p_characteristic_uuid;
	permission = false;
	readRequest = -1;
}

BluetoothAdvertiser::BluetoothAdvertiser(int p_id) {
	// initialize us
	id = p_id;
	service_uuid = "";
	active = false;
}

BluetoothAdvertiser::BluetoothAdvertiser() {
	// initialize us
	id = BluetoothServer::get_singleton()->get_free_advertiser_id();
	service_uuid = "";
	active = false;
}

BluetoothAdvertiser::BluetoothAdvertiser(String p_service_uuid) {
	// initialize us
	id = BluetoothServer::get_singleton()->get_free_advertiser_id();
	service_uuid = p_service_uuid;
	active = false;
}

BluetoothAdvertiser::~BluetoothAdvertiser() {
	// nothing to do here
}

bool BluetoothAdvertiser::start_advertising() const {
	// nothing to do here
	return false;
}

bool BluetoothAdvertiser::stop_advertising() const {
	// nothing to do here
	return false;
}

void BluetoothAdvertiser::respond_characteristic_read_request(String p_characteristic_uuid, String p_response, int p_request) const {
	// nothing to do here
}

void BluetoothAdvertiser::on_register() const {
	// nothing to do here
}

void BluetoothAdvertiser::on_unregister() const {
	// nothing to do here
}

bool BluetoothAdvertiser::on_start() const {
	if (can_emit_signal(SNAME("bluetooth_service_advertisement_started"))) {
		Ref<BluetoothAdvertiser>* reference = new Ref<BluetoothAdvertiser>(const_cast<BluetoothAdvertiser*>(this));
		Thread thread;
		thread.start([](void *p_udata) {
			Ref<BluetoothAdvertiser>* advertiser = static_cast<Ref<BluetoothAdvertiser>*>(p_udata);
			(*advertiser)->emit_signal(SNAME("bluetooth_service_advertisement_started"), (*advertiser)->get_id(), (*advertiser)->get_service_uuid());
		}, reference);
		thread.wait_to_finish();
		delete reference;
		return true;
	}
	return false;
}

bool BluetoothAdvertiser::on_stop() const {
	if (can_emit_signal(SNAME("bluetooth_service_advertisement_stopped"))) {
		Ref<BluetoothAdvertiser>* reference = new Ref<BluetoothAdvertiser>(const_cast<BluetoothAdvertiser*>(this));
		Thread thread;
		thread.start([](void *p_udata) {
			Ref<BluetoothAdvertiser>* advertiser = static_cast<Ref<BluetoothAdvertiser>*>(p_udata);
			(*advertiser)->emit_signal(SNAME("bluetooth_service_advertisement_stopped"), (*advertiser)->get_id(), (*advertiser)->get_service_uuid());
		}, reference);
		thread.wait_to_finish();
		delete reference;
		return true;
	}
	return false;
}

bool BluetoothAdvertiser::on_read(String p_characteristic_uuid, int p_request) const {
	if (!has_characteristic(p_characteristic_uuid)) {
		return false;
	}
	if (can_emit_signal(SNAME("bluetooth_service_characteristic_read"))) {
		Ref<BluetoothAdvertiser::BluetoothAdvertiserCharacteristic>* reference = new Ref<BluetoothAdvertiser::BluetoothAdvertiserCharacteristic>(get_characteristic_by_uuid(p_characteristic_uuid));
		Thread thread;
		(*reference)->readRequest = p_request;
		(*reference)->advertiser = Ref<BluetoothAdvertiser>(const_cast<BluetoothAdvertiser*>(this));
		thread.start([](void *p_udata) {
			Ref<BluetoothAdvertiser::BluetoothAdvertiserCharacteristic>* characteristic = static_cast<Ref<BluetoothAdvertiser::BluetoothAdvertiserCharacteristic>*>(p_udata);
			(*characteristic)->advertiser->emit_signal(SNAME("bluetooth_service_characteristic_read"), (*characteristic)->advertiser->get_id(), (*characteristic)->uuid, (*characteristic)->readRequest);
		}, reference);
		thread.wait_to_finish();
		(*reference)->advertiser = *BluetoothAdvertiser::null_advertiser;
		(*reference)->readRequest = -1;
		delete reference;
		return true;
	}
	return false;
}

bool BluetoothAdvertiser::can_emit_signal(const StringName &p_name) const {
	List<Connection> conns;
	get_signal_connection_list(p_name, &conns);
	if (conns.size() > 0) {
		return true;
	}
	return false;
}

Ref<BluetoothAdvertiser>* BluetoothAdvertiser::null_advertiser = new Ref<BluetoothAdvertiser>(new BluetoothAdvertiser(-1));
