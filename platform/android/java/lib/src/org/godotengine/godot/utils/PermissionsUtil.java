/**************************************************************************/
/*  PermissionsUtil.java                                                  */
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

package org.godotengine.godot.utils;

import org.godotengine.godot.BuildConfig;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;

import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * This class includes utility functions for Android permissions related operations.
 */
public final class PermissionsUtil {
	private static final String TAG = PermissionsUtil.class.getSimpleName();

	public static final int REQUEST_RECORD_AUDIO_PERMISSION = 1;
	public static final int REQUEST_CAMERA_PERMISSION = 2;
	public static final int REQUEST_VIBRATE_PERMISSION = 3;
	public static final int REQUEST_BLUETOOTH_ADVERTISE_PERMISSION = 4;
	public static final int REQUEST_BLUETOOTH_CONNECT_PERMISSION = 5;
	public static final int REQUEST_BLUETOOTH_ENABLE_PERMISSION = 6;
	public static final int REQUEST_BLUETOOTH_LOCATION_PERMISSION = 7;
	public static final int REQUEST_BLUETOOTH_SCAN_PERMISSION = 8;
	public static final int REQUEST_ALL_PERMISSION_REQ_CODE = 1001;
	public static final int REQUEST_SINGLE_PERMISSION_REQ_CODE = 1002;
	public static final int REQUEST_MANAGE_EXTERNAL_STORAGE_REQ_CODE = 2002;

	private PermissionsUtil() {
	}

	/**
	 * Alias/wrapper for ActivityCompat.requestPermissions
	 * @param activity the caller activity for this method.
	 * @param permissions the names of permissions to be requested.
	 * @param code the request code of the permission grant/rejection response.
	 * @return true/false. "false" if any of the permissions did indeed need to be requested otherwise returns "true".
	 */
	public static boolean requestPermissions(Activity activity, String[] permissions, int code) {
		List<String> names = new ArrayList<>();
		if (permissions != null) {
			for (String permission : permissions) {
				try {
					if (permission != null && !permission.isEmpty() && hasManifestPermission(activity, permission) && isPermissionDangerous(activity, permission))
						names.add(permission);
				} catch (PackageManager.NameNotFoundException ignored) {
				}
			}
		}
		if (!names.isEmpty()) {
			ActivityCompat.requestPermissions(activity, names.toArray(new String[0]), code);
			return false;
		}
		return true;
	}

	/**
	 * Request a dangerous permission. name must be specified in <a href="https://github.com/aosp-mirror/platform_frameworks_base/blob/master/core/res/AndroidManifest.xml">this</a>
	 * @param permissionName the name of the requested permission.
	 * @param activity the caller activity for this method.
	 * @return true/false. "true" if permission was granted otherwise returns "false".
	 */
	public static boolean requestPermission(String permissionName, Activity activity) {
		if ("debug".equalsIgnoreCase(BuildConfig.BUILD_TYPE)) {
			Log.i(TAG, "Request permission @ " + permissionName);
		}

		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
			// Not necessary, asked on install already
			return true;
		}

