/**************************************************************************/
/*  GodotBluetoothAdvertiser.java                                         */
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

import org.godotengine.godot.*;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.os.Build;
import android.util.Base64;
import android.util.Log;

import java.util.ArrayList;
import java.util.concurrent.locks.ReentrantLock;

public class GodotBluetoothAdvertiser extends BluetoothGattServerCallback implements BluetoothProfile.ServiceListener {
	private static final String TAG = GodotBluetoothAdvertiser.class.getSimpleName();

	private final GodotBluetooth bluetooth;
	private final int id;

	private ArrayList<BluetoothGattService> services;
	private ArrayList<GodotBluetoothCharacteristic> characteristics;
	private GodotBluetoothAdvertisement advertisement;
	private BluetoothLeAdvertiser advertiser;
	private BluetoothGattServer server;
	private BluetoothManager manager;
	private ReentrantLock lock;
	private String name;

	public GodotBluetoothAdvertiser(GodotBluetooth p_bluetooth, int p_id) {
		bluetooth = p_bluetooth;
		id = p_id;
		lock = new ReentrantLock();
		advertisement = null;
		advertiser = null;
		server = null;
		manager = null;
		characteristics = null;
		services = null;
		name = null;
	}

	public GodotBluetooth getBluetooth() {
		return bluetooth;
	}

	public int getIdentifier() {
		return id;
	}

	public String getName() {
		return name;
	}

	public BluetoothLeAdvertiser getAdvertiser() {
		return advertiser;
	}

