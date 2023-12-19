/**************************************************************************/
/*  bluetooth_advertiser_android.cpp                                      */
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

#include "bluetooth_advertiser_android.h"
#include "core/config/engine.h"
#include "core/core_bind.h"
#include "core/variant/typed_array.h"
#include "platform/android/bluetooth_android.h"
#include "thread_jandroid.h"

BluetoothAdvertiserAndroid::BluetoothAdvertiserAndroid() {
	id = BluetoothAndroid::get_singleton()->register_advertiser(this);
}

BluetoothAdvertiserAndroid::~BluetoothAdvertiserAndroid() {
	BluetoothAndroid::get_singleton()->unregister_advertiser(id);
}

void BluetoothAdvertiserAndroid::initialize() {
	BluetoothAdvertiser::_create = BluetoothAdvertiserAndroid::_create;
}

void BluetoothAdvertiserAndroid::deinitialize() {
	// nothing to do here
}

void BluetoothAdvertiserAndroid::respond_characteristic_read_request(String p_characteristic_uuid, String p_response, int p_request) const {
	BluetoothAndroid::respond_advertiser_characteristic_read_request(id, p_characteristic_uuid, p_response, p_request);
}

void BluetoothAdvertiserAndroid::respond_characteristic_write_request(String p_characteristic_uuid, String p_response, int p_request) const {
	BluetoothAndroid::respond_advertiser_characteristic_write_request(id, p_characteristic_uuid, p_response, p_request);
}

String BluetoothAdvertiserAndroid::get_device_name() const {
	return BluetoothAndroid::get_name();
}

String BluetoothAdvertiserAndroid::get_device_address() const {
	return BluetoothAndroid::get_address();
}

bool BluetoothAdvertiserAndroid::start_advertising() const {
	if (id == -1) {
		print_line("Registration failure");
		return false;
	}
	if (!BluetoothAndroid::is_supported(true)) {
		return false;
	}
	return BluetoothAndroid::start_advertising(id);
}

bool BluetoothAdvertiserAndroid::stop_advertising() const {
	if (id == -1) {
		print_line("Unregistration failure");
		return false;
	}
	if (!BluetoothAndroid::is_supported(true)) {
		return false;
	}
	return BluetoothAndroid::stop_advertising(id);
}

void BluetoothAdvertiserAndroid::on_register() const {
	// nothing to do here
}

void BluetoothAdvertiserAndroid::on_unregister() const {
	// nothing to do here
}

