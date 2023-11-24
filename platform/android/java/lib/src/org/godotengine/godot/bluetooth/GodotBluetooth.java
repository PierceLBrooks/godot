/**************************************************************************/
/*  GodotBluetooth.java                                                   */
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

import android.Manifest;
import android.annotation.TargetApi;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import androidx.core.content.ContextCompat;

import java.util.HashMap;
import java.util.concurrent.locks.ReentrantLock;

import org.godotengine.godot.utils.PermissionsUtil;
import org.godotengine.godot.*;

public class GodotBluetooth {
	public enum BluetoothReason {
		SCANNING,
		ADVERTISING,
		CONNECTING,
	}

	private static final String TAG = GodotBluetooth.class.getSimpleName();
	// Note: These constants must be in sync with BluetoothAndroid::BluetoothEvent enum from "platform/android/bluetooth_android.h".
	public static final int EVENT_GET_SOUGHT_SERVICES = 0;
	public static final int EVENT_ON_STOP_SCANNING = 1;
	public static final int EVENT_ON_START_SCANNING = 2;
	public static final int EVENT_ON_DISCOVER = 3;
	public static final int EVENT_ON_CONNECT = 4;
	public static final int EVENT_ON_DISCONNECT = 5;
	public static final int EVENT_ON_DISCOVER_SERVICE_CHARACTERISTIC = 6;

	private final Activity activity;

	private boolean supported;
	private ReentrantLock lock;
	private BluetoothAdapter adapter;
	@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
	private BluetoothManager manager;
	private HashMap<Integer, GodotBluetoothAdvertiser> advertisers;
	private HashMap<Integer, GodotBluetoothEnumerator> enumerators;

	public GodotBluetooth(Activity p_activity) {
		activity = p_activity;
		advertisers = new HashMap<Integer, GodotBluetoothAdvertiser>();
		enumerators = new HashMap<Integer, GodotBluetoothEnumerator>();
		lock = new ReentrantLock();
		supported = false;
	}

	public void initialize() {
		supported = true;
		try {
			if (Build.VERSION.SDK_INT > Build.VERSION_CODES.JELLY_BEAN_MR1) {
				if (activity == null || !activity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
					Log.w(TAG, "No activity or feature!");
					supported = false;
				}
			}
			if (supported) {
				supported = false;
				if (activity != null && GodotLib.hasFeature("bluetooth_module")) {
					if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.JELLY_BEAN_MR1) {
						adapter = BluetoothAdapter.getDefaultAdapter();
						if (adapter != null) {
							supported = true;
						} else {
							Log.w(TAG, "No adapter!");
						}
					} else {
						manager = (BluetoothManager)activity.getSystemService(activity.BLUETOOTH_SERVICE);
						if (manager != null) {
							adapter = manager.getAdapter();
							if (adapter != null && adapter.isMultipleAdvertisementSupported()) {
								supported = true;
							} else {
								Log.w(TAG, "No multiple advertisement support!");
							}
						} else {
							Log.w(TAG, "No manager!");
						}
					}
				} else {
					Log.w(TAG, "No activity or module!");
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
			supported = false;
		}
		if (!supported) {
			Log.w(TAG, "Got no support!");
		}
	}

	public Activity getActivity() {
		return activity;
	}

	public BluetoothAdapter getAdapter(BluetoothReason reason) {
		if (!supported) {
			Log.w(TAG, "No support!");
			return null;
		}
		if (reason != null && Build.VERSION.SDK_INT > Build.VERSION_CODES.R) { // Bluetooth permissions are only considered dangerous at runtime above API level 30
			switch (reason) {
				case SCANNING:
					if (ContextCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
						if (!PermissionsUtil.requestPermission("BLUETOOTH_SCAN", activity)) {
							return null;
						}
					}
					break;
				case ADVERTISING:
					if (ContextCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_ADVERTISE) != PackageManager.PERMISSION_GRANTED) {
						if (!PermissionsUtil.requestPermission("BLUETOOTH_ADVERTISE", activity)) {
							return null;
						}
					}
					break;
				case CONNECTING:
					if (ContextCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
						if (!PermissionsUtil.requestPermission("BLUETOOTH_CONNECT", activity)) {
							return null;
						}
					}
					break;
				default:
					return null;
			}
		}
		return adapter;
	}

