/**************************************************************************/
/*  bluetooth_enumerator.h                                                */
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

#ifndef BLUETOOTH_ENUMERATOR_H
#define BLUETOOTH_ENUMERATOR_H

#include "bluetooth.h"

/**
	The bluetooth server is a singleton object that gives access to the various
	bluetooth enumerator and device enumerators that can be used as the background for our environment.
**/

class BluetoothEnumerator : public RefCounted {
	GDCLASS(BluetoothEnumerator, RefCounted);

protected:
	class BluetoothEnumeratorPeer;
	class BluetoothEnumeratorService;
	class BluetoothEnumeratorCharacteristic;

private:
	friend class Bluetooth;

	int id; // unique id for this, for internal use in case devices are removed

	bool can_emit_signal(const StringName &p_name) const;

protected:
	class BluetoothEnumeratorCharacteristic : public RefCounted {
		GDCLASS(BluetoothEnumeratorCharacteristic, RefCounted);

	public:
		BluetoothEnumeratorCharacteristic();
		BluetoothEnumeratorCharacteristic(String p_characteristic_uuid);
		Ref<BluetoothEnumerator::BluetoothEnumeratorPeer> peer;
		String service;
		String uuid;
		String value;
		bool permission;
	};

	class BluetoothEnumeratorService : public RefCounted {
		GDCLASS(BluetoothEnumeratorService, RefCounted);

	public:
		BluetoothEnumeratorService();
		BluetoothEnumeratorService(String p_service_uuid);
		Ref<BluetoothEnumerator::BluetoothEnumeratorPeer> peer;
		String uuid;
		HashMap<String, Ref<BluetoothEnumerator::BluetoothEnumeratorCharacteristic>> characteristics;
	};

	class BluetoothEnumeratorPeer : public RefCounted {
		GDCLASS(BluetoothEnumeratorPeer, RefCounted);

	public:
		BluetoothEnumeratorPeer();
		BluetoothEnumeratorPeer(String p_peer_uuid);
		Ref<BluetoothEnumerator> enumerator;
		String uuid;
		String name;
		Dictionary advertisement_data;
		HashMap<String, Ref<BluetoothEnumerator::BluetoothEnumeratorService>> services;
	};

	Vector<String> sought_services; // our sought services
	HashMap<String, Ref<BluetoothEnumerator::BluetoothEnumeratorPeer>> peers; // our peers

	bool active; // only when active do we actually update the bluetooth status

	static void _bind_methods();

	virtual void on_register() const;
	virtual void on_unregister() const;

public:
	int get_id() const;
	bool is_active() const;
	void set_active(bool p_is_active);

	// Add and remove sought services.
	void add_sought_service(String p_service_uuid);
	void remove_sought_service(int p_index);

	// Get our sought services.
	int get_sought_service_count() const;
	String get_sought_service(int p_index) const;
	TypedArray<String> get_sought_services() const;
	bool has_sought_service(String p_service_uuid) const;

	BluetoothEnumerator(int p_id);
	BluetoothEnumerator();
	virtual ~BluetoothEnumerator();

	virtual bool start_scanning() const;
	virtual bool stop_scanning() const;

	virtual void connect_peer(String p_peer_uuid);
	virtual void read_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid);
	virtual void write_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value);

	bool on_start() const;
	bool on_stop() const;
	bool on_discover(String p_peer_uuid, String p_peer_name, String p_peer_advertisement_data) const;
	bool on_connect(String p_peer_uuid) const;
	bool on_disconnect(String p_peer_uuid) const;
	bool on_discover_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, bool p_writable_permission) const;
	bool on_read(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value_base64) const;
	bool on_write(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) const;
	bool on_error(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) const;
};

#endif // BLUETOOTH_ENUMERATOR_H
