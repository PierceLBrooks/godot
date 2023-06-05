/**************************************************************************/
/*  bluetooth_server.h                                                    */
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

#ifndef BLUETOOTH_SERVER_H
#define BLUETOOTH_SERVER_H

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/os/thread_safe.h"
#include "core/templates/rid.h"
#include "core/variant/variant.h"

/**
	The bluetooth server is a singleton object that gives access to the various
	bluetooth devices that can be used as the background for our environment.
**/

class BluetoothAdvertiser;
class BluetoothEnumerator;
template <typename T>
class TypedArray;

class BluetoothServer : public Object {
	GDCLASS(BluetoothServer, Object);
	_THREAD_SAFE_CLASS_

public:

	typedef BluetoothServer *(*CreateFunc)();

private:
protected:
	static CreateFunc create_func;

	Vector<Ref<BluetoothAdvertiser>> advertisers;
	Vector<Ref<BluetoothEnumerator>> enumerators;

	static BluetoothServer *singleton;

	static void _bind_methods();

	template <class T>
	static BluetoothServer *_create_builtin() {
		return memnew(T);
	}

public:
	static BluetoothServer *get_singleton();

	template <class T>
	static void make_default() {
		create_func = _create_builtin<T>;
	}

	static BluetoothServer *create() {
		BluetoothServer *server = create_func ? create_func() : memnew(BluetoothServer);
		return server;
	};

	// Right now we identify our advertiser by it's ID when it's used in the background.
	// May see if we can change this to purely relying on BluetoothAdvertiser objects or by name.
	int get_free_advertiser_id();
	int get_advertiser_index(int p_id) const;
	Ref<BluetoothAdvertiser> get_advertiser_by_id(int p_id) const;

	// Add and remove advertisers.
	virtual Ref<BluetoothAdvertiser> new_advertiser();
	void add_advertiser(const Ref<BluetoothAdvertiser> &p_advertiser);
	void remove_advertiser(const Ref<BluetoothAdvertiser> &p_advertiser);

	// Get our advertisers.
	Ref<BluetoothAdvertiser> get_advertiser(int p_index) const;
	int get_advertiser_count() const;
	TypedArray<BluetoothAdvertiser> get_advertisers() const;

	// Right now we identify our enumerator by it's ID when it's used in the background.
	// May see if we can change this to purely relying on BluetoothEnumerator objects or by name.
	int get_free_enumerator_id();
	int get_enumerator_index(int p_id) const;
	Ref<BluetoothEnumerator> get_enumerator_by_id(int p_id) const;

	// Add and remove enumerators.
	virtual Ref<BluetoothEnumerator> new_enumerator();
	void add_enumerator(const Ref<BluetoothEnumerator> &p_enumerator);
	void remove_enumerator(const Ref<BluetoothEnumerator> &p_enumerator);

	// Get our enumerators.
	Ref<BluetoothEnumerator> get_enumerator(int p_index) const;
	int get_enumerator_count() const;
	TypedArray<BluetoothEnumerator> get_enumerators() const;

	virtual bool is_supported() const;

	BluetoothServer();
	~BluetoothServer();
};

#endif // BLUETOOTH_SERVER_H
