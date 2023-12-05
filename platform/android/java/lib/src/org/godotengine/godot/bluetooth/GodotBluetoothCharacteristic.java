/**************************************************************************/
/*  GodotBluetoothCharacteristic.java                                     */
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

package org.godotengine.godot.bluetooth;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;

import org.godotengine.godot.Dictionary;

public class GodotBluetoothCharacteristic extends Dictionary {
	private static final String TAG = GodotBluetoothCharacteristic.class.getSimpleName();

	private BluetoothGattService service;
	private BluetoothGattCharacteristic characteristic;
	private boolean permission;

	public GodotBluetoothCharacteristic(BluetoothGattService p_service, BluetoothGattCharacteristic p_characteristic, boolean p_permission) {
		service = p_service;
		characteristic = p_characteristic;
		permission = p_permission;
	}

	public BluetoothGattService getService() {
		return service;
	}

	public BluetoothGattCharacteristic getCharacteristic() {
		return characteristic;
	}

	public boolean getPermission() {
		return permission;
	}

	public boolean putRequestDevice(int id, BluetoothDevice device) {
		if (containsKey(Integer.toString(id)) || device == null) {
			return false;
		}
		put(Integer.toString(id), device);
		return true;
	}

	public BluetoothDevice getRequestDevice(int id) {
		BluetoothDevice device = null;
		if (!containsKey(Integer.toString(id))) {
			return device;
		}
		device = (BluetoothDevice)get(Integer.toString(id));
		remove(Integer.toString(id));
		return device;
	}
}
