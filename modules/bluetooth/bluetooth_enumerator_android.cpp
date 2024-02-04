/**************************************************************************/
/*  bluetooth_enumerator_android.cpp                                      */
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

#include "bluetooth_enumerator_android.h"
#include "core/config/engine.h"
#include "core/core_bind.h"
#include "core/variant/typed_array.h"
#include "platform/android/bluetooth_android.h"
#include "thread_jandroid.h"

BluetoothEnumeratorAndroid::BluetoothEnumeratorAndroid() {
	id = BluetoothAndroid::get_singleton()->register_enumerator(this);
}

BluetoothEnumeratorAndroid::~BluetoothEnumeratorAndroid() {
	BluetoothAndroid::get_singleton()->unregister_enumerator(id);
}

void BluetoothEnumeratorAndroid::initialize() {
	BluetoothEnumerator::_create = BluetoothEnumeratorAndroid::_create;
}

void BluetoothEnumeratorAndroid::deinitialize() {
	// nothing to do here
}

String BluetoothEnumeratorAndroid::get_device_name() const {
	return BluetoothAndroid::get_name();
}

String BluetoothEnumeratorAndroid::get_device_address() const {
	return BluetoothAndroid::get_address();
}

bool BluetoothEnumeratorAndroid::start_scanning() const {
	if (id == -1) {
		print_line("Registration failure");
		return false;
	}
	if (!BluetoothAndroid::is_supported(false)) {
		return false;
	}
	return BluetoothAndroid::start_scanning(id);
}

bool BluetoothEnumeratorAndroid::stop_scanning() const {
	if (id == -1) {
		print_line("Unregistration failure");
		return false;
	}
	if (!BluetoothAndroid::is_supported(false)) {
		return false;
	}
	return BluetoothAndroid::stop_scanning(id);
}

void BluetoothEnumeratorAndroid::connect_peer(String p_peer_uuid) {
	BluetoothAndroid::connect_enumerator_peer(id, p_peer_uuid);
}

void BluetoothEnumeratorAndroid::read_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) {
	BluetoothAndroid::read_enumerator_characteristic(id, p_peer_uuid, p_service_uuid, p_characteristic_uuid);
}

void BluetoothEnumeratorAndroid::write_peer_service_characteristic(String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value) {
	BluetoothAndroid::write_enumerator_characteristic(id, p_peer_uuid, p_service_uuid, p_characteristic_uuid, p_value);
}

void BluetoothEnumeratorAndroid::on_register() const {
	// nothing to do here
}

void BluetoothEnumeratorAndroid::on_unregister() const {
	// nothing to do here
}

