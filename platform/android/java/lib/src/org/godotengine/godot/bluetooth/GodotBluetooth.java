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

import org.godotengine.godot.*;
import org.godotengine.godot.utils.PermissionsUtil;

import android.Manifest;
import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReentrantLock;

public class GodotBluetooth {
	public enum BluetoothReason {
		ADVERTISING,
		CONNECTING,
		SCANNING,
	}

	private static final String TAG = GodotBluetooth.class.getSimpleName();
	// Note: These constants must be in sync with BluetoothAndroid::BluetoothEvent enum from "platform/android/bluetooth_android.h".
	public static final int EVENT_GET_SOUGHT_SERVICES = 0;
	public static final int EVENT_ON_STOP_SCANNING = 1;
	public static final int EVENT_ON_START_SCANNING = 2;
	public static final int EVENT_ON_DISCOVER = 3;
	public static final int EVENT_ON_ENUMERATOR_CONNECT = 4;
	public static final int EVENT_ON_ENUMERATOR_DISCONNECT = 5;
	public static final int EVENT_ON_DISCOVER_SERVICE_CHARACTERISTIC = 6;
	public static final int EVENT_ON_ENUMERATOR_READ = 7;
	public static final int EVENT_ON_ENUMERATOR_WRITE = 8;
	public static final int EVENT_ON_ENUMERATOR_ERROR = 9;
	public static final int EVENT_GET_NAME = 10;
	public static final int EVENT_GET_MANUFACTURER_DATA = 11;
	public static final int EVENT_GET_SERVICES = 12;
	public static final int EVENT_GET_CHARACTERISTICS = 13;
	public static final int EVENT_GET_CHARACTERISTIC_PERMISSIONS = 14;
	public static final int EVENT_GET_CHARACTERISTIC_VALUE = 15;
	public static final int EVENT_ON_ADVERTISER_ERROR = 16;
	public static final int EVENT_ON_ADVERTISER_CONNECT = 17;
	public static final int EVENT_ON_ADVERTISER_DISCONNECT = 18;
	public static final int EVENT_ON_ADVERTISER_READ = 19;
	public static final int EVENT_ON_ADVERTISER_WRITE = 20;
	public static final int EVENT_ON_START_BROADCASTING = 21;
	public static final int EVENT_ON_STOP_BROADCASTING = 22;

	private Activity activity;
	private Context context;

	private AtomicBoolean supported;
	private AtomicBoolean power;
	private ReentrantLock lock;
	private BluetoothAdapter adapter;
	@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
	private BluetoothManager manager;
	private HashMap<Integer, GodotBluetoothAdvertiser> advertisers;
	private HashMap<Integer, GodotBluetoothEnumerator> enumerators;
	private BroadcastReceiver receiver;

	public GodotBluetooth(Context p_context) {
		activity = null;
		adapter = null;
		context = p_context;
		advertisers = new HashMap<Integer, GodotBluetoothAdvertiser>();
		enumerators = new HashMap<Integer, GodotBluetoothEnumerator>();
		lock = new ReentrantLock();
		receiver = null;
		power = new AtomicBoolean(false);
		supported = new AtomicBoolean(false);
	}

