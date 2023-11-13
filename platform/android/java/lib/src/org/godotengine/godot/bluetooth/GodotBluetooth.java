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

import android.annotation.TargetApi;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.pm.PackageManager;
import android.os.Build;

import org.godotengine.godot.*;

public class GodotBluetooth {
	private static final String TAG = GodotBluetooth.class.getSimpleName();

	private final Activity activity;

	private boolean supported;
	private BluetoothAdapter adapter;
	@TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
	private BluetoothManager manager;

	public GodotBluetooth(Activity p_activity) {
		activity = p_activity;

		supported = true;
		try {
			if (Build.VERSION.SDK_INT > Build.VERSION_CODES.JELLY_BEAN_MR1) {
				if (activity == null || !activity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
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
						}
					} else {
						manager = (BluetoothManager)activity.getSystemService(activity.BLUETOOTH_SERVICE);
						if (manager != null) {
							adapter = manager.getAdapter();
							if (adapter != null && adapter.isMultipleAdvertisementSupported()) {
								supported = true;
							}
						}
					}
				}
			}
		} catch (Exception exception) {
			exception.printStackTrace();
			supported = false;
		}
	}

	public boolean isSupported() {
		return supported;
	}
}