		switch (permissionName) {
			case "RECORD_AUDIO":
			case Manifest.permission.RECORD_AUDIO:
				if (ContextCompat.checkSelfPermission(activity, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
					return requestPermissions(activity, new String[] { Manifest.permission.RECORD_AUDIO }, REQUEST_RECORD_AUDIO_PERMISSION);
				}
				return true;

			case "CAMERA":
			case Manifest.permission.CAMERA:
				if (ContextCompat.checkSelfPermission(activity, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
					return requestPermissions(activity, new String[] { Manifest.permission.CAMERA }, REQUEST_CAMERA_PERMISSION);
				}
				return true;

			case "VIBRATE":
			case Manifest.permission.VIBRATE:
				if (ContextCompat.checkSelfPermission(activity, Manifest.permission.VIBRATE) != PackageManager.PERMISSION_GRANTED) {
					return requestPermissions(activity, new String[] { Manifest.permission.VIBRATE }, REQUEST_VIBRATE_PERMISSION);
				}
				return true;

			case "BLUETOOTH":
			case "BLUETOOTH_ADMIN":
			case "BLUETOOTH_ADVERTISE":
			case "BLUETOOTH_CONNECT":
			case "BLUETOOTH_SCAN":
			case Manifest.permission.BLUETOOTH:
			case Manifest.permission.BLUETOOTH_ADMIN:
			case Manifest.permission.BLUETOOTH_ADVERTISE:
			case Manifest.permission.BLUETOOTH_CONNECT:
			case Manifest.permission.BLUETOOTH_SCAN:
				if (Build.VERSION.SDK_INT > Build.VERSION_CODES.R) { // Bluetooth permissions are only considered dangerous at runtime above API level 30
					int request = 0;
					boolean connect = false;
					boolean location = false;
					String[] permissions = null;
					if (ContextCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
						connect = true;
					}
					if (ContextCompat.checkSelfPermission(activity, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
						location = true;
					}
					if (permissionName.equals("BLUETOOTH_ADVERTISE") || permissionName.equals(Manifest.permission.BLUETOOTH_ADVERTISE)) {
						if (ContextCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_ADVERTISE) != PackageManager.PERMISSION_GRANTED) {
							request = REQUEST_BLUETOOTH_ADVERTISE_PERMISSION;
							if (connect) {
								permissions = new String[] { Manifest.permission.BLUETOOTH_ADVERTISE, Manifest.permission.BLUETOOTH_CONNECT };
							} else {
								permissions = new String[] { Manifest.permission.BLUETOOTH_ADVERTISE };
							}
						}
					} else if (permissionName.equals("BLUETOOTH_SCAN") || permissionName.equals(Manifest.permission.BLUETOOTH_SCAN)) {
						if (ContextCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
							request = REQUEST_BLUETOOTH_SCAN_PERMISSION;
							if (connect) {
								permissions = new String[] { Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT };
							} else {
								permissions = new String[] { Manifest.permission.BLUETOOTH_SCAN };
							}
						}
					}
					if (permissions != null) {
						if (location) {
							String[] temp = permissions;
							permissions = new String[temp.length + 1];
							System.arraycopy(temp, 0, permissions, 0, temp.length);
							permissions[temp.length] = Manifest.permission.ACCESS_COARSE_LOCATION;
						}
					} else {
						if (connect || location) {
							if (connect) {
								request = REQUEST_BLUETOOTH_CONNECT_PERMISSION;
								if (location) {
									permissions = new String[] { Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.BLUETOOTH_CONNECT };
								} else {
									permissions = new String[] { Manifest.permission.BLUETOOTH_CONNECT };
								}
							} else {
								request = REQUEST_BLUETOOTH_LOCATION_PERMISSION;
								permissions = new String[] { Manifest.permission.ACCESS_COARSE_LOCATION };
							}
						}
					}
					if (permissions != null) {
						return requestPermissions(activity, permissions, request);
					}
				}
				return true;

			default:
				// Check if the given permission is a dangerous permission
				try {
					if (isPermissionDangerous(activity, permissionName)) {
						return requestPermissions(activity, new String[] { permissionName }, REQUEST_SINGLE_PERMISSION_REQ_CODE);
					}
				} catch (PackageManager.NameNotFoundException e) {
					// Unknown permission - return false as it can't be granted.
					Log.w(TAG, "Unable to identify permission " + permissionName, e);
					return false;
				}
				return true;
		}
	}

	/**
	 * Request dangerous permissions which are defined in the Android manifest file from the user.
	 * @param activity the caller activity for this method.
	 * @return true/false. "true" if all permissions were granted otherwise returns "false".
	 */
	public static boolean requestManifestPermissions(Activity activity) {
		return requestManifestPermissions(activity, null);
	}

	/**
	 * Request dangerous permissions which are defined in the Android manifest file from the user.
	 * @param activity the caller activity for this method.
	 * @param excludes Set of permissions to exclude from the request
	 * @return true/false. "true" if all permissions were granted otherwise returns "false".
	 */
	public static boolean requestManifestPermissions(Activity activity, @Nullable Set<String> excludes) {
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
			return true;
		}

		String[] manifestPermissions = null;
		try {
			manifestPermissions = getManifestPermissions(activity);
		} catch (PackageManager.NameNotFoundException e) {
			e.printStackTrace();
			return false;
		}

		if (manifestPermissions == null || manifestPermissions.length == 0)
			return true;

		List<String> requestedPermissions = new ArrayList<>();
		for (String manifestPermission : manifestPermissions) {
			if (excludes != null && excludes.contains(manifestPermission)) {
				continue;
			}
			try {
				if (manifestPermission.equals(Manifest.permission.MANAGE_EXTERNAL_STORAGE)) {
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !Environment.isExternalStorageManager()) {
						Log.d(TAG, "Requesting permission " + manifestPermission);
						try {
							Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
							intent.setData(Uri.parse(String.format("package:%s", activity.getPackageName())));
							activity.startActivityForResult(intent, REQUEST_MANAGE_EXTERNAL_STORAGE_REQ_CODE);
						} catch (Exception ignored) {
							Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
							activity.startActivityForResult(intent, REQUEST_MANAGE_EXTERNAL_STORAGE_REQ_CODE);
						}
					}
				} else {
					if (isPermissionDangerous(activity, manifestPermission) && ContextCompat.checkSelfPermission(activity, manifestPermission) != PackageManager.PERMISSION_GRANTED) {
						Log.d(TAG, "Requesting permission " + manifestPermission);
						requestedPermissions.add(manifestPermission);
					}
				}
			} catch (PackageManager.NameNotFoundException e) {
				// Skip this permission and continue.
				Log.w(TAG, "Unable to identify permission " + manifestPermission, e);
			}
		}

