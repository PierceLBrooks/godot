/**************************************************************************/
/*  bluetooth_enumerator_macos.mm                                         */
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

#include "bluetooth_server_macos.h"
#include "bluetooth_enumerator_macos.h"
#include "servers/bluetooth/bluetooth_enumerator.h"
#include "core/config/engine.h"
#include "core/core_bind.h"

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

BluetoothEnumeratorMacOS::BluetoothEnumeratorMacOS() {
}

BluetoothEnumeratorMacOS::~BluetoothEnumeratorMacOS() {
}

bool BluetoothEnumeratorMacOS::start_scanning() const {
	return true;
}

bool BluetoothEnumeratorMacOS::stop_scanning() const {
	return true;
}

void BluetoothEnumeratorMacOS::on_register() const {
	// nothing to do here
}

void BluetoothEnumeratorMacOS::on_unregister() const {
	// nothing to do here
}
