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
#include "core/io/json.h"
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

	ClassDB::bind_method(D_METHOD("connect_peer", "peer_uuid"), &BluetoothEnumerator::connect_peer);
	ClassDB::bind_method(D_METHOD("read_peer_service_characteristic", "peer_uuid", "service_uuid", "characteristic_uuid"), &BluetoothEnumerator::read_peer_service_characteristic);
	ClassDB::bind_method(D_METHOD("write_peer_service_characteristic", "peer_uuid", "service_uuid", "characteristic_uuid", "value"), &BluetoothEnumerator::write_peer_service_characteristic);

	ADD_GROUP("Enumerator", "enumerator_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enumerator_is_active"), "set_active", "is_active");

	ADD_SIGNAL(MethodInfo("bluetooth_service_enumeration_started", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("bluetooth_service_enumeration_stopped", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("bluetooth_peer_discovered", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "peer"), PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::DICTIONARY, "advertisement_data")));
	ADD_SIGNAL(MethodInfo("bluetooth_peer_connected", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "peer")));
	ADD_SIGNAL(MethodInfo("bluetooth_peer_disconnected", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "peer")));
	ADD_SIGNAL(MethodInfo("bluetooth_peer_characteristic_discovered", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "peer"), PropertyInfo(Variant::STRING, "service"), PropertyInfo(Variant::STRING, "characteristic"), PropertyInfo(Variant::BOOL, "writable_permission")));
	ADD_SIGNAL(MethodInfo("bluetooth_peer_characteristic_read", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "peer"), PropertyInfo(Variant::STRING, "service"), PropertyInfo(Variant::STRING, "characteristic"), PropertyInfo(Variant::STRING, "value")));
	ADD_SIGNAL(MethodInfo("bluetooth_peer_characteristic_wrote", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "peer"), PropertyInfo(Variant::STRING, "service"), PropertyInfo(Variant::STRING, "characteristic")));
	ADD_SIGNAL(MethodInfo("bluetooth_peer_characteristic_error", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "peer"), PropertyInfo(Variant::STRING, "service"), PropertyInfo(Variant::STRING, "characteristic")));
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
	TypedArray<String> return_sought_services;
	int count = get_sought_service_count();
	return_sought_services.resize(count);

	for (int i = 0; i < count; i++) {
		return_sought_services[i] = get_sought_service(i);
	}

	return return_sought_services;
}

bool BluetoothEnumerator::has_sought_service(String p_service_uuid) const {
	for (int i = 0; i < sought_services.size(); i++) {
		if (get_sought_service(i) == p_service_uuid) {
			return true;
		}
	}
	return false;
}

BluetoothEnumerator::BluetoothEnumeratorPeer::BluetoothEnumeratorPeer() {
	// initialize us
	uuid = "";
}

BluetoothEnumerator::BluetoothEnumeratorPeer::BluetoothEnumeratorPeer(String p_peer_uuid) {
	// initialize us
	uuid = p_peer_uuid;
}

BluetoothEnumerator::BluetoothEnumeratorService::BluetoothEnumeratorService() {
	// initialize us
	uuid = "";
}

BluetoothEnumerator::BluetoothEnumeratorService::BluetoothEnumeratorService(String p_service_uuid) {
	// initialize us
	uuid = p_service_uuid;
}

BluetoothEnumerator::BluetoothEnumeratorCharacteristic::BluetoothEnumeratorCharacteristic() {
	// initialize us
	uuid = "";
}

BluetoothEnumerator::BluetoothEnumeratorCharacteristic::BluetoothEnumeratorCharacteristic(String p_characteristic_uuid) {
	// initialize us
	uuid = p_characteristic_uuid;
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

void BluetoothEnumerator::connect_peer(String p_peer_uuid) {
	// nothing to do here
}

void BluetoothEnumerator::read_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) {
	// nothing to do here
}

