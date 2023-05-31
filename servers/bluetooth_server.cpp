/**************************************************************************/
/*  bluetooth_server.cpp                                                  */
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

#include "bluetooth_server.h"
#include "core/variant/typed_array.h"
#include "servers/bluetooth/bluetooth_advertiser.h"
#include "servers/bluetooth/bluetooth_enumerator.h"

////////////////////////////////////////////////////////
// BluetoothServer

BluetoothServer::CreateFunc BluetoothServer::create_func = nullptr;

void BluetoothServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_advertiser_by_id", "id"), &BluetoothServer::get_advertiser_by_id);
	ClassDB::bind_method(D_METHOD("get_advertiser", "index"), &BluetoothServer::get_advertiser);
	ClassDB::bind_method(D_METHOD("get_advertiser_count"), &BluetoothServer::get_advertiser_count);
	ClassDB::bind_method(D_METHOD("advertisers"), &BluetoothServer::get_advertisers);

	ClassDB::bind_method(D_METHOD("add_advertiser", "advertiser"), &BluetoothServer::add_advertiser);
	ClassDB::bind_method(D_METHOD("remove_advertiser", "advertiser"), &BluetoothServer::remove_advertiser);

	ClassDB::bind_method(D_METHOD("new_advertiser"), &BluetoothServer::new_advertiser);

	ADD_SIGNAL(MethodInfo("bluetooth_advertiser_added", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("bluetooth_advertiser_removed", PropertyInfo(Variant::INT, "id")));

	ClassDB::bind_method(D_METHOD("get_enumerator_by_id", "id"), &BluetoothServer::get_enumerator_by_id);
	ClassDB::bind_method(D_METHOD("get_enumerator", "index"), &BluetoothServer::get_enumerator);
	ClassDB::bind_method(D_METHOD("get_enumerator_count"), &BluetoothServer::get_enumerator_count);
	ClassDB::bind_method(D_METHOD("enumerators"), &BluetoothServer::get_enumerators);

	ClassDB::bind_method(D_METHOD("add_enumerator", "enumerator"), &BluetoothServer::add_enumerator);
	ClassDB::bind_method(D_METHOD("remove_enumerator", "enumerator"), &BluetoothServer::remove_enumerator);

	ClassDB::bind_method(D_METHOD("new_enumerator"), &BluetoothServer::new_enumerator);

	ADD_SIGNAL(MethodInfo("bluetooth_enumerator_added", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("bluetooth_enumerator_removed", PropertyInfo(Variant::INT, "id")));
}

BluetoothServer *BluetoothServer::singleton = nullptr;

BluetoothServer *BluetoothServer::get_singleton() {
	return singleton;
}

int BluetoothServer::get_free_advertiser_id() {
	bool id_exists = true;
	int newid = 0;

	// find a free id
	while (id_exists) {
		newid++;
		id_exists = false;
		for (int i = 0; i < advertisers.size() && !id_exists; i++) {
			if (advertisers[i]->get_id() == newid) {
				id_exists = true;
			}
		}
	}

	return newid;
}

int BluetoothServer::get_advertiser_index(int p_id) const {
	for (int i = 0; i < advertisers.size(); i++) {
		if (advertisers[i]->get_id() == p_id) {
			return i;
		}
	}

	return -1;
}

Ref<BluetoothAdvertiser> BluetoothServer::get_advertiser_by_id(int p_id) const {
	int index = get_advertiser_index(p_id);

	if (index < 0) {
		return nullptr;
	}

	return advertisers[index];
}

Ref<BluetoothAdvertiser> BluetoothServer::new_advertiser() {
	// nothing to do here
	return nullptr;
}

void BluetoothServer::add_advertiser(const Ref<BluetoothAdvertiser> &p_advertiser) {
	if (!get_advertiser_by_id(p_advertiser->get_id()).is_null()) {
		return;
	}

	ERR_FAIL_COND(p_advertiser.is_null());

	// add our advertiser
	advertisers.push_back(p_advertiser);

	print_verbose("BluetoothServer: Registered advertiser \"" + p_advertiser->get_service_uuid() + "\" with ID " + itos(p_advertiser->get_id()) + " at index " + itos(advertisers.size() - 1));

	// let whomever is interested know
	p_advertiser->on_register();
	emit_signal(SNAME("bluetooth_advertiser_added"), p_advertiser->get_id());
}

void BluetoothServer::remove_advertiser(const Ref<BluetoothAdvertiser> &p_advertiser) {
	for (int i = 0; i < advertisers.size(); i++) {
		if (advertisers[i] == p_advertiser) {
			int advertiser_id = p_advertiser->get_id();

			print_verbose("BluetoothServer: Removed advertiser \"" + p_advertiser->get_service_uuid() + "\" with ID " + itos(advertiser_id));

			// remove it from our array, if this results in our advertiser being unreferenced it will be destroyed
			advertisers.remove_at(i);

			// let whomever is interested know
			p_advertiser->on_unregister();
			emit_signal(SNAME("bluetooth_advertiser_removed"), advertiser_id);
			return;
		}
	}
}

Ref<BluetoothAdvertiser> BluetoothServer::get_advertiser(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, advertisers.size(), nullptr);

	return advertisers[p_index];
}