std::function<jvalret()> BluetoothEnumeratorAndroid::on_java_callback(int p_event, Variant *p_arg) {
    jvalret result;
    std::function<jvalret()> callback = [=](){return result;};

    Variant truth = Variant(true);
    Variant falsehood = Variant(false);

    JNIEnv *env = get_jni_env();
    ERR_FAIL_COND_V(env == nullptr, callback);

    switch (static_cast<BluetoothAndroid::BluetoothEvent>(p_event)) {
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_GET_SOUGHT_SERVICES: {
                TypedArray<String> sought_services_array = get_sought_services();
                Vector<String> sought_services_vector;
                for (int idx = 0; idx < sought_services_array.size(); ++idx) {
                    String sought_service = sought_services_array[idx];
                    if (!sought_service.is_empty()) {
                        sought_services_vector.push_back(sought_service);
                    }
                }
                if (!sought_services_vector.is_empty()) {
                    callback = [=](){return _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, (Variant *)(&sought_services_vector));};
                } else {
                    callback = [=](){return _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, nullptr);};
                }
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_STOP_SCANNING: {
                callback = [=]() {
                    if (on_stop()) {
#ifdef DEBUG_ENABLED
                        print_line("Unscan");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_START_SCANNING: {
                callback = [=]() {
                    if (on_start()) {
#ifdef DEBUG_ENABLED
                        print_line("Scan");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_DISCOVER: {
                String address = "";
                String name = "";
                String data = "";
                bool valid = false;
                if (p_arg->has_key("address", valid) && valid) {
                    address += p_arg->get("address").stringify().strip_edges();
                }
                if (p_arg->has_key("name", valid) && valid) {
                    name += p_arg->get("name").stringify().strip_edges();
                }
                if (p_arg->has_key("data", valid) && valid) {
                    data += p_arg->get("data").stringify().strip_edges();
                }
                if (!data.is_empty()) {
                    data = core_bind::Marshalls::get_singleton()->utf8_to_base64(data);
                }
#ifdef DEBUG_ENABLED
                print_line("Discovery address = " + address);
                print_line("Discovery name = " + name);
                print_line("Discovery data = " + data);
#endif
                callback = [=]() {
                    if (on_discover(address, name, data)) {
#ifdef DEBUG_ENABLED
                        print_line("Discover");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_CONNECT: {
                callback = [=]() {
                    if (on_connect(p_arg->stringify().strip_edges())) {
#ifdef DEBUG_ENABLED
                        print_line("Connect");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_DISCONNECT: {
                callback = [=]() {
                    if (on_disconnect(p_arg->stringify().strip_edges())) {
#ifdef DEBUG_ENABLED
                        print_line("Disconnect");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_DISCOVER_SERVICE_CHARACTERISTIC: {
                Vector<String> args;
                Variant iter;
                bool valid = false;
                String peer = "";
                String service = "";
                String characteristic = "";
                String permission = "";
                if (p_arg->iter_init(iter, valid) && valid) {
                    do {
                        args.push_back(p_arg->iter_get(iter, valid).stringify().strip_edges());
                        if (!valid) {
                            args.remove_at(args.size() - 1);
                            break;
                        }
                    } while (p_arg->iter_next(iter, valid) && valid);
                }
                if (args.size() >= 4) {
                    peer += args.get(0).strip_edges();
                    service += args.get(1).strip_edges();
                    characteristic += args.get(2).strip_edges();
                    permission += args.get(3).strip_edges();
                }
                callback = [=](){
                if (on_discover_service_characteristic(peer, service, characteristic, permission == "true")) {
#ifdef DEBUG_ENABLED
                        print_line("Characteristic");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_READ: {
                String peer = "";
                String service = "";
                String characteristic = "";
                String value = "";
                bool valid = false;
                if (p_arg->has_key("peer", valid) && valid) {
                    peer += p_arg->get("peer").stringify().strip_edges();
                }
                if (p_arg->has_key("service", valid) && valid) {
                    service += p_arg->get("service").stringify().strip_edges();
                }
                if (p_arg->has_key("characteristic", valid) && valid) {
                    characteristic += p_arg->get("characteristic").stringify().strip_edges();
                }
                if (p_arg->has_key("value", valid) && valid) {
                    value += p_arg->get("value").stringify().strip_edges();
                }
                callback = [=]() {
                    if (on_read(peer, service, characteristic, value)) {
#ifdef DEBUG_ENABLED
                        print_line("Read");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_WRITE: {
                String peer = "";
                String service = "";
                String characteristic = "";
                bool valid = false;
                if (p_arg->has_key("peer", valid) && valid) {
                    peer += p_arg->get("peer").stringify().strip_edges();
                }
                if (p_arg->has_key("service", valid) && valid) {
                    service += p_arg->get("service").stringify().strip_edges();
                }
                if (p_arg->has_key("characteristic", valid) && valid) {
                    characteristic += p_arg->get("characteristic").stringify().strip_edges();
                }
                callback = [=](){
                    if (on_write(peer, service, characteristic)) {
#ifdef DEBUG_ENABLED
                        print_line("Write");
#endif
                        return _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
                    }
                    return _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
                };
            }
            break;
        case BluetoothAndroid::BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_ERROR: {
                String peer = "";
                String service = "";
                String characteristic = "";
                bool valid = false;
                if (p_arg->has_key("peer", valid) && valid) {
                    peer += p_arg->get("peer").stringify().strip_edges();
                }
                if (p_arg->has_key("service", valid) && valid) {
                    service += p_arg->get("service").stringify().strip_edges();
                }
                if (p_arg->has_key("characteristic", valid) && valid) {
                    characteristic += p_arg->get("characteristic").stringify().strip_edges();
                }
                callback = [=]() {
                    if (on_error(peer, service, characteristic)) {
#ifdef DEBUG_ENABLED
                        print_line("Error");
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