void BluetoothEnumerator::write_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value) {
	// nothing to do here
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

bool BluetoothEnumerator::on_stop() const {
	if (can_emit_signal(SNAME("bluetooth_service_enumeration_stopped"))) {
		Ref<BluetoothEnumerator>* reference = new Ref<BluetoothEnumerator>(const_cast<BluetoothEnumerator*>(this));
		if (reference->is_valid()) {
			Thread thread;
			thread.start([](void *p_udata) {
				Ref<BluetoothEnumerator>* enumerator = static_cast<Ref<BluetoothEnumerator>*>(p_udata);
				if (enumerator->is_valid()) {
					(*enumerator)->active = false;
					(*enumerator)->emit_signal(SNAME("bluetooth_service_enumeration_stopped"), (*enumerator)->get_id());
				}
			}, reference);
			thread.wait_to_finish();
		}
		delete reference;
		return true;
	}
	return false;
}

bool BluetoothEnumerator::on_discover(String p_peer_uuid, String p_peer_name, String p_peer_advertisement_data) const {
	Dictionary peer_advertisement_data;
	if (!p_peer_advertisement_data.is_empty()) {
		Variant advertisement_data = JSON::parse_string(core_bind::Marshalls::get_singleton()->base64_to_utf8(p_peer_advertisement_data));
		if (advertisement_data.get_type() == Variant::Type::DICTIONARY) {
			peer_advertisement_data = advertisement_data;
		}
	}
	if (can_emit_signal(SNAME("bluetooth_peer_discovered"))) {
		Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* reference = new Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>();
		reference->instantiate();
		if (reference->is_valid()) {
			Thread thread;
			(*reference)->uuid = p_peer_uuid;
			(*reference)->name = p_peer_name;
			(*reference)->advertisement_data = peer_advertisement_data;
			(*reference)->enumerator = Ref<BluetoothEnumerator>(const_cast<BluetoothEnumerator*>(this));
			thread.start([](void *p_udata) {
				Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* peer = static_cast<Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>*>(p_udata);
				if (peer->is_valid() && (*peer)->enumerator.is_valid()) {
					(*peer)->enumerator->peers[(*peer)->uuid] = *peer;
					(*peer)->enumerator->emit_signal(SNAME("bluetooth_peer_discovered"), (*peer)->enumerator->get_id(), (*peer)->uuid, (*peer)->name, (*peer)->advertisement_data);
				}
			}, reference);
			thread.wait_to_finish();
			(*reference)->enumerator = *BluetoothEnumerator::null_enumerator;
		}
		delete reference;
		return true;
	}
	return false;
}

bool BluetoothEnumerator::on_connect(String p_peer_uuid) const {
	if (can_emit_signal(SNAME("bluetooth_peer_connected"))) {
		Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* reference = new Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>(peers[p_peer_uuid]);
		if (reference->is_valid()) {
			Thread thread;
			(*reference)->enumerator = Ref<BluetoothEnumerator>(const_cast<BluetoothEnumerator*>(this));
			thread.start([](void *p_udata) {
				Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* peer = static_cast<Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>*>(p_udata);
				if (peer->is_valid() && (*peer)->enumerator.is_valid()) {
					(*peer)->enumerator->emit_signal(SNAME("bluetooth_peer_connected"), (*peer)->enumerator->get_id(), (*peer)->uuid);
				}
			}, reference);
			thread.wait_to_finish();
			(*reference)->enumerator = *BluetoothEnumerator::null_enumerator;
		}
		delete reference;
		return true;
	}
	return false;
}

bool BluetoothEnumerator::on_disconnect(String p_peer_uuid) const {
	if (can_emit_signal(SNAME("bluetooth_peer_disconnected"))) {
		Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* reference = new Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>(peers[p_peer_uuid]);
		if (reference->is_valid()) {
			Thread thread;
			(*reference)->enumerator = Ref<BluetoothEnumerator>(const_cast<BluetoothEnumerator*>(this));
			thread.start([](void *p_udata) {
				Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* peer = static_cast<Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>*>(p_udata);
				if (peer->is_valid() && (*peer)->enumerator.is_valid()) {
					(*peer)->enumerator->peers.erase((*peer)->uuid);
					(*peer)->enumerator->emit_signal(SNAME("bluetooth_peer_disconnected"), (*peer)->enumerator->get_id(), (*peer)->uuid);
				}
			}, reference);
			thread.wait_to_finish();
			(*reference)->enumerator = *BluetoothEnumerator::null_enumerator;
		}
		delete reference;
		return true;
	}
	return false;
}

bool BluetoothEnumerator::on_discover_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, bool p_writable_permission) const {
	if (peers.find(p_peer_uuid) == peers.end()) {
		return false;
	}
	if (can_emit_signal(SNAME("bluetooth_peer_characteristic_discovered"))) {
		Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* peer = new Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>(peers[p_peer_uuid]);
		Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>* reference = new Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>();
		reference->instantiate();
		if (peer->is_valid() && reference->is_valid()) {
			Thread thread;
			(*reference)->uuid = p_characteristic_uuid;
			(*reference)->service = p_service_uuid;
			(*reference)->permission = p_writable_permission;
			(*reference)->peer = *peer;
			(*peer)->enumerator = Ref<BluetoothEnumerator>(const_cast<BluetoothEnumerator*>(this));
			thread.start([](void *p_udata) {
				Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>* characteristic = static_cast<Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>*>(p_udata);
				if (characteristic->is_valid() && (*characteristic)->peer.is_valid() && (*characteristic)->peer->enumerator.is_valid()) {
					Ref<BluetoothEnumerator::BluetoothEnumeratorService> service;
					String uuid = (*characteristic)->service;
					if ((*characteristic)->peer->services.find(uuid) == (*characteristic)->peer->services.end()) {
						service.instantiate();
						service->uuid = uuid;
						(*characteristic)->peer->services[uuid] = service;
					} else {
						service.reference_ptr(*((*characteristic)->peer->services[uuid]));
					}
					if (service.is_valid()) {
						uuid = (*characteristic)->uuid;
						if (service->characteristics.find(uuid) == service->characteristics.end()) {
							service->characteristics[uuid] = *characteristic;
						}
						(*characteristic)->peer->enumerator->emit_signal(SNAME("bluetooth_peer_characteristic_discovered"), (*characteristic)->peer->enumerator->get_id(), (*characteristic)->peer->uuid, service->uuid, uuid, (*characteristic)->permission);
					}
				}
			}, reference);
			thread.wait_to_finish();
			(*peer)->enumerator = *BluetoothEnumerator::null_enumerator;
			(*reference)->peer = *BluetoothEnumerator::null_peer;
		}
		delete reference;
		delete peer;
		return true;
	}
	return false;
}