std::function<jvalret()> BluetoothAdvertiserAndroid::on_java_callback(int p_event, Variant *p_arg) {
    jvalret result;
    std::function<jvalret()> callback = [=](){return result;};

    Variant truth = Variant(true);
    Variant falsehood = Variant(false);

    JNIEnv *env = get_jni_env();
    ERR_FAIL_COND_V(env == nullptr, callback);

    switch (static_cast<BluetoothAndroid::BluetoothEvent>(p_event)) {
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_GET_NAME: {
                String name = get_service_name();
                callback = [=](){return _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&name));};
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_GET_MANUFACTURER_DATA: {
                Vector<uint8_t> data = get_service_manufacturer_information();
                if (!data.is_empty()) {
                    String manufacturer = core_bind::Marshalls::get_singleton()->raw_to_base64(data);
                    if (!manufacturer.is_empty()) {
                        callback = [=](){return _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&manufacturer), true);};
                    }
                }
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_GET_SERVICES: {
                Vector<String> services_vector;
                services_vector.push_back(get_service_uuid());
                callback = [=](){return _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, (Variant *)(&services_vector));};
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_GET_CHARACTERISTICS: {
                TypedArray<String> characteristics_array = get_characteristics();
                Vector<String> characteristics_vector;
                for (int idx = 0; idx < characteristics_array.size(); ++idx) {
                    String characteristic = characteristics_array[idx];
                    if (!characteristic.is_empty()) {
                        characteristics_vector.push_back(characteristic);
                    }
                }
                if (!characteristics_vector.is_empty()) {
                    callback = [=](){return _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, (Variant *)(&characteristics_vector));};
                } else {
                    callback = [=](){return _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, nullptr);};
                }
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_GET_CHARACTERISTIC_PERMISSIONS: {
                TypedArray<String> permissions_array = get_characteristics();
                Vector<int> permissions_vector;
                for (int idx = 0; idx < permissions_array.size(); ++idx) {
                    String permission = permissions_array[idx];
                    if (permission.is_empty()) {
                        continue;
                    }
                    if (get_characteristic_permission(permission)) {
                        permissions_vector.push_back(1);
                    } else {
                        permissions_vector.push_back(0);
                    }
                }
                if (!permissions_vector.is_empty()) {
                    callback = [=](){return _variant_to_jvalue(env, Variant::Type::PACKED_INT32_ARRAY, (Variant *)(&permissions_vector));};
                } else {
                    callback = [=](){return _variant_to_jvalue(env, Variant::Type::PACKED_INT32_ARRAY, nullptr);};
                }
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_GET_CHARACTERISTIC_VALUE: {
                Ref<BluetoothAdvertiser::BluetoothAdvertiserCharacteristic> characteristic = get_characteristic_by_uuid(p_arg->stringify().strip_edges());
                if (characteristic.is_valid()) {
                    String value = characteristic->value_base64;
                    if (!value.is_empty()) {
                        callback = [=](){return _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&value));};
                    }
                }
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_ON_ERROR: {
#ifdef DEBUG_ENABLED
                print_line("Error: "+p_arg->stringify().strip_edges());
#endif
                callback = [=]() {
                    if (on_error()) {
#ifdef DEBUG_ENABLED
                        print_line("Error");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_ON_CONNECT: {
#ifdef DEBUG_ENABLED
                print_line("Connect");
#endif
#if 0
                //on_connect(p_arg->stringify().strip_edges());
#endif
                callback = [=](){return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);};
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_ON_DISCONNECT: {
#ifdef DEBUG_ENABLED
                print_line("Disconnect");
#endif
#if 0
                //on_disconnect(p_arg->stringify().strip_edges());
#endif
                callback = [=](){return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);};
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_ON_READ: {
                String peer = "";
                String request = "";
                String characteristic = "";
                bool valid = false;
                if (p_arg->has_key("peer", valid) && valid) {
                    peer += p_arg->get("peer").stringify().strip_edges();
                }
                if (p_arg->has_key("request", valid) && valid) {
                    request += p_arg->get("request").stringify().strip_edges();
                }
                if (p_arg->has_key("characteristic", valid) && valid) {
                    characteristic += p_arg->get("characteristic").stringify().strip_edges();
                }
                if (request.is_empty()) {
                    request += "0";
                }
                callback = [=]() {
                    if (on_read(characteristic, atoi(request.utf8().get_data()), peer)) {
#ifdef DEBUG_ENABLED
                        print_line("Read");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_ON_WRITE: {
                String peer = "";
                String request = "";
                String characteristic = "";
                String value = "";
                bool valid = false;
                if (p_arg->has_key("peer", valid) && valid) {
                    peer += p_arg->get("peer").stringify().strip_edges();
                }
                if (p_arg->has_key("request", valid) && valid) {
                    request += p_arg->get("request").stringify().strip_edges();
                }
                if (p_arg->has_key("characteristic", valid) && valid) {
                    characteristic += p_arg->get("characteristic").stringify().strip_edges();
                }
                if (p_arg->has_key("value", valid) && valid) {
                    value += p_arg->get("value").stringify().strip_edges();
                }
                if (request.is_empty()) {
                    request += "0";
                }
                callback = [=]() {
                    if (on_write(characteristic, atoi(request.utf8().get_data()), peer, value)) {
#ifdef DEBUG_ENABLED
                        print_line("Write");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_ON_START_BROADCASTING: {
                callback = [=]() {
                    if (on_start()) {
#ifdef DEBUG_ENABLED
                        print_line("Start");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ADVERTISER_ON_STOP_BROADCASTING: {
                callback = [=]() {
                    if (on_stop()) {
#ifdef DEBUG_ENABLED
                        print_line("Stop");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        default:
            break;
    }

    return callback;
}
