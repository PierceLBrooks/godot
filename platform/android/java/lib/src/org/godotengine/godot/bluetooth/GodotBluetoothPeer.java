/**************************************************************************/
/*  GodotBluetoothPeer.java                                               */
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

import android.annotation.SuppressLint;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.os.Build;

import org.godotengine.godot.GodotLib;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class GodotBluetoothPeer extends BluetoothGattCallback {
	private int id;
    private BluetoothDevice device;
	private BluetoothAdapter adapter;
	private BluetoothGatt gatt;
	private AtomicBoolean connection;

    GodotBluetoothPeer(int p_id, BluetoothDevice p_device) {
		id = p_id;
        device = p_device;
		connection = new AtomicBoolean(false);
    }

	@SuppressLint("MissingPermission")
	public boolean connect(GodotBluetooth p_bluetooth) {
		boolean success = false;
		Activity activity = null;
		if (gatt != null) {
			return true;
		}
		if (p_bluetooth == null) {
			return success;
		}
		activity = p_bluetooth.getActivity();
		if (activity == null) {
			return success;
		}
		if (device == null) {
			return success;
		}
		if (adapter == null) {
			adapter = p_bluetooth.getAdapter(GodotBluetooth.BluetoothReason.CONNECTING);
		}
		if (adapter != null) {
			try {
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
					gatt = device.connectGatt(activity, true, this, BluetoothDevice.TRANSPORT_LE);
				} else {
					gatt = device.connectGatt(activity, true, this);
				}
			} catch (Exception exception) {
				GodotLib.printStackTrace(exception);
			}
			if (gatt != null) {
				success = true;
			}
		}
		return success;
	}

    public BluetoothDevice getDevice() {
        return device;
    }

	@SuppressLint("MissingPermission")
	@Override
	public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
		boolean success = false;
		Object result = null;
		super.onConnectionStateChange(gatt, status, newState);
		try {
			switch (newState) {
				case BluetoothProfile.STATE_CONNECTED:
					connection.set(true);
					result = GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_CONNECT, id, device.getAddress());
					break;
				case BluetoothProfile.STATE_DISCONNECTED:
					connection.set(false);
					result = GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_DISCONNECT, id, device.getAddress());
					break;
				default:
					break;
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		if (result != null) {
			try {
				success = ((Boolean)result).booleanValue();
				if (success) {
					success = gatt.discoverServices();
				}
			} catch (Exception exception) {
				GodotLib.printStackTrace(exception);
			}
		}
	}

	@Override
	public void onServicesDiscovered(BluetoothGatt gatt, int status) {
		super.onServicesDiscovered(gatt, status);
		if (gatt != null) {
			List<BluetoothGattService> services = gatt.getServices();
			if (services != null) {
				for (int i = 0; i < services.size(); ++i) {
					BluetoothGattService service = services.get(i);
					if (service != null) {
						List<BluetoothGattCharacteristic> characteristics = service.getCharacteristics();
						if (characteristics != null) {
							for (int j = 0; j < characteristics.size(); ++j) {
								BluetoothGattCharacteristic characteristic = characteristics.get(j);
								if (characteristic != null) {
									try {
										boolean permission = false;
										if ((characteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_WRITE) != 0) {
											permission = true;
										}
										GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_DISCOVER_SERVICE_CHARACTERISTIC, id, new String[]{device.getAddress(), service.getUuid().toString(), characteristic.getUuid().toString(), String.valueOf(permission), ""});
									} catch (Exception exception) {
										GodotLib.printStackTrace(exception);
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