bool BluetoothEnumerator::on_read(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value) const {
	if (peers.find(p_peer_uuid) == peers.end()) {
		return false;
	}
	if (can_emit_signal(SNAME("bluetooth_peer_characteristic_read"))) {
		Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* peer = new Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>(peers[p_peer_uuid]);
		if (peer->is_valid()) {
			Ref<BluetoothEnumerator::BluetoothEnumeratorService> service;
			if ((*peer)->services.find(p_service_uuid) != (*peer)->services.end()) {
				service = (*peer)->services[p_service_uuid];
			}
			if (service.is_valid() && service->characteristics.find(p_characteristic_uuid) != service->characteristics.end()) {
				Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>* reference = new Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>(service->characteristics[p_characteristic_uuid]);
				if (reference->is_valid()) {
					Thread thread;
					(*reference)->value = core_bind::Marshalls::get_singleton()->base64_to_utf8(p_value);
					(*reference)->uuid = p_characteristic_uuid;
					(*reference)->service = p_service_uuid;
					(*reference)->peer = *peer;
					(*peer)->enumerator = Ref<BluetoothEnumerator>(const_cast<BluetoothEnumerator*>(this));
					thread.start([](void *p_udata) {
						Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>* characteristic = static_cast<Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>*>(p_udata);
						if (characteristic->is_valid() && (*characteristic)->peer.is_valid() && (*characteristic)->peer->enumerator.is_valid()) {
							(*characteristic)->peer->enumerator->emit_signal(SNAME("bluetooth_peer_characteristic_read"), (*characteristic)->peer->enumerator->get_id(), (*characteristic)->peer->uuid, (*characteristic)->service, (*characteristic)->uuid, (*characteristic)->value);
						}
					}, reference);
					thread.wait_to_finish();
					(*peer)->enumerator = *BluetoothEnumerator::null_enumerator;
					(*reference)->peer = *BluetoothEnumerator::null_peer;
				}
				delete reference;
			}
		}
		delete peer;
		return true;
	}
	return false;
}

