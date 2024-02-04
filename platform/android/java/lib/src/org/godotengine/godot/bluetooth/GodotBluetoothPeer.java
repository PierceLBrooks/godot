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

import org.godotengine.godot.Dictionary;
import org.godotengine.godot.GodotLib;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.os.Build;
import android.util.Base64;
import android.util.Log;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReentrantLock;

import org.json.JSONObject;

public class GodotBluetoothPeer extends BluetoothGattCallback {
	private static final String TAG = GodotBluetoothPeer.class.getSimpleName();

	private int id;
	private boolean connectable;
	private BluetoothDevice device;
	private BluetoothAdapter adapter;
	private BluetoothGatt gatt;
	private AtomicBoolean connection;
	private ReentrantLock lock;
	private HashMap<String, HashMap<String, GodotBluetoothCharacteristic>> characteristics;

	public GodotBluetoothPeer(int p_id, BluetoothDevice p_device, boolean p_connectable) {
		super();
		id = p_id;
		device = p_device;
		connection = new AtomicBoolean(false);
		lock = new ReentrantLock();
		characteristics = new HashMap<String, HashMap<String, GodotBluetoothCharacteristic>>();
		gatt = null;
		adapter = null;
		connectable = p_connectable;
	}

	@SuppressLint("MissingPermission")
	public boolean connect(GodotBluetooth p_bluetooth) {
		boolean success = false;
		Activity activity = null;
		if (gatt != null) {
			Log.w(TAG, "Already gatting!");
			int state = gatt.getConnectionState(device);
			if (device != null && state == BluetoothProfile.STATE_CONNECTED) {
				return true;
			} else {
				Log.w(TAG, "Still connecting! "+state);
			}
		}
		if (!connectable) {
			Log.w(TAG, "Not connectable!");
			return success;
		}
		if (p_bluetooth == null) {
			Log.w(TAG, "Null device!");
			return success;
		}
		activity = p_bluetooth.getActivity();
		if (activity == null) {
			Log.w(TAG, "Null activity!");
			return success;
		}
		if (device == null) {
			Log.w(TAG, "Null device!");
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
			} else {
				Log.w(TAG, "Null gatt!");
			}
		} else {
			Log.w(TAG, "Null adapter!");
		}
		if (success) {
			Log.w(TAG, "Connect!");
		} else {
			Log.w(TAG, "No connect!");
		}
		return success;
	}

	public BluetoothDevice getDevice() {
		return device;
	}

	@SuppressLint("MissingPermission")
	public boolean readCharacteristic(String p_service_uuid, String p_characteristic_uuid) {
		boolean success = false;
		GodotBluetoothCharacteristic characteristic = null;
		if (!connection.get()) {
			return success;
		}
		if (gatt == null) {
			return success;
		}
		lock.lock();
		try {
			if (characteristics.containsKey(p_service_uuid)) {
				if (characteristics.get(p_service_uuid).containsKey(p_characteristic_uuid)) {
					characteristic = characteristics.get(p_service_uuid).get(p_characteristic_uuid);
				}
			}
			if (characteristic != null) {
				success = gatt.readCharacteristic(characteristic.getCharacteristic());
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		return success;
	}

	@SuppressLint("MissingPermission")
	public boolean writeCharacteristic(String p_service_uuid, String p_characteristic_uuid, String p_value) {
		boolean success = false;
		GodotBluetoothCharacteristic characteristic = null;
		if (!connection.get()) {
			return success;
		}
		if (gatt == null) {
			return success;
		}
		lock.lock();
		try {
			if (characteristics.containsKey(p_service_uuid)) {
				if (characteristics.get(p_service_uuid).containsKey(p_characteristic_uuid)) {
					characteristic = characteristics.get(p_service_uuid).get(p_characteristic_uuid);
				}
			}
			if (characteristic != null && characteristic.getPermission()) {
				if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
					success = gatt.writeCharacteristic(characteristic.getCharacteristic());
				} else {
					if (characteristic.getCharacteristic().getWriteType() != BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE && (characteristic.getCharacteristic().getPermissions() & BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) == 0) {
						gatt.beginReliableWrite();
					}
					if (gatt.writeCharacteristic(characteristic.getCharacteristic(), Base64.decode(p_value, 0), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS) {
						success = true;
					}
					if (characteristic.getCharacteristic().getWriteType() != BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE && (characteristic.getCharacteristic().getPermissions() & BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) == 0) {
						if (success) {
							gatt.executeReliableWrite();
						} else {
							gatt.abortReliableWrite();
						}
					}
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		return success;
	}

	@Override
	@TargetApi(Build.VERSION_CODES.TIRAMISU)
	public void onCharacteristicRead(@NonNull BluetoothGatt gatt, @NonNull BluetoothGattCharacteristic characteristic, @NonNull byte[] value, int status) {
		Boolean result = null;
		Dictionary event = new Dictionary();
		super.onCharacteristicRead(gatt, characteristic, value, status);
		Log.i(TAG, "Read");
		try {
			if (device != null) {
				event.put("peer", device.getAddress());
			}
			if (characteristic != null) {
				event.put("service", characteristic.getService().getUuid().toString().toLowerCase());
				event.put("characteristic", characteristic.getUuid().toString().toLowerCase());
			}
			if (status == BluetoothStatusCodes.SUCCESS) {
				if (value != null) {
					event.put("value", Base64.encodeToString(value, Base64.DEFAULT).trim());
				}
				result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ENUMERATOR_READ, id, event);
			} else {
				result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ENUMERATOR_ERROR, id, event);
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
	}

	@Override
	public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
		Boolean result = null;
		Dictionary event = new Dictionary();
		super.onCharacteristicRead(gatt, characteristic, status);
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
			Log.i(TAG, "Read "+status);
			try {
				if (device != null) {
					event.put("peer", device.getAddress());
				}
				if (characteristic != null) {
					event.put("service", characteristic.getService().getUuid().toString().toLowerCase());
					event.put("characteristic", characteristic.getUuid().toString().toLowerCase());
				}
				if (status == BluetoothStatusCodes.SUCCESS) {
					byte[] value = characteristic.getValue();
					if (value != null) {
						event.put("value", Base64.encodeToString(value, Base64.DEFAULT).trim());
					}
					result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ENUMERATOR_READ, id, event);
				} else {
					result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ENUMERATOR_ERROR, id, event);
				}
			} catch (Exception exception) {
				GodotLib.printStackTrace(exception);
			}
		}
	}

	@Override
	public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
		Boolean result = null;
		Dictionary event = new Dictionary();
		super.onCharacteristicWrite(gatt, characteristic, status);
		Log.w(TAG, "Write "+status);
		try {
			if (device != null) {
				event.put("peer", device.getAddress());
			}
			if (characteristic != null) {
				event.put("service", characteristic.getService().getUuid().toString().toLowerCase());
				event.put("characteristic", characteristic.getUuid().toString().toLowerCase());
			}
			if (status == BluetoothStatusCodes.SUCCESS) {
				result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ENUMERATOR_WRITE, id, event);
			} else {
				result = (Boolean)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ENUMERATOR_ERROR, id, event);
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
	}

	@SuppressLint("MissingPermission")
	@Override
	public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
		boolean success = false;
		Object result = null;
		Log.w(TAG, "Status @ " + newState + " = " + status);
		super.onConnectionStateChange(gatt, status, newState);
		if (gatt == null || status != BluetoothStatusCodes.SUCCESS) {
			return;
		}
		try {
			switch (newState) {
				case BluetoothProfile.STATE_CONNECTED:
					Log.w(TAG, "CONNECTED");
					connection.set(true);
					result = GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ENUMERATOR_CONNECT, id, device.getAddress());
					break;
				case BluetoothProfile.STATE_DISCONNECTED:
					Log.w(TAG, "DISCONNECTED");
					connection.set(false);
					result = GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ENUMERATOR_DISCONNECT, id, device.getAddress());
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
					Log.w(TAG, "discoverServices");
					success = gatt.discoverServices();
				} else {
					Log.e(TAG, "!discoverServices");
				}
			} catch (Exception exception) {
				GodotLib.printStackTrace(exception);
			}
		} else {
			Log.w(TAG, "Null result!");
		}
		if (!success) {
			Log.w(TAG, "No success!");
		}
	}

	@SuppressLint("MissingPermission")
	@TargetApi(Build.VERSION_CODES.S)
	@Override
	public void onServiceChanged(@NonNull BluetoothGatt gatt) {
		Log.w(TAG, "onServiceChanged");
		try {
			if (gatt != null) {
				gatt.discoverServices();
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		super.onServiceChanged(gatt);
	}

	@Override
	public void onServicesDiscovered(BluetoothGatt gatt, int status) {
		Log.w(TAG, "onServicesDiscovered");
		if (device == null) {
			super.onServicesDiscovered(gatt, status);
			return;
		}
		if (gatt != null) {
			List<BluetoothGattService> services = gatt.getServices();
			if (services != null) {
				HashMap<String, HashMap<String, GodotBluetoothCharacteristic>> serviceCharacteristics = new HashMap<String, HashMap<String, GodotBluetoothCharacteristic>>();
				ArrayList<GodotBluetoothCharacteristic> allCharacteristics = new ArrayList<GodotBluetoothCharacteristic>();
				for (int i = 0; i < services.size(); ++i) {
					BluetoothGattService service = services.get(i);
					if (service != null) {
						List<BluetoothGattCharacteristic> characteristics = service.getCharacteristics();
						if (characteristics != null) {
							String serviceUuid = service.getUuid().toString();
							if (!serviceCharacteristics.containsKey(serviceUuid)) {
								serviceCharacteristics.put(serviceUuid.toLowerCase(), new HashMap<String, GodotBluetoothCharacteristic>());
							}
							for (int j = 0; j < characteristics.size(); ++j) {
								BluetoothGattCharacteristic characteristic = characteristics.get(j);
								if (characteristic != null) {
									try {
										boolean permission = false;
										String characteristicUuid = characteristic.getUuid().toString();
										if ((characteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_READ) == 0) {
											continue;
										}
										if ((characteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_WRITE) != 0) {
											permission = true;
										}
										allCharacteristics.add(new GodotBluetoothCharacteristic(service, characteristic, permission));
										if (!serviceCharacteristics.get(serviceUuid).containsKey(characteristicUuid.toLowerCase())) {
											serviceCharacteristics.get(serviceUuid).put(characteristicUuid.toLowerCase(), allCharacteristics.get(allCharacteristics.size() - 1));
										}
									} catch (Exception exceptionInner) {
										GodotLib.printStackTrace(exceptionInner);
									}
								}
							}
						}
					}
				}
				if (!serviceCharacteristics.isEmpty()) {
					lock.lock();
					try {
						characteristics = serviceCharacteristics;
					} catch (Exception exceptionOuter) {
						GodotLib.printStackTrace(exceptionOuter);
					} finally {
						lock.unlock();
					}
				}
				if (!allCharacteristics.isEmpty()) {
					for (int i = 0; i < allCharacteristics.size(); ++i) {
						try {
							GodotBluetoothCharacteristic serviceCharacteristic = allCharacteristics.get(i);
							String serviceUuid = serviceCharacteristic.getService().getUuid().toString();
							String characteristicUuid = serviceCharacteristic.getCharacteristic().getUuid().toString();
							boolean permission = serviceCharacteristic.getPermission();
							GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_DISCOVER_SERVICE_CHARACTERISTIC, id, new String[] { device.getAddress(), serviceUuid, characteristicUuid, Boolean.toString(permission), "" });
						} catch (Exception exceptionOuter) {
							GodotLib.printStackTrace(exceptionOuter);
						}
					}
				}
			}
		}
		super.onServicesDiscovered(gatt, status);
	}
}