	public GodotBluetoothCharacteristic getCharacteristic(String p_characteristic_uuid) {
		GodotBluetoothCharacteristic characteristic = null;
		lock.lock();
		try {
			if (characteristics != null) {
				for (int i = 0; i < characteristics.size(); ++i) {
					characteristic = characteristics.get(i);
					if (characteristic != null) {
						if (characteristic.getCharacteristic().getUuid().toString().equalsIgnoreCase(p_characteristic_uuid)) {
							break;
						}
						characteristic = null;
					}
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		return characteristic;
	}

	private boolean processResult(Object result) {
		Boolean success = null;
		if (result == null) {
			return false;
		}
		try {
			success = (Boolean)result;
			if (GodotLib.hasFeature("debug")) {
				if (success != null && success.booleanValue()) {
					Log.v(TAG, "Result handle success!");
				} else {
					Log.e(TAG, "Result handle failure!");
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
			success = null;
		}
		if (success == null) {
			return false;
		}
		return success.booleanValue();
	}

	@SuppressLint("MissingPermission")
	public boolean respondReadRequest(String p_characteristic_uuid, String p_response, int p_request) {
		Boolean result = new Boolean(true);
		BluetoothDevice device = null;
		GodotBluetoothCharacteristic characteristic = getCharacteristic(p_characteristic_uuid);
		if (characteristic == null) {
			return false;
		}
		device = characteristic.getRequestDevice(p_request);
		lock.lock();
		try {
			if (server != null) {
				if (result != null && result.booleanValue()) {
					String read = p_response;
					if (read == null && characteristic != null) {
						read = (String)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_CHARACTERISTIC_VALUE, id, characteristic.getCharacteristic().getUuid().toString());
					}
					if (read == null) {
						read = "";
					}
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
						result = server.sendResponse(device, p_request, BluetoothStatusCodes.SUCCESS, 0, (!read.isEmpty()) ? Base64.decode(read, 0) : null);
					} else {
						result = server.sendResponse(device, p_request, 0, 0, (!read.isEmpty()) ? Base64.decode(read, 0) : null);
					}
				} else {
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
						result = server.sendResponse(device, p_request, BluetoothStatusCodes.ERROR_UNKNOWN, 0, null);
					} else {
						result = server.sendResponse(device, p_request, Integer.MAX_VALUE, 0, null);
					}
				}
				if (result != null && !result.booleanValue()) {
					result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_ERROR, id, null);
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (processResult(result)) {
			return true;
		}
		return false;
	}

	@SuppressLint("MissingPermission")
	public boolean respondWriteRequest(String p_characteristic_uuid, String p_response, int p_request) {
		Boolean result = new Boolean(true);
		BluetoothDevice device = null;
		GodotBluetoothCharacteristic characteristic = getCharacteristic(p_characteristic_uuid);
		if (characteristic == null) {
			return false;
		}
		device = characteristic.getRequestDevice(p_request);
		lock.lock();
		try {
			if (server != null) {
				if (result != null && result.booleanValue()) {
					String wrote = p_response;
					if (wrote == null && characteristic != null) {
						wrote = (String)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_CHARACTERISTIC_VALUE, id, characteristic.getCharacteristic().getUuid().toString());
					}
					if (wrote == null) {
						wrote = "";
					}
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
						result = server.sendResponse(device, p_request, BluetoothStatusCodes.SUCCESS, 0, (!wrote.isEmpty()) ? Base64.decode(wrote, 0) : null);
					} else {
						result = server.sendResponse(device, p_request, 0, 0, (!wrote.isEmpty()) ? Base64.decode(wrote, 0) : null);
					}
				} else {
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
						result = server.sendResponse(device, p_request, BluetoothStatusCodes.ERROR_UNKNOWN, 0, null);
					} else {
						result = server.sendResponse(device, p_request, Integer.MAX_VALUE, 0, null);
					}
				}
				if (result != null && !result.booleanValue()) {
					result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_ERROR, id, null);
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (processResult(result)) {
			return true;
		}
		return false;
	}

	@SuppressLint("MissingPermission")
	public boolean startAdvertising() {
		if (bluetooth == null) {
			return false;
		}
		if (advertiser != null && server != null) {
			return true;
		}
		if ((advertiser == null) ^ (server == null)) {
			return false;
		}
		lock.lock();
		try {
			BluetoothAdapter adapter = bluetooth.getAdapter(GodotBluetooth.BluetoothReason.ADVERTISING);
			if (adapter == null) {
				return false;
			}
			/*if (!adapter.getProfileProxy(bluetooth.getContext(), this, BluetoothProfile.GATT_SERVER)) {
				return false;
			}*/
			manager = bluetooth.getManager();
			if (manager == null) {
				return false;
			}
			advertiser = adapter.getBluetoothLeAdvertiser();
			if (advertiser == null) {
				return false;
			}
			name = (String)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_NAME, id, null);
			if (name != null) {
				adapter.setName(name);
			}
			characteristics = advertisement.getCharacteristics();
			services = advertisement.getServices();
			if (characteristics == null || characteristics.isEmpty() || services == null || services.isEmpty()) {
				if (advertisement != null) {
					advertisement.stopAdvertising();
					advertisement = null;
				}
				advertiser = null;
				characteristics = null;
				services = null;
				return false;
			}
			server = manager.openGattServer(bluetooth.getContext(), this);
			if (!server.addService(services.get(0))) {
				if (advertisement != null) {
					advertisement.stopAdvertising();
					advertisement = null;
				}
				advertiser = null;
				characteristics = null;
				services = null;
				return false;
			}
			//services.remove(0);
			if (advertisement == null) {
				advertisement = new GodotBluetoothAdvertisement(this);
			}
			if (!advertisement.startAdvertising()) {
				advertisement = null;
				advertiser = null;
				return false;
			}
			return true;
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		return false;
	}

	@SuppressLint("MissingPermission")
	public boolean stopAdvertising() {
		boolean success = false;
		lock.lock();
		try {
			if (advertiser != null) {
				if (advertisement != null) {
					advertisement.stopAdvertising();
					advertisement = null;
				}
				advertiser = null;
				success = true;
			}
			if (server != null) {
				server.close();
				server = null;
				success = true;
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		return success;
	}

	@Override
	public void onServiceConnected(int profile, BluetoothProfile proxy) {
	}

	@Override
	public void onServiceDisconnected(int profile) {
	}

	@SuppressLint("MissingPermission")
	@Override
	public void onServiceAdded(int status, BluetoothGattService service) {
		Boolean result = null;
		if (server == null) {
			super.onServiceAdded(status, service);
			return;
		}
		try {
			if (status == BluetoothStatusCodes.SUCCESS) {
				if (services.remove(service)) {
					if (!services.isEmpty()) {
						if (!server.addService(services.get(0))) {
							result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_ERROR, id, null);
						}
					}
				}
			} else {
				result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_ERROR, id, null);
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		if (!processResult(result)) {
			return;
		}
		super.onServiceAdded(status, service);
	}

	@SuppressLint("MissingPermission")
	@Override
	public void onCharacteristicReadRequest(BluetoothDevice device, int requestId, int offset, BluetoothGattCharacteristic characteristic) {
		Boolean result = null;
		Dictionary event = new Dictionary();
		super.onCharacteristicReadRequest(device, requestId, offset, characteristic);
		Log.w(TAG, "Read");
		try {
			event.put("request", Integer.toString(requestId));
			if (device != null) {
				event.put("peer", device.getAddress());
			}
			if (characteristic != null) {
				event.put("characteristic", characteristic.getUuid().toString());
				getCharacteristic(characteristic.getUuid().toString()).putRequestDevice(requestId, device);
			}
			result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_READ, id, event);
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		processResult(result);
	}

	@SuppressLint("MissingPermission")
	@Override
	public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId, BluetoothGattCharacteristic characteristic, boolean preparedWrite, boolean responseNeeded, int offset, byte[] value) {
		Boolean result = null;
		Dictionary event = new Dictionary();
		super.onCharacteristicWriteRequest(device, requestId, characteristic, preparedWrite, responseNeeded, offset, value);
		Log.w(TAG, "Write");
		try {
			event.put("request", Integer.toString(requestId));
			if (device != null) {
				event.put("peer", device.getAddress());
			}
			if (characteristic != null) {
				event.put("characteristic", characteristic.getUuid().toString());
				if (responseNeeded) {
					getCharacteristic(characteristic.getUuid().toString()).putRequestDevice(requestId, device);
				}
			}
			if (value != null) {
				event.put("value", Base64.encodeToString(value, 0));
			}
			result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_WRITE, id, event);
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		processResult(result);
	}

	@Override
	public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
		Boolean result = null;
		super.onConnectionStateChange(device, status, newState);
		if (device == null || status != BluetoothStatusCodes.SUCCESS) {
			return;
		}
		try {
			switch (newState) {
				case BluetoothProfile.STATE_CONNECTED:
					Log.w(TAG, "CONNECTED");
					result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_CONNECT, id, device.getAddress());
					break;
				case BluetoothProfile.STATE_DISCONNECTED:
					Log.w(TAG, "DISCONNECTED");
					result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_DISCONNECT, id, device.getAddress());
					break;
				default:
					break;
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		if (processResult(result)) {
			Log.w(TAG, "CONNECTION");
		}
	}
}