bool BluetoothEnumerator::on_write(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) const {
	if (peers.find(p_peer_uuid) == peers.end()) {
		return false;
	}
	if (can_emit_signal(SNAME("bluetooth_peer_characteristic_wrote"))) {
		Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* peer = new Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>(peers[p_peer_uuid]);
		if (peer->is_valid()) {
			Ref<BluetoothEnumerator::BluetoothEnumeratorService> service;
			if ((*peer)->services.find(p_service_uuid) != (*peer)->services.end()) {
				service = (*peer)->services[p_service_uuid];
			}
			if (service.is_valid() && service->characteristics.find(p_characteristic_uuid) != service->characteristics.end()) {
				Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>* reference = new Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>(service->characteristics[p_characteristic_uuid]);
				if (reference->is_valid()) {
					Thread thread;
					(*reference)->uuid = p_characteristic_uuid;
					(*reference)->service = p_service_uuid;
					(*reference)->peer = *peer;
					(*peer)->enumerator = Ref<BluetoothEnumerator>(const_cast<BluetoothEnumerator*>(this));
					thread.start([](void *p_udata) {
						Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>* characteristic = static_cast<Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>*>(p_udata);
						if (characteristic->is_valid() && (*characteristic)->peer.is_valid() && (*characteristic)->peer->enumerator.is_valid()) {
							(*characteristic)->peer->enumerator->emit_signal(SNAME("bluetooth_peer_characteristic_wrote"), (*characteristic)->peer->enumerator->get_id(), (*characteristic)->peer->uuid, (*characteristic)->service, (*characteristic)->uuid);
						}
					}, reference);
					thread.wait_to_finish();
					(*peer)->enumerator = *BluetoothEnumerator::null_enumerator;
					(*reference)->peer = *BluetoothEnumerator::null_peer;
				}
				delete reference;
			}
		}
		delete peer;
		return true;
	}
	return false;
}

bool BluetoothEnumerator::on_error(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) const {
	if (peers.find(p_peer_uuid) == peers.end()) {
		return false;
	}
	if (can_emit_signal(SNAME("bluetooth_peer_characteristic_error"))) {
		Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* peer = new Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>(peers[p_peer_uuid]);
		if (peer->is_valid()) {
			Ref<BluetoothEnumerator::BluetoothEnumeratorService> service;
			if ((*peer)->services.find(p_service_uuid) != (*peer)->services.end()) {
				service = (*peer)->services[p_service_uuid];
			}
			if (service.is_valid() && service->characteristics.find(p_characteristic_uuid) != service->characteristics.end()) {
				Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>* reference = new Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>(service->characteristics[p_characteristic_uuid]);
				if (reference->is_valid()) {
					Thread thread;
					(*reference)->uuid = p_characteristic_uuid;
					(*reference)->service = p_service_uuid;
					(*reference)->peer = *peer;
					(*peer)->enumerator = Ref<BluetoothEnumerator>(const_cast<BluetoothEnumerator*>(this));
					thread.start([](void *p_udata) {
						Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>* characteristic = static_cast<Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>*>(p_udata);
						if (characteristic->is_valid() && (*characteristic)->peer.is_valid() && (*characteristic)->peer->enumerator.is_valid()) {
							(*characteristic)->peer->enumerator->emit_signal(SNAME("bluetooth_peer_characteristic_error"), (*characteristic)->peer->enumerator->get_id(), (*characteristic)->peer->uuid, (*characteristic)->service, (*characteristic)->uuid);
						}
					}, reference);
					thread.wait_to_finish();
					(*peer)->enumerator = *BluetoothEnumerator::null_enumerator;
					(*reference)->peer = *BluetoothEnumerator::null_peer;
				}
				delete reference;
			}
		}
		delete peer;
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
Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>* BluetoothEnumerator::null_peer = new Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>(new BluetoothEnumerator::BluetoothEnumeratorPeer());
Ref<BluetoothEnumerator::BluetoothEnumeratorService>* BluetoothEnumerator::null_service = new Ref<BluetoothEnumerator::BluetoothEnumeratorService>(new BluetoothEnumerator::BluetoothEnumeratorService());
Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>* BluetoothEnumerator::null_characteristic = new Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>(new BluetoothEnumerator::BluetoothEnumeratorCharacteristic());
