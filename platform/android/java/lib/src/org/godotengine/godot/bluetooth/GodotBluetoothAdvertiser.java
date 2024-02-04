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
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.os.Build;
import android.util.Base64;
import android.util.Log;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.concurrent.locks.ReentrantLock;

public class GodotBluetoothAdvertiser extends BluetoothGattServerCallback implements BluetoothProfile.ServiceListener {
	private static final String TAG = GodotBluetoothAdvertiser.class.getSimpleName();

	private final GodotBluetooth bluetooth;
	private final int id;

	private HashSet<BluetoothDevice> devices;
	private ArrayList<BluetoothGattService> services;
	private ArrayList<GodotBluetoothCharacteristic> characteristics;
	private GodotBluetoothAdvertisement advertisement;
	private BluetoothLeAdvertiser advertiser;
	private BluetoothGattServer server;
	private BluetoothManager manager;
	private ReentrantLock lock;
	private String name;
	private AdvertiseData data;
	private AdvertiseSettings settings;

	public GodotBluetoothAdvertiser(GodotBluetooth p_bluetooth, int p_id) {
		super();
		bluetooth = p_bluetooth;
		id = p_id;
		lock = new ReentrantLock();
		devices = new HashSet<BluetoothDevice>();
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
		if (p_characteristic_uuid == null) {
			return characteristic;
		}
		lock.lock();
		try {
			if (characteristics != null) {
				for (int i = 0; i < characteristics.size(); ++i) {
					characteristic = characteristics.get(i);
					if (characteristic != null) {
						if (characteristic.getCharacteristic().getUuid().toString().trim().equalsIgnoreCase(p_characteristic_uuid.trim())) {
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
		if (result == null || !(result instanceof Boolean)) {
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
		characteristics = null;
		services = null;
		data = null;
		settings = null;
		try {
			BluetoothAdapter adapter = bluetooth.getAdapter(GodotBluetooth.BluetoothReason.ADVERTISING);
			if (adapter == null) {
				Log.w(TAG, "No adapter!");
				return false;
			}
			if (!adapter.isEnabled()) {
				Log.w(TAG, "No support!");
				return false;
			}
			/*if (!adapter.getProfileProxy(bluetooth.getContext(), this, BluetoothProfile.GATT_SERVER)) {
				return false;
			}*/
			manager = bluetooth.getManager();
			if (manager == null) {
				Log.w(TAG, "No manager!");
				return false;
			}
			advertiser = adapter.getBluetoothLeAdvertiser();
			if (advertiser == null) {
				Log.w(TAG, "No advertiser!");
				return false;
			}
			name = (String)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_NAME, id, null);
			if (name != null) {
				if (name.length() > 8) {
					name = name.substring(0, 8);
				}
				adapter.setName(name);
			}
			if (advertisement == null) {
				advertisement = new GodotBluetoothAdvertisement(this);
			}
			data = advertisement.getData();
			settings = advertisement.getSettings();
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
				data = null;
				settings = null;
				Log.w(TAG, "No characteristics!");
				return false;
			}
			server = manager.openGattServer(bluetooth.getContext(), this);
			if (!server.addService(services.get(0))) {
				if (advertisement != null) {
					advertisement.stopAdvertising();
					advertisement = null;
				}
				if (server != null) {
					server.close();
					server = null;
				}
				advertiser = null;
				characteristics = null;
				services = null;
				data = null;
				settings = null;
				Log.w(TAG, "No services!");
				return false;
			}
			//services.remove(0);
			if (!advertisement.startAdvertising()) {
				if (advertisement != null) {
					advertisement.stopAdvertising();
					advertisement = null;
				}
				if (server != null) {
					server.close();
					server = null;
				}
				advertiser = null;
				characteristics = null;
				services = null;
				data = null;
				settings = null;
				Log.w(TAG, "No advertisement!");
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
		if (success) {
			GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_STOP_BROADCASTING, id, "");
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
		Object result = new Object();
		for (int i = 0; i < services.size(); ++i) {
			if (services.get(i).getUuid().toString().trim().equalsIgnoreCase(service.getUuid().toString().trim())) {
				result = null;
				break;
			}
		}
		if (result != null) {
			Log.w(TAG, "Bad service! "+service.getUuid().toString().trim());
		}
		if (server == null) {
			super.onServiceAdded(status, service);
			return;
		}
		try {
			if (status == BluetoothStatusCodes.SUCCESS) {
				if (services.remove(service)) {
					if (!services.isEmpty()) {
						if (!server.addService(services.get(0))) {
							result = GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_ERROR, id, null);
						}
					}
				} else {
					Log.w(TAG, "Bad service!");
				}
			} else {
				result = GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_ERROR, id, null);
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
				event.put("characteristic", characteristic.getUuid().toString().toLowerCase());
				if (responseNeeded) {
					getCharacteristic(characteristic.getUuid().toString().toLowerCase()).putRequestDevice(requestId, device);
				}
			}
			if (value != null) {
				event.put("value", Base64.encodeToString(value, Base64.DEFAULT).trim());
			}
			result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_WRITE, id, event);
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		processResult(result);
	}

	@SuppressLint("MissingPermission")
	@Override
	public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
		boolean connect = false;
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
					connect = true;
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
		lock.lock();
		try {
			if (connect) {
				if (!devices.contains(device)) {
					//result = device.createBond(); // this is for pairing, and we should just let that be up to users to arrange externally through native system menu interfaces for the platform's bluetooth driver
					devices.add(device);
				}
			} else {
				if (devices.contains(device)) {
					devices.remove(device);
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
	}

	public void onPower() {
		if (!bluetooth.getPower() && (advertiser != null || server != null)) {
			stopAdvertising();
		}
	}
}
