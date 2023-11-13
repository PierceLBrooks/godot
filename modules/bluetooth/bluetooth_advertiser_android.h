/**************************************************************************/
/*  bluetooth_advertiser_android.h                                        */
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

#ifndef BLUETOOTH_ADVERTISER_ANDROID_H
#define BLUETOOTH_ADVERTISER_ANDROID_H

#include "bluetooth_advertiser.h"

//////////////////////////////////////////////////////////////////////////
// BluetoothAdvertiserAndroid - Subclass for bluetooth advertisers in Android

class BluetoothAdvertiserAndroid : public BluetoothAdvertiser {
private:
	static BluetoothAdvertiser *_create() { return memnew(BluetoothAdvertiserAndroid); }
public:
	BluetoothAdvertiserAndroid();
	virtual ~BluetoothAdvertiserAndroid();

	static void initialize();
	static void deinitialize();

    void respond_characteristic_read_request(String p_characteristic_uuid, String p_response, int p_request) const override;
    void respond_characteristic_write_request(String p_characteristic_uuid, String p_response, int p_request) const override;

	bool start_advertising() const override;
	bool stop_advertising() const override;

    void on_register() const override;
    void on_unregister() const override;
};

#endif // BLUETOOTH_ADVERTISER_ANDROID_H