	public boolean initialize(Activity p_activity) {
		activity = p_activity;
		if (context == null) {
			context = activity;
		}
		supported.set(true);
		try {
			if (Build.VERSION.SDK_INT > Build.VERSION_CODES.JELLY_BEAN_MR1) {
				if (activity == null || !activity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
					Log.w(TAG, "No activity or feature!");
					supported.set(false);
				}
			}
			if (supported.get()) {
				supported.set(false);
				if (activity != null && GodotLib.hasFeature("bluetooth_module")) {
					if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.JELLY_BEAN_MR1) {
						adapter = BluetoothAdapter.getDefaultAdapter();
						if (adapter != null) {
							supported.set(true);
						} else {
							Log.w(TAG, "No adapter!");
						}
					} else {
						manager = (BluetoothManager)activity.getSystemService(Context.BLUETOOTH_SERVICE);
						if (manager != null) {
							adapter = manager.getAdapter();
							if (adapter != null) {
								supported.set(true);
							} else {
								Log.w(TAG, "No multiple advertisement support!");
							}
						} else {
							Log.w(TAG, "No manager!");
						}
					}
					if (adapter != null && receiver == null) {
						receiver = new GodotBluetoothReceiver(this);
					}
				} else {
					Log.w(TAG, "No activity or module!");
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
			supported.set(false);
		}
		if (!supported.get()) {
			Log.w(TAG, "Got no support!");
		} else if (adapter != null && adapter.isEnabled()) {
			power.set(true);
		}
		return supported.get();
	}

	public Activity getActivity() {
		return activity;
	}

	public Context getContext() {
		return context;
	}

	public BluetoothManager getManager() {
		return manager;
	}

	@SuppressLint("HardwareIds")
	public String getAddress() {
		String address = null;
		try {
			BluetoothAdapter adapter = getAdapter(BluetoothReason.CONNECTING);
			if (adapter != null) {
				address = adapter.getAddress();
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		if (address == null) {
			address = "";
		}
		return address;
	}

	@SuppressLint("MissingPermission")
	public String getName() {
		String name = null;
		try {
			BluetoothAdapter adapter = getAdapter(BluetoothReason.CONNECTING);
			if (adapter != null) {
				name = adapter.getName();
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		if (name == null) {
			name = "";
		}
		return name;
	}

	public BluetoothAdapter getAdapter(BluetoothReason reason) {
		if (!supported.get()) {
			Log.w(TAG, "No support!");
			return null;
		}
		if (reason != null && Build.VERSION.SDK_INT > Build.VERSION_CODES.R) { // Bluetooth permissions are only considered dangerous at runtime above API level 30
			switch (reason) {
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
				case SCANNING:
					if (ContextCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
						if (!PermissionsUtil.requestPermission("BLUETOOTH_SCAN", activity)) {
							return null;
						}
					}
					break;
				default:
					return null;
			}
		}
		if (adapter == null) {
			if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.JELLY_BEAN_MR1) {
				adapter = BluetoothAdapter.getDefaultAdapter();
			} else {
				if (manager == null) {
					manager = (BluetoothManager)activity.getSystemService(Context.BLUETOOTH_SERVICE);
				}
				if (manager != null) {
					adapter = manager.getAdapter();
				}
			}
			if (adapter != null && receiver == null) {
				receiver = new GodotBluetoothReceiver(this);
			}
		}
		return adapter;
	}

	public void setPower(boolean p_power) {
		ArrayList<GodotBluetoothAdvertiser> advertisers = getAdvertisers();
		ArrayList<GodotBluetoothEnumerator> enumerators = getEnumerators();
		Log.w(TAG, "Power = " + p_power);
		if (power.get() != p_power) {
			power.set(p_power);
			if (advertisers != null) {
				for (int i = 0; i < advertisers.size(); ++i) {
					GodotBluetoothAdvertiser advertiser = advertisers.get(i);
					if (advertiser == null) {
						continue;
					}
					advertiser.onPower();
				}
			}
			if (enumerators != null) {
				for (int i = 0; i < enumerators.size(); ++i) {
					GodotBluetoothEnumerator enumerator = enumerators.get(i);
					if (enumerator == null) {
						continue;
					}
					enumerator.onPower();
				}
			}
		}
	}

	public boolean getPower() {
		return power.get();
	}

	public boolean isSupported(boolean p_role) {
		if (!supported.get()) {
			if (adapter == null) {
				supported.set(true);
				if (p_role) {
					adapter = getAdapter(BluetoothReason.ADVERTISING);
				} else {
					adapter = getAdapter(BluetoothReason.SCANNING);
				}
				supported.set(initialize(activity));
			}
			if (!supported.get()) {
				Log.w(TAG, "Get no support!");
			}
		}
		if (supported.get()) {
			if (!power.get() && adapter != null) {
				if (adapter.isEnabled()) {
					power.set(true);
				} else if (activity != null) {
					if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.R || ContextCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED || PermissionsUtil.requestPermission("BLUETOOTH_CONNECT", activity)) {
						Intent intent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
						activity.startActivityForResult(intent, PermissionsUtil.REQUEST_BLUETOOTH_ENABLE_PERMISSION);
					}
				}
			}
			if (!power.get() || (p_role && !adapter.isMultipleAdvertisementSupported())) {
				return false;
			}
		}
		return supported.get();
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
				Log.w(TAG, "No enumerator @ " + p_enumerator_id);
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
		}
		if (success) {
			Log.w(TAG, "Connect!");
		} else {
			Log.w(TAG, "No connect!");
		}
		return success;
	}

	public boolean readEnumeratorCharacteristic(int p_enumerator_id, String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) {
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
			success = enumerator.readCharacteristic(p_peer_uuid, p_service_uuid, p_characteristic_uuid);
		}
		return success;
	}

	public boolean writeEnumeratorCharacteristic(int p_enumerator_id, String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value) {
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
			success = enumerator.writeCharacteristic(p_peer_uuid, p_service_uuid, p_characteristic_uuid, p_value);
		}
		return success;
	}