int BluetoothServer::get_advertiser_count() const {
	return advertisers.size();
}

TypedArray<BluetoothAdvertiser> BluetoothServer::get_advertisers() const {
	TypedArray<BluetoothAdvertiser> return_advertisers;
	int cc = get_advertiser_count();
	return_advertisers.resize(cc);

	for (int i = 0; i < advertisers.size(); i++) {
		return_advertisers[i] = get_advertiser(i);
	}

	return return_advertisers;
}

int BluetoothServer::get_free_enumerator_id() {
	bool id_exists = true;
	int newid = 0;

	// find a free id
	while (id_exists) {
		newid++;
		id_exists = false;
		for (int i = 0; i < enumerators.size() && !id_exists; i++) {
			if (enumerators[i]->get_id() == newid) {
				id_exists = true;
			}
		}
	}

	return newid;
}

int BluetoothServer::get_enumerator_index(int p_id) const {
	for (int i = 0; i < enumerators.size(); i++) {
		if (enumerators[i]->get_id() == p_id) {
			return i;
		}
	}

	return -1;
}

Ref<BluetoothEnumerator> BluetoothServer::get_enumerator_by_id(int p_id) const {
	int index = get_enumerator_index(p_id);

	if (index < 0) {
		return nullptr;
	}

	return enumerators[index];
}

Ref<BluetoothEnumerator> BluetoothServer::new_enumerator() {
	// nothing to do here
	return nullptr;
}

void BluetoothServer::add_enumerator(const Ref<BluetoothEnumerator> &p_enumerator) {
	if (!get_enumerator_by_id(p_enumerator->get_id()).is_null()) {
		return;
	}

	ERR_FAIL_COND(p_enumerator.is_null());

	// add our enumerator
	enumerators.push_back(p_enumerator);

	print_verbose("BluetoothServer: Registered enumerator with ID " + itos(p_enumerator->get_id()) + " at index " + itos(enumerators.size() - 1));

	// let whomever is interested know
	p_enumerator->on_register();
	emit_signal(SNAME("bluetooth_enumerator_added"), p_enumerator->get_id());
}

void BluetoothServer::remove_enumerator(const Ref<BluetoothEnumerator> &p_enumerator) {
	for (int i = 0; i < enumerators.size(); i++) {
		if (enumerators[i] == p_enumerator) {
			int enumerator_id = p_enumerator->get_id();

			print_verbose("BluetoothServer: Removed enumerator with ID " + itos(enumerator_id));

			// remove it from our array, if this results in our enumerator being unreferenced it will be destroyed
			enumerators.remove_at(i);

			// let whomever is interested know
			p_enumerator->on_unregister();
			emit_signal(SNAME("bluetooth_enumerator_removed"), enumerator_id);
			return;
		}
	}
}

Ref<BluetoothEnumerator> BluetoothServer::get_enumerator(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, enumerators.size(), nullptr);

	return enumerators[p_index];
}

int BluetoothServer::get_enumerator_count() const {
	return enumerators.size();
}

TypedArray<BluetoothEnumerator> BluetoothServer::get_enumerators() const {
	TypedArray<BluetoothEnumerator> return_enumerators;
	int cc = get_enumerator_count();
	return_enumerators.resize(cc);

	for (int i = 0; i < enumerators.size(); i++) {
		return_enumerators[i] = get_enumerator(i);
	}

	return return_enumerators;
}

BluetoothServer::BluetoothServer() {
	singleton = this;
}

BluetoothServer::~BluetoothServer() {
	singleton = nullptr;
}
