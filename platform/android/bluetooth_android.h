/**************************************************************************/
/*  bluetooth_android.h                                                   */
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

#ifndef BLUETOOTH_ANDROID_H
#define BLUETOOTH_ANDROID_H

#include "core/os/thread_safe.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "modules/modules_enabled.gen.h" // For bluetooth.

#ifdef MODULE_BLUETOOTH_ENABLED
#include "modules/bluetooth/bluetooth_advertiser_android.h"
#include "modules/bluetooth/bluetooth_enumerator_android.h"
#else
typedef void* BluetoothAdvertiserAndroid;
typedef void* BluetoothEnumeratorAndroid;
#endif

#include <jni.h>

class BluetoothAndroid {
    _THREAD_SAFE_CLASS_

    enum BluetoothEvent {
        BLUETOOTH_ENUMERATOR_GET_SOUGHT_SERVICES = 0,
    };

    static BluetoothAndroid *singleton;

	static jobject bluetooth;
	static jclass cls;

	static jmethodID _is_supported;

    static jmethodID _start_advertising;
    static jmethodID _stop_advertising;
    static jmethodID _start_scanning;
    static jmethodID _stop_scanning;

    static jmethodID _register_advertiser;
    static jmethodID _register_enumerator;
    static jmethodID _unregister_advertiser;
    static jmethodID _unregister_enumerator;

    static HashMap<int, BluetoothAdvertiserAndroid*> advertisers;
    static HashMap<int, BluetoothEnumeratorAndroid*> enumerators;
    static int advertiser_id;
    static int enumerator_id;

public:
    static BluetoothAndroid *get_singleton();

    static jobject _java_bluetooth_callback(int p_event, int p_id);

    static void setup(jobject p_bluetooth);

	static bool is_supported();

    static bool start_advertising(int p_advertiser_id);
    static bool stop_advertising(int p_advertiser_id);
    static bool start_scanning(int p_enumerator_id);
    static bool stop_scanning(int p_enumerator_id);

    int register_advertiser(BluetoothAdvertiserAndroid* p_advertiser);
    int register_enumerator(BluetoothEnumeratorAndroid* p_enumerator);
    bool unregister_advertiser(int p_advertiser_id);
    bool unregister_enumerator(int p_enumerator_id);
};

#endif // BLUETOOTH_ANDROID_H
