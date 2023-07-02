/**************************************************************************/
/*  bluetooth_enumerator_macos.h                                          */
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

#ifndef BLUETOOTH_ENUMERATOR_MACOS_H
#define BLUETOOTH_ENUMERATOR_MACOS_H

#include "bluetooth_enumerator.h"

//////////////////////////////////////////////////////////////////////////
// BluetoothEnumeratorMacOS - Subclass for bluetooth enumerators in macOS

class BluetoothEnumeratorMacOS : public BluetoothEnumerator {
private:
	void *central_manager_delegate;
	static BluetoothEnumerator *_create() { return memnew(BluetoothEnumeratorMacOS); }
public:
	BluetoothEnumeratorMacOS();
	virtual ~BluetoothEnumeratorMacOS();

	static void initialize();
	static void deinitialize();

	bool start_scanning() const override;
	bool stop_scanning() const override;

	void connect_peer(String p_peer_uuid) override;
	void read_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) override;
	void write_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value) override;

    void on_register() const override;
    void on_unregister() const override;
};

#endif // BLUETOOTH_ENUMERATOR_MACOS_H