		if (requestedPermissions.isEmpty()) {
			// If list is empty, all of dangerous permissions were granted.
			return true;
		}

		requestPermissions(activity, requestedPermissions.toArray(new String[0]), REQUEST_ALL_PERMISSION_REQ_CODE);
		return false;
	}

	/**
	 * With this function you can get the list of dangerous permissions that have been granted to the Android application.
	 * @param context the caller context for this method.
	 * @return granted permissions list
	 */
	public static String[] getGrantedPermissions(Context context) {
		String[] manifestPermissions = null;
		try {
			manifestPermissions = getManifestPermissions(context);
		} catch (PackageManager.NameNotFoundException e) {
			e.printStackTrace();
			return new String[0];
		}
		if (manifestPermissions == null || manifestPermissions.length == 0)
			return manifestPermissions;

		List<String> grantedPermissions = new ArrayList<>();
		for (String manifestPermission : manifestPermissions) {
			try {
				if (manifestPermission.equals(Manifest.permission.MANAGE_EXTERNAL_STORAGE)) {
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && Environment.isExternalStorageManager()) {
						grantedPermissions.add(manifestPermission);
					}
				} else {
					if (isPermissionDangerous(context, manifestPermission) && ContextCompat.checkSelfPermission(context, manifestPermission) == PackageManager.PERMISSION_GRANTED) {
						grantedPermissions.add(manifestPermission);
					}
				}
			} catch (PackageManager.NameNotFoundException e) {
				// Skip this permission and continue.
				Log.w(TAG, "Unable to identify permission " + manifestPermission, e);
			}
		}

		return grantedPermissions.toArray(new String[0]);
	}

	/**
	 * Check if the given permission is in the AndroidManifest.xml file.
	 * @param context the caller context for this method.
	 * @param permission the permession to look for in the manifest file.
	 * @return "true" if the permission is in the manifest file of the activity, "false" otherwise.
	 */
	public static boolean hasManifestPermission(Context context, String permission) {
		if (context == null || permission == null) {
			return false;
		}
		try {
			for (String p : getManifestPermissions(context)) {
				if (permission.equals(p))
					return true;
			}
		} catch (PackageManager.NameNotFoundException ignored) {
		}
		return false;
	}

	/**
	 * Returns the permissions defined in the AndroidManifest.xml file.
	 * @param context the caller context for this method.
	 * @return manifest permissions list
	 * @throws PackageManager.NameNotFoundException the exception is thrown when a given package, application, or component name cannot be found.
	 */
	private static String[] getManifestPermissions(Context context) throws PackageManager.NameNotFoundException {
		if (context == null) {
			return new String[0];
		}
		PackageManager packageManager = context.getPackageManager();
		PackageInfo packageInfo = packageManager.getPackageInfo(context.getPackageName(), PackageManager.GET_PERMISSIONS);
		if (packageInfo.requestedPermissions == null)
			return new String[0];
		return packageInfo.requestedPermissions;
	}

	/**
	 * Returns the information of the desired permission.
	 * @param context the caller context for this method.
	 * @param permission the name of the permission.
	 * @return permission info object
	 * @throws PackageManager.NameNotFoundException the exception is thrown when a given package, application, or component name cannot be found.
	 */
	private static PermissionInfo getPermissionInfo(Context context, String permission) throws PackageManager.NameNotFoundException {
		if (context == null || permission == null) {
			return null;
		}
		PackageManager packageManager = context.getPackageManager();
		return packageManager.getPermissionInfo(permission, 0);
	}

	private static boolean isPermissionDangerous(Context context, String permission) throws PackageManager.NameNotFoundException {
		try {
			PermissionInfo permissionInfo = getPermissionInfo(context, permission);
			int protectionLevel = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ? permissionInfo.getProtection() : permissionInfo.protectionLevel;
			return protectionLevel == PermissionInfo.PROTECTION_DANGEROUS;
		} catch (PackageManager.NameNotFoundException e) {
			if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.R && permission.toUpperCase().contains("BLUETOOTH")) {
				return false;
			}
			throw e;
		}
	}
}
