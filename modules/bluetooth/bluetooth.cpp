/**************************************************************************/
/*  bluetooth.cpp                                                  */
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

#include "bluetooth.h"
#include "bluetooth_advertiser.h"
#include "bluetooth_enumerator.h"
#include "core/variant/typed_array.h"

////////////////////////////////////////////////////////
// Bluetooth

Bluetooth::CreateFunc Bluetooth::create_func = nullptr;

void Bluetooth::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_supported"), &Bluetooth::is_supported);

	ClassDB::bind_method(D_METHOD("get_advertiser_by_id", "id"), &Bluetooth::get_advertiser_by_id);
	ClassDB::bind_method(D_METHOD("get_advertiser", "index"), &Bluetooth::get_advertiser);
	ClassDB::bind_method(D_METHOD("get_advertiser_count"), &Bluetooth::get_advertiser_count);
	ClassDB::bind_method(D_METHOD("advertisers"), &Bluetooth::get_advertisers);

	ClassDB::bind_method(D_METHOD("add_advertiser", "advertiser"), &Bluetooth::add_advertiser);
	ClassDB::bind_method(D_METHOD("remove_advertiser", "advertiser"), &Bluetooth::remove_advertiser);

	ClassDB::bind_method(D_METHOD("new_advertiser"), &Bluetooth::new_advertiser);

	ADD_SIGNAL(MethodInfo("bluetooth_advertiser_added", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("bluetooth_advertiser_removed", PropertyInfo(Variant::INT, "id")));

	ClassDB::bind_method(D_METHOD("get_enumerator_by_id", "id"), &Bluetooth::get_enumerator_by_id);
	ClassDB::bind_method(D_METHOD("get_enumerator", "index"), &Bluetooth::get_enumerator);
	ClassDB::bind_method(D_METHOD("get_enumerator_count"), &Bluetooth::get_enumerator_count);
	ClassDB::bind_method(D_METHOD("enumerators"), &Bluetooth::get_enumerators);

	ClassDB::bind_method(D_METHOD("add_enumerator", "enumerator"), &Bluetooth::add_enumerator);
	ClassDB::bind_method(D_METHOD("remove_enumerator", "enumerator"), &Bluetooth::remove_enumerator);

	ClassDB::bind_method(D_METHOD("new_enumerator"), &Bluetooth::new_enumerator);

	ADD_SIGNAL(MethodInfo("bluetooth_enumerator_added", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("bluetooth_enumerator_removed", PropertyInfo(Variant::INT, "id")));
}

Bluetooth *Bluetooth::singleton = nullptr;

Bluetooth *Bluetooth::get_singleton() {
	return singleton;
}

int Bluetooth::get_free_advertiser_id() {
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

int Bluetooth::get_advertiser_index(int p_id) const {
	for (int i = 0; i < advertisers.size(); i++) {
		if (advertisers[i]->get_id() == p_id) {
			return i;
		}
	}

	return -1;
}

Ref<BluetoothAdvertiser> Bluetooth::get_advertiser_by_id(int p_id) const {
	int index = get_advertiser_index(p_id);

	if (index < 0) {
		return nullptr;
	}

	return advertisers[index];
}

Ref<BluetoothAdvertiser> Bluetooth::new_advertiser() {
	// nothing to do here
	return nullptr;
}

void Bluetooth::add_advertiser(const Ref<BluetoothAdvertiser> &p_advertiser) {
	if (!get_advertiser_by_id(p_advertiser->get_id()).is_null()) {
		return;
	}

	ERR_FAIL_COND(p_advertiser.is_null());

	// add our advertiser
	advertisers.push_back(p_advertiser);

	print_verbose("Bluetooth: Registered advertiser \"" + p_advertiser->get_service_uuid() + "\" with ID " + itos(p_advertiser->get_id()) + " at index " + itos(advertisers.size() - 1));

	// let whomever is interested know
	p_advertiser->on_register();
	emit_signal(SNAME("bluetooth_advertiser_added"), p_advertiser->get_id());
}

void Bluetooth::remove_advertiser(const Ref<BluetoothAdvertiser> &p_advertiser) {
	for (int i = 0; i < advertisers.size(); i++) {
		if (advertisers[i] == p_advertiser) {
			int advertiser_id = p_advertiser->get_id();

			print_verbose("Bluetooth: Removed advertiser \"" + p_advertiser->get_service_uuid() + "\" with ID " + itos(advertiser_id));

			// remove it from our array, if this results in our advertiser being unreferenced it will be destroyed
			advertisers.remove_at(i);

			// let whomever is interested know
			p_advertiser->on_unregister();
			emit_signal(SNAME("bluetooth_advertiser_removed"), advertiser_id);
			return;
		}
	}
}

Ref<BluetoothAdvertiser> Bluetooth::get_advertiser(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, advertisers.size(), nullptr);

	return advertisers[p_index];
}

int Bluetooth::get_advertiser_count() const {
	return advertisers.size();
}

TypedArray<BluetoothAdvertiser> Bluetooth::get_advertisers() const {
	TypedArray<BluetoothAdvertiser> return_advertisers;
	int cc = get_advertiser_count();
	return_advertisers.resize(cc);

	for (int i = 0; i < advertisers.size(); i++) {
		return_advertisers[i] = get_advertiser(i);
	}

	return return_advertisers;
}

int Bluetooth::get_free_enumerator_id() {
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

int Bluetooth::get_enumerator_index(int p_id) const {
	for (int i = 0; i < enumerators.size(); i++) {
		if (enumerators[i]->get_id() == p_id) {
			return i;
		}
	}

	return -1;
}

Ref<BluetoothEnumerator> Bluetooth::get_enumerator_by_id(int p_id) const {
	int index = get_enumerator_index(p_id);

	if (index < 0) {
		return nullptr;
	}

	return enumerators[index];
}

Ref<BluetoothEnumerator> Bluetooth::new_enumerator() {
	// nothing to do here
	return nullptr;
}

void Bluetooth::add_enumerator(const Ref<BluetoothEnumerator> &p_enumerator) {
	if (!get_enumerator_by_id(p_enumerator->get_id()).is_null()) {
		return;
	}

	ERR_FAIL_COND(p_enumerator.is_null());

	// add our enumerator
	enumerators.push_back(p_enumerator);

	print_verbose("Bluetooth: Registered enumerator with ID " + itos(p_enumerator->get_id()) + " at index " + itos(enumerators.size() - 1));

	// let whomever is interested know
	p_enumerator->on_register();
	emit_signal(SNAME("bluetooth_enumerator_added"), p_enumerator->get_id());
}

void Bluetooth::remove_enumerator(const Ref<BluetoothEnumerator> &p_enumerator) {
	for (int i = 0; i < enumerators.size(); i++) {
		if (enumerators[i] == p_enumerator) {
			int enumerator_id = p_enumerator->get_id();

			print_verbose("Bluetooth: Removed enumerator with ID " + itos(enumerator_id));

			// remove it from our array, if this results in our enumerator being unreferenced it will be destroyed
			enumerators.remove_at(i);

			// let whomever is interested know
			p_enumerator->on_unregister();
			emit_signal(SNAME("bluetooth_enumerator_removed"), enumerator_id);
			return;
		}
	}
}

Ref<BluetoothEnumerator> Bluetooth::get_enumerator(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, enumerators.size(), nullptr);

	return enumerators[p_index];
}

int Bluetooth::get_enumerator_count() const {
	return enumerators.size();
}

TypedArray<BluetoothEnumerator> Bluetooth::get_enumerators() const {
	TypedArray<BluetoothEnumerator> return_enumerators;
	int cc = get_enumerator_count();
	return_enumerators.resize(cc);

	for (int i = 0; i < enumerators.size(); i++) {
		return_enumerators[i] = get_enumerator(i);
	}

	return return_enumerators;
}

bool Bluetooth::is_supported() const {
	return false;
}

Bluetooth::Bluetooth() {
	singleton = this;
}

Bluetooth::~Bluetooth() {
	singleton = nullptr;
}
