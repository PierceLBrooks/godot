/**************************************************************************/
/*  GodotBluetoothEnumerator.java                                         */
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
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.os.Build;
import android.os.ParcelUuid;
import android.util.Base64;
import android.util.Log;
import android.util.SparseArray;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReentrantLock;

import org.godotengine.godot.*;
import org.json.JSONObject;

public class GodotBluetoothEnumerator extends ScanCallback
{
	private static final String TAG = GodotBluetoothEnumerator.class.getSimpleName();

	private final GodotBluetooth bluetooth;
	private final int id;

	private HashMap<String, GodotBluetoothPeer> peers;
	private String[] sought_services;
	private BluetoothLeScanner scanner;
	private AtomicBoolean scanning;
	private ReentrantLock lock;

	public GodotBluetoothEnumerator(GodotBluetooth p_bluetooth, int p_id) {
		bluetooth = p_bluetooth;
		id = p_id;
		sought_services = null;
		scanner = null;
		scanning = new AtomicBoolean(false);
		lock = new ReentrantLock();
	}

	public int getIdentifier() {
		return id;
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

	public boolean connectPeer(String p_peer_uuid) {
		boolean success = false;
		lock.lock();
		try {
			if (peers.containsKey(p_peer_uuid)) {
				GodotBluetoothPeer peer = peers.get(p_peer_uuid);
				if (peer != null) {
					success = peer.connect(bluetooth);
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		return success;
	}

	@SuppressLint("MissingPermission")
	public boolean startScanning() {
		if (scanning.get()) {
			Log.w(TAG, "Already scanning!");
			return false;
		}
		try {
			BluetoothAdapter adapter = bluetooth.getAdapter(GodotBluetooth.BluetoothReason.SCANNING);
			if (adapter == null) {
				Log.w(TAG, "No adapter!");
				return false;
			}
			sought_services = (String[])GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_SOUGHT_SERVICES, id, null);
			if (sought_services == null) {
				Log.w(TAG, "No sought services!");
				return false;
			}
			scanner = adapter.getBluetoothLeScanner();
			if (scanner == null) {
				Log.w(TAG, "No scanner!");
				return false;
			}
			if (sought_services.length > 0) {
				ArrayList<ScanFilter> filters = new ArrayList<ScanFilter>();
				for (int idx = 0; idx < sought_services.length; ++idx) {
					ScanFilter.Builder builder = new ScanFilter.Builder();
					ParcelUuid uuid = new ParcelUuid(UUID.nameUUIDFromBytes(sought_services[idx].getBytes()));
					builder.setServiceUuid(uuid);
					ScanFilter filter = builder.build();
					if (filter == null) {
						continue;
					}
					filters.add(filter);
				}
				if (!filters.isEmpty()) {
					ScanSettings.Builder builder = new ScanSettings.Builder();
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
						builder.setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES);
					}
					builder.setScanMode(ScanSettings.SCAN_MODE_BALANCED);
					ScanSettings settings = builder.build();
					if (settings != null) {
						scanner.startScan(filters, settings, this);
					} else {
						scanner.startScan(this);
					}
				} else {
					scanner.startScan(this);
				}
			} else {
				scanner.startScan(this);
			}
			scanning.set(true);
			processResult(GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_START_SCANNING, id, null));
			return true;
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		return false;
	}

	public boolean stopScanning() {
		boolean success = false;
		lock.lock();
		try {
			success = stopScanning(false);
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		return success;
	}

	@SuppressLint("MissingPermission")
	private boolean stopScanning(boolean force) {
		boolean scanning = this.scanning.get();
		if ((!force && !scanning) || scanner == null) {
			return false;
		}
		try {
			if (scanning) {
				this.scanning.set(false);
				processResult(GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_STOP_SCANNING, id, null));
			}
			scanner.stopScan(this);
			scanner = null;
			return true;
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		return false;
	}

	private void onScanResult(ScanResult result) {
		Boolean success = null;
		BluetoothDevice device = null;
		String address = "";
		lock.lock();
		try {
			if (result != null) {
				ScanRecord record = result.getScanRecord();
				device = result.getDevice();
				if (record != null && device != null) {
					String name = record.getDeviceName();
					address = device.getAddress();
					if (name != null && address != null) {
						HashMap<String, Object> data = new HashMap<String, Object>();
						Dictionary argument = new Dictionary();
						SparseArray<byte[]> manufacturer = record.getManufacturerSpecificData();
						argument.put("address", address);
						argument.put("name", name);
						if (manufacturer != null) {
							HashMap<String, String> encoding = new HashMap<String, String>();
							for (int idx = 0; idx < manufacturer.size(); ++idx) {
								int key = manufacturer.keyAt(idx);
								byte[] value = manufacturer.get(key, new byte[]{});
								if (value.length > 0) {
									encoding.put(String.valueOf(key), Base64.encodeToString(value, 0).strip());
								}
							}
							if (!encoding.isEmpty()) {
								data.put("manufacturer", encoding);
							}
						}
						if (!data.isEmpty()) {
							JSONObject object = (JSONObject)JSONObject.wrap(data);
							if (object != null) {
								argument.put("data", object.toString().strip());
							}
						}
						success = processResult(GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_DISCOVER, id, argument));
					}
				}
			}
			if (success != null) {
				if (success.booleanValue()) {
					if (peers.containsKey(address)) {
						Log.w(TAG, "Double discovery?");
					} else {
						GodotBluetoothPeer peer = new GodotBluetoothPeer(getIdentifier(), device);
						peers.put(address, peer);
					}
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}

	}

	@Override
	public void onScanResult(int callbackType, ScanResult result) {
		super.onScanResult(callbackType, result);
		if (callbackType != ScanSettings.CALLBACK_TYPE_MATCH_LOST) {
			onScanResult(result);
		}
	}

	@Override
	public void onBatchScanResults(List<ScanResult> results) {
		super.onBatchScanResults(results);
		if (results != null) {
			for (int idx = 0; idx < results.size(); ++idx) {
				onScanResult(results.get(idx));
			}
		}
	}

	@Override
	public void onScanFailed(int errorCode) {
		super.onScanFailed(errorCode);
		lock.lock();
		try {
			if (scanning.get()) {
				scanning.set(false);
				stopScanning(true);
				processResult(GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_STOP_SCANNING, id, null));
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
	}
}

