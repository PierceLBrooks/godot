/**************************************************************************/
/*  GodotEditorApplication.java                                           */
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

package org.godotengine.godot;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Application;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.util.Log;

import androidx.annotation.NonNull;

import java.lang.reflect.Method;

/**
 * Base application for the Godot editor app.
 */
public class GodotEditorApplication extends Application implements Application.ActivityLifecycleCallbacks {
	private static final String TAG = GodotEditorApplication.class.getSimpleName();

	@Override
	public void onCreate() {
		super.onCreate();
		Log.v(TAG, "onCreate: "+getProcess());
		registerActivityLifecycleCallbacks(this);
		//Debug.waitForDebugger();
	}

	@Override
	public void onTerminate() {
		Log.v(TAG, "onTerminate: "+getProcess());
		unregisterActivityLifecycleCallbacks(this);
		super.onTerminate();
	}

	@Override
	public void onActivityCreated(Activity activity, Bundle bundle) {
		Log.v(TAG, "onActivityCreated: "+activity.getLocalClassName());
	}

	@Override
	public void onActivityDestroyed(Activity activity) {
		Log.v(TAG, "onActivityDestroyed: "+activity.getLocalClassName());
	}

	@Override
	public void onActivityStarted(Activity activity) {
		Log.v(TAG, "onActivityStarted: "+activity.getLocalClassName());
	}

	@Override
	public void onActivityStopped(Activity activity) {
		Log.v(TAG, "onActivityStopped: "+activity.getLocalClassName());
	}

	@Override
	public void onActivityResumed(Activity activity) {
		Log.v(TAG, "onActivityResumed: "+activity.getLocalClassName());
	}

	@Override
	public void onActivityPaused(Activity activity) {
		Log.v(TAG, "onActivityPaused: "+activity.getLocalClassName());
	}

	@Override
	public void onActivitySaveInstanceState(Activity activity, @NonNull Bundle bundle) {
		Log.v(TAG, "onActivitySaveInstanceState: "+activity.getLocalClassName());
	}

	public static String getProcess() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
			return Application.getProcessName();

		// Using the same technique as Application.getProcessName() for older devices
		// Using reflection since ActivityThread is an internal API

		try {
			@SuppressLint("PrivateApi")
			Class<?> activityThread = Class.forName("android.app.ActivityThread");

			// Before API 18, the method was incorrectly named "currentPackageName", but it still returned the process name
			// See https://github.com/aosp-mirror/platform_frameworks_base/commit/b57a50bd16ce25db441da5c1b63d48721bb90687
			String methodName = Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2 ? "currentProcessName" : "currentPackageName";

			Method getProcessName = activityThread.getDeclaredMethod(methodName);
			return (String) getProcessName.invoke(null);
		} catch (Exception exception) {
			exception.printStackTrace();
		}

		return null;
	}
}