	public boolean respondAdvertiserCharacteristicReadRequest(int p_advertiser_id, String p_characteristic_uuid, String p_response, int p_request) {
		GodotBluetoothAdvertiser advertiser = null;
		boolean success = false;
		if (p_characteristic_uuid == null) {
			return success;
		}
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
			success = advertiser.respondReadRequest(p_characteristic_uuid, p_response, p_request);
		}
		return success;
	}

	public boolean respondAdvertiserCharacteristicWriteRequest(int p_advertiser_id, String p_characteristic_uuid, String p_response, int p_request) {
		GodotBluetoothAdvertiser advertiser = null;
		boolean success = false;
		if (p_characteristic_uuid == null) {
			return success;
		}
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
			success = advertiser.respondWriteRequest(p_characteristic_uuid, p_response, p_request);
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
			Log.d(TAG, "New advertiser @ " + p_advertiser_id);
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
			Log.d(TAG, "New enumerator @ " + p_enumerator_id);
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
			Log.d(TAG, "Old advertiser @ " + p_advertiser_id);
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
			Log.d(TAG, "Old enumerator @ " + p_enumerator_id);
		}
		return success;
	}

	public ArrayList<GodotBluetoothAdvertiser> getAdvertisers() {
		ArrayList<GodotBluetoothAdvertiser> advertisers = new ArrayList<GodotBluetoothAdvertiser>();
		lock.lock();
		try {
			Set<Map.Entry<Integer, GodotBluetoothAdvertiser>> entries = this.advertisers.entrySet();
			if (entries != null) {
				Iterator<Map.Entry<Integer, GodotBluetoothAdvertiser>> iterator = entries.iterator();
				if (iterator != null) {
					while (iterator.hasNext()) {
						Map.Entry<Integer, GodotBluetoothAdvertiser> entry = iterator.next();
						if (entry == null) {
							continue;
						}
						advertisers.add(entry.getValue());
					}
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		return advertisers;
	}

	public ArrayList<GodotBluetoothEnumerator> getEnumerators() {
		ArrayList<GodotBluetoothEnumerator> enumerators = new ArrayList<GodotBluetoothEnumerator>();
		lock.lock();
		try {
			Set<Map.Entry<Integer, GodotBluetoothEnumerator>> entries = this.enumerators.entrySet();
			if (entries != null) {
				Iterator<Map.Entry<Integer, GodotBluetoothEnumerator>> iterator = entries.iterator();
				if (iterator != null) {
					while (iterator.hasNext()) {
						Map.Entry<Integer, GodotBluetoothEnumerator> entry = iterator.next();
						if (entry == null) {
							continue;
						}
						enumerators.add(entry.getValue());
					}
				}
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		} finally {
			lock.unlock();
		}
		return enumerators;
	}
}
