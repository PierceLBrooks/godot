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
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.os.Build;
import android.os.ParcelUuid;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicBoolean;

import org.godotengine.godot.*;

public class GodotBluetoothEnumerator extends ScanCallback
{
	private static final String TAG = GodotBluetoothEnumerator.class.getSimpleName();

	private final GodotBluetooth bluetooth;
	private final int id;

	private ArrayList<String> sought_services;
	private BluetoothLeScanner scanner;
	private AtomicBoolean scanning;

	public GodotBluetoothEnumerator(GodotBluetooth p_bluetooth, int p_id) {
		bluetooth = p_bluetooth;
		id = p_id;
		sought_services = null;
		scanner = null;
		scanning = new AtomicBoolean(false);
	}

	@SuppressLint("MissingPermission")
	public boolean startScanning() {
		if (scanning.get()) {
			return false;
		}
		try {
			int result = 0;
			BluetoothAdapter adapter = bluetooth.getAdapter(GodotBluetooth.BluetoothReason.SCANNING);
			if (adapter == null) {
				return false;
			}
			sought_services = (ArrayList<String>)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_SOUGHT_SERVICES, id);
			if (sought_services == null) {
				return false;
			}
			if (!sought_services.isEmpty()) {
				ArrayList<ScanFilter> filters = new ArrayList<ScanFilter>();
				for (int idx = 0; idx < sought_services.size(); ++idx) {
					ScanFilter.Builder builder = new ScanFilter.Builder();
					ParcelUuid uuid = new ParcelUuid(UUID.nameUUIDFromBytes(sought_services.get(idx).getBytes()));
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
				scanner = adapter.getBluetoothLeScanner();
				scanner.startScan(this);
			}
			scanning.set(true);
			GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_START_SCANNING, id);
			return true;
		} catch (Exception exception) {
			exception.printStackTrace();
		}
		return false;
	}

	public boolean stopScanning() {
		return stopScanning(false);
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
				GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_STOP_SCANNING, id);
			}
			scanner.stopScan(this);
			scanner = null;
		} catch (Exception exception) {
			exception.printStackTrace();
		}
		return false;
	}

	@Override
	public void onBatchScanResults(List<ScanResult> results) {
		super.onBatchScanResults(results);
	}

	@Override
	public void onScanResult(int callbackType, ScanResult result) {
		super.onScanResult(callbackType, result);
	}

	@Override
	public void onScanFailed(int errorCode) {
		super.onScanFailed(errorCode);
		if (scanning.get()) {
			scanning.set(false);
			stopScanning(true);
			GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_STOP_SCANNING, id);
		}
	}
}