	public boolean isSupported() {
		if (!supported) {
			Log.w(TAG, "Get no support!");
		}
		return supported;
	}

	public boolean startAdvertising(int p_advertiser_id) {
		GodotBluetoothAdvertiser advertiser = null;
		boolean success = false;
		lock.lock();
		try {
			if (advertisers.containsKey(p_advertiser_id)) {
				advertiser = advertisers.get(p_advertiser_id);
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (advertiser != null) {
			success = advertiser.startAdvertising();
		}
		return success;
	}

	public boolean stopAdvertising(int p_advertiser_id) {
		GodotBluetoothAdvertiser advertiser = null;
		boolean success = false;
		lock.lock();
		try {
			if (advertisers.containsKey(p_advertiser_id)) {
				advertiser = advertisers.get(p_advertiser_id);
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (advertiser != null) {
			success = advertiser.stopAdvertising();
		}
		return success;
	}

	public boolean startScanning(int p_enumerator_id) {
		GodotBluetoothEnumerator enumerator = null;
		boolean success = false;
		lock.lock();
		try {
			if (enumerators.containsKey(p_enumerator_id)) {
				enumerator = enumerators.get(p_enumerator_id);
			} else {
				Log.w(TAG, "No enumerator @ "+p_enumerator_id);
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (enumerator != null) {
			success = enumerator.startScanning();
		}
		return success;
	}

	public boolean stopScanning(int p_enumerator_id) {
		GodotBluetoothEnumerator enumerator = null;
		boolean success = false;
		lock.lock();
		try {
			if (enumerators.containsKey(p_enumerator_id)) {
				enumerator = enumerators.get(p_enumerator_id);
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (enumerator != null) {
			success = enumerator.stopScanning();
		}
		return success;
	}

	public boolean connectEnumeratorPeer(int p_enumerator_id, String p_peer_uuid) {
		GodotBluetoothEnumerator enumerator = null;
		boolean success = false;
		if (p_peer_uuid == null) {
			return success;
		}
		lock.lock();
		try {
			if (enumerators.containsKey(p_enumerator_id)) {
				enumerator = enumerators.get(p_enumerator_id);
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (enumerator != null) {
			success = enumerator.connectPeer(p_peer_uuid);
		} else {
			Log.w(TAG, "Null enumerator @ "+p_peer_uuid);
		}
		return success;
	}

	public boolean registerAdvertiser(int p_advertiser_id) {
		boolean success = false;
		lock.lock();
		try {
			GodotBluetoothAdvertiser advertiser = new GodotBluetoothAdvertiser(this, p_advertiser_id);
			advertisers.put(p_advertiser_id, advertiser);
			success = true;
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (success) {
			Log.d(TAG, "New advertiser @ "+p_advertiser_id);
		}
		return success;
	}

	public boolean registerEnumerator(int p_enumerator_id) {
		boolean success = false;
		lock.lock();
		try {
			if (!enumerators.containsKey(p_enumerator_id)) {
				GodotBluetoothEnumerator enumerator = new GodotBluetoothEnumerator(this, p_enumerator_id);
				enumerators.put(p_enumerator_id, enumerator);
				success = true;
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (success) {
			Log.d(TAG, "New enumerator @ "+p_enumerator_id);
		}
		return success;
	}

	public boolean unregisterAdvertiser(int p_advertiser_id) {
		boolean success = false;
		lock.lock();
		try {
			if (advertisers.containsKey(p_advertiser_id)) {
				GodotBluetoothAdvertiser advertiser = advertisers.get(p_advertiser_id);
				advertisers.remove(p_advertiser_id);
				if (advertiser != null) {
					advertiser.stopAdvertising();
				}
				success = true;
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (success) {
			Log.d(TAG, "Old advertiser @ "+p_advertiser_id);
		}
		return success;
	}

	public boolean unregisterEnumerator(int p_enumerator_id) {
		boolean success = false;
		lock.lock();
		try {
			if (enumerators.containsKey(p_enumerator_id)) {
				GodotBluetoothEnumerator enumerator = enumerators.get(p_enumerator_id);
				enumerators.remove(p_enumerator_id);
				if (enumerator != null) {
					enumerator.stopScanning();
				}
				success = true;
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		if (success) {
			Log.d(TAG, "Old enumerator @ "+p_enumerator_id);
		}
		return success;
	}
}

