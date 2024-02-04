/**************************************************************************/
/*  GodotBluetoothAdvertisement.java                                      */
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

import org.godotengine.godot.GodotLib;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.os.ParcelUuid;
import android.util.Base64;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.UUID;

public class GodotBluetoothAdvertisement extends AdvertiseCallback {
	private static final String TAG = GodotBluetoothAdvertisement.class.getSimpleName();

	private GodotBluetoothAdvertiser advertiser;
	private AdvertiseData data;
	private AdvertiseSettings settings;
	private ArrayList<GodotBluetoothCharacteristic> characteristics;
	private HashMap<String, BluetoothGattService> services;

	public GodotBluetoothAdvertisement(GodotBluetoothAdvertiser p_advertiser) {
		super();
		advertiser = p_advertiser;
		data = null;
		settings = null;
		characteristics = null;
		services = null;
	}

	@SuppressLint("MissingPermission")
	public boolean startAdvertising() {
		boolean success = true;
		try {
			BluetoothLeAdvertiser advertiser = this.advertiser.getAdvertiser();
			AdvertiseData data = getData();
			AdvertiseSettings settings = getSettings();
			if (advertiser != null && data != null && settings != null) {
				advertiser.startAdvertising(settings, (new AdvertiseData.Builder()).setIncludeDeviceName(false).build(), data, this);
			} else {
				success = false;
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
			success = false;
		}
		return success;
	}

	@SuppressLint("MissingPermission")
	public boolean stopAdvertising() {
		boolean success = true;
		try {
			BluetoothLeAdvertiser advertiser = this.advertiser.getAdvertiser();
			if (advertiser != null) {
				data = null;
				settings = null;
				characteristics = null;
				advertiser.stopAdvertising(this);
			} else {
				success = false;
			}
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
			success = false;
		}
		return success;
	}

	public AdvertiseData getData() {
		if (data == null) {
			String manufacturer = (String)GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_MANUFACTURER_DATA, advertiser.getIdentifier(), null);
			AdvertiseData.Builder builder = new AdvertiseData.Builder();
			String[] services = (String[])GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_SERVICES, advertiser.getIdentifier(), null);
			if (services == null) {
				return null;
			}
			this.services = new HashMap<String, BluetoothGattService>();
			for (int i = 0; i < services.length; ++i) {
				if (services[i] == null) {
					continue;
				}
				services[i] = services[i].toLowerCase();
				if (this.services.containsKey(services[i])) {
					continue;
				}
				this.services.put(services[i], new BluetoothGattService(UUID.fromString(services[i]), BluetoothGattService.SERVICE_TYPE_PRIMARY));
				builder.addServiceUuid(new ParcelUuid(this.services.get(services[i]).getUuid()));
			}
			if (manufacturer != null) {
				byte[] manufacturerData = Base64.decode(manufacturer, 0);
				if (manufacturerData != null && manufacturerData.length > 2) {
					String company = String.format("%02x", manufacturerData[0]) + String.format("%02x", manufacturerData[1]);
					if (manufacturerData.length > 8) {
						manufacturerData = Arrays.copyOfRange(manufacturerData, 0, 8);
					}
					Log.w(TAG, "Company @ \""+company+"\" = "+Base64.encodeToString(manufacturerData, Base64.DEFAULT).trim());
					//builder.addManufacturerData((int)Short.parseShort(company, 16), Arrays.copyOfRange(manufacturerData, 2, manufacturerData.length - 2));
				}
			}
			builder.setIncludeDeviceName(false); // don't include name, because if name size > 8 bytes, ADVERTISE_FAILED_DATA_TOO_LARGE
			//builder.setIncludeTxPowerLevel(true);
			data = builder.build();
		}
		return data;
	}

	public AdvertiseSettings getSettings() {
		if (settings == null) {
			AdvertiseSettings.Builder builder = new AdvertiseSettings.Builder();
			builder.setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM);
			builder.setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_BALANCED);
			builder.setConnectable(true);
			builder.setTimeout(0);
			settings = builder.build();
		}
		return settings;
	}

	public ArrayList<GodotBluetoothCharacteristic> getCharacteristics() {
		ArrayList<BluetoothGattService> services = getServices();
		if (characteristics == null && services != null) {
			int[] permissions = (int[])GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_CHARACTERISTIC_PERMISSIONS, advertiser.getIdentifier(), null);
			String[] characteristic = (String[])GodotLib.bluetoothCallback(GodotBluetooth.EVENT_GET_CHARACTERISTICS, advertiser.getIdentifier(), null);
			//Log.i(TAG, "PERMISSIONS = \""+((permissions != null)?Arrays.toString(Arrays.copyOf(permissions, permissions.length, Object[].class)):"")+"\"");
			//Log.i(TAG, "CHARACTERISTIC = \""+((characteristic != null)?Arrays.toString(Arrays.copyOf(characteristic, characteristic.length, Object[].class)):"")+"\"");
			if (permissions == null || characteristic == null || permissions.length != characteristic.length) {
				return null;
			}
			characteristics = new ArrayList<GodotBluetoothCharacteristic>();
			for (int i = 0; i < characteristic.length; ++i) {
				for (int j = 0; j < services.size(); ++j) {
					int property = BluetoothGattCharacteristic.PROPERTY_READ;
					int permission = BluetoothGattCharacteristic.PERMISSION_READ;
					if (permissions[i] != 0) {
						property |= BluetoothGattCharacteristic.PROPERTY_WRITE;
						permission |= BluetoothGattCharacteristic.PERMISSION_WRITE;
					}
					characteristics.add(new GodotBluetoothCharacteristic(services.get(j), new BluetoothGattCharacteristic(UUID.fromString(characteristic[i]), property, permission), permissions[i] != 0));
					characteristics.get(characteristics.size() - 1).getCharacteristic().setValue(new byte[0]);
					/*if (permissions[i] != 0) {
						characteristics.get(characteristics.size() - 1).getCharacteristic().setWriteType(BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
					}*/
					services.get(j).addCharacteristic(characteristics.get(characteristics.size() - 1).getCharacteristic());
				}
			}
		}
		return characteristics;
	}

	public ArrayList<BluetoothGattService> getServices() {
		if (this.services != null) {
			ArrayList<BluetoothGattService> services = new ArrayList<BluetoothGattService>();
			Iterator<Map.Entry<String, BluetoothGattService>> iterator = this.services.entrySet().iterator();
			while (iterator.hasNext()) {
				services.add(iterator.next().getValue());
			}
			return services;
		}
		return null;
	}

	public GodotBluetoothAdvertiser getAdvertiser() {
		return advertiser;
	}

	@Override
	public void onStartFailure(int errorCode) {
		String message = "";
		switch (errorCode) {
			case ADVERTISE_FAILED_ALREADY_STARTED:
				message += "ADVERTISE_FAILED_ALREADY_STARTED";
				break;
			case ADVERTISE_FAILED_DATA_TOO_LARGE:
				message += "ADVERTISE_FAILED_DATA_TOO_LARGE";
				break;
			case ADVERTISE_FAILED_FEATURE_UNSUPPORTED:
				message += "ADVERTISE_FAILED_FEATURE_UNSUPPORTED";
				break;
			case ADVERTISE_FAILED_INTERNAL_ERROR:
				message += "ADVERTISE_FAILED_INTERNAL_ERROR";
				break;
			case ADVERTISE_FAILED_TOO_MANY_ADVERTISERS:
				message += "ADVERTISE_FAILED_TOO_MANY_ADVERTISERS";
				break;
			default:
				break;
		}
		try {
			GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_ADVERTISER_ERROR, getAdvertiser().getIdentifier(), message);
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		super.onStartFailure(errorCode);
	}

	@Override
	public void onStartSuccess(AdvertiseSettings settingsInEffect) {
		try {
			GodotLib.bluetoothCallback(GodotBluetooth.EVENT_ON_START_BROADCASTING, getAdvertiser().getIdentifier(), "");
		} catch (Exception exception) {
			GodotLib.printStackTrace(exception);
		}
		super.onStartSuccess(settingsInEffect);
	}
}
