/**************************************************************************/
/*  bluetooth_advertiser.h                                                */
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

#ifndef BLUETOOTH_ADVERTISER_H
#define BLUETOOTH_ADVERTISER_H

#include "servers/bluetooth_server.h"

/**
	The bluetooth server is a singleton object that gives access to the various
	bluetooth advertisers and device enumerators that can be used as the background for our environment.
**/

class BluetoothAdvertiser : public RefCounted {
	GDCLASS(BluetoothAdvertiser, RefCounted);

private:
	friend class BluetoothServer;

	int id; // unique id for this, for internal use in case devices are removed

	bool can_emit_signal(const StringName &p_name) const;

protected:
	String service_uuid; // uuid of our service advertisement
	Vector<String> characteristic_uuids; // uuids of our characteristics

	bool active; // only when active do we actually update the bluetooth status

	static void _bind_methods();

	virtual void on_register() const;
	virtual void on_unregister() const;

public:
	int get_id() const;
	bool is_active() const;
	void set_active(bool p_is_active);

	String get_service_uuid() const;
	void set_service_uuid(String p_service_uuid);

	// Add and remove characteristics.
	void add_characteristic(String p_characteristic_uuid);
	void remove_characteristic(int p_index);

	// Get our characteristics.
	String get_characteristic(int p_index) const;
	int get_characteristic_count() const;
	TypedArray<String> get_characteristics() const;

	BluetoothAdvertiser();
	BluetoothAdvertiser(String p_service_uuid);
	virtual ~BluetoothAdvertiser();

	virtual bool start_advertising() const;
	virtual bool stop_advertising() const;

	bool on_start() const;
	bool on_stop() const;
};

#endif // BLUETOOTH_ADVERTISER_H
