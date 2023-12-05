/**************************************************************************/
/*  bluetooth_android.cpp                                                 */
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

#include "bluetooth_android.h"

#include "core/core_bind.h"
#include "core/variant/typed_array.h"
#include "java_godot_wrapper.h"
#include "jni_utils.h"
#include "os_android.h"
#include "string_android.h"
#include "thread_jandroid.h"

jobject BluetoothAndroid::bluetooth = nullptr;
jclass BluetoothAndroid::cls = nullptr;

jmethodID BluetoothAndroid::_is_supported = nullptr;

jmethodID BluetoothAndroid::_register_advertiser = nullptr;
jmethodID BluetoothAndroid::_register_enumerator = nullptr;
jmethodID BluetoothAndroid::_unregister_advertiser = nullptr;
jmethodID BluetoothAndroid::_unregister_enumerator = nullptr;

jmethodID BluetoothAndroid::_start_advertising = nullptr;
jmethodID BluetoothAndroid::_stop_advertising = nullptr;
jmethodID BluetoothAndroid::_start_scanning = nullptr;
jmethodID BluetoothAndroid::_stop_scanning = nullptr;

jmethodID BluetoothAndroid::_get_name = nullptr;
jmethodID BluetoothAndroid::_get_address = nullptr;
jmethodID BluetoothAndroid::_connect_enumerator_peer = nullptr;
jmethodID BluetoothAndroid::_read_enumerator_characteristic = nullptr;
jmethodID BluetoothAndroid::_write_enumerator_characteristic = nullptr;
jmethodID BluetoothAndroid::_respond_advertiser_characteristic_read_request = nullptr;
jmethodID BluetoothAndroid::_respond_advertiser_characteristic_write_request = nullptr;

HashMap<int, BluetoothAdvertiserAndroid *> BluetoothAndroid::advertisers;
HashMap<int, BluetoothEnumeratorAndroid *> BluetoothAndroid::enumerators;
int BluetoothAndroid::advertiser_id = 0;
int BluetoothAndroid::enumerator_id = 0;

BluetoothAndroid *BluetoothAndroid::singleton = nullptr;

BluetoothAndroid *BluetoothAndroid::get_singleton() {
	return singleton;
}

jobject BluetoothAndroid::_java_bluetooth_callback(int p_event, int p_id, Variant *p_arg) {
	jvalret result;

	Variant truth = Variant(true);
	Variant falsehood = Variant(false);

#ifdef MODULE_BLUETOOTH_ENABLED
	JNIEnv *env = get_jni_env();

#ifdef DEBUG_ENABLED
	print_line(String((std::string("Bluetooth event ") + std::to_string(p_event) + " @ " + std::to_string(p_id)).c_str()) + " = " + p_arg->stringify());
#endif

	ERR_FAIL_COND_V(env == nullptr, result.obj);

	switch ((BluetoothEvent)p_event) {
		case BLUETOOTH_ENUMERATOR_GET_SOUGHT_SERVICES:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
					TypedArray<String> sought_services_array = enumerator->get_sought_services();
					Vector<String> sought_services_vector;
					for (int idx = 0; idx < sought_services_array.size(); ++idx) {
						String sought_service = sought_services_array[idx];
						if (!sought_service.is_empty()) {
							sought_services_vector.push_back(sought_service);
						}
					}
					if (!sought_services_vector.is_empty()) {
						result = _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, (Variant *)(&sought_services_vector));
					} else {
						result = _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, nullptr);
					}
				}
			}
			break;
		case BLUETOOTH_ENUMERATOR_ON_STOP_SCANNING:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
					if (enumerator->on_stop()) {
#ifdef DEBUG_ENABLED
						print_line("Unscan");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ENUMERATOR_ON_START_SCANNING:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
					if (enumerator->on_start()) {
#ifdef DEBUG_ENABLED
						print_line("Scan");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ENUMERATOR_ON_DISCOVER:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
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
					if (enumerator->on_discover(address, name, data)) {
#ifdef DEBUG_ENABLED
						print_line("Discover");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ENUMERATOR_ON_CONNECT:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
					if (enumerator->on_connect(p_arg->stringify().strip_edges())) {
#ifdef DEBUG_ENABLED
						print_line("Connect");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ENUMERATOR_ON_DISCONNECT:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
					if (enumerator->on_disconnect(p_arg->stringify().strip_edges())) {
#ifdef DEBUG_ENABLED
						print_line("Disconnect");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ENUMERATOR_ON_DISCOVER_SERVICE_CHARACTERISTIC:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
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
						} while (!p_arg->iter_next(iter, valid) && valid);
					}
					if (args.size() >= 4) {
						peer += args.get(0).strip_edges();
						service += args.get(1).strip_edges();
						characteristic += args.get(2).strip_edges();
						permission += args.get(3).strip_edges();
					}
					if (enumerator->on_discover_service_characteristic(peer, service, characteristic, permission == "true")) {
#ifdef DEBUG_ENABLED
						print_line("Characteristic");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ENUMERATOR_ON_READ:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
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
					if (enumerator->on_read(peer, service, characteristic, value)) {
#ifdef DEBUG_ENABLED
						print_line("Read");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ENUMERATOR_ON_WRITE:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
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
					if (enumerator->on_write(peer, service, characteristic)) {
#ifdef DEBUG_ENABLED
						print_line("Write");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ENUMERATOR_ON_ERROR:
			if (enumerators.has(p_id)) {
				BluetoothEnumeratorAndroid *enumerator = enumerators.get(p_id);
				if (enumerator != nullptr) {
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
					if (enumerator->on_error(peer, service, characteristic)) {
#ifdef DEBUG_ENABLED
						print_line("Error");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ADVERTISER_GET_NAME:
			if (advertisers.has(p_id)) {
				BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
				if (advertiser != nullptr) {
					String name = advertiser->get_service_name();
					result = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&name));
				}
			}
			break;
		case BLUETOOTH_ADVERTISER_GET_MANUFACTURER_DATA:
			if (advertisers.has(p_id)) {
				BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
				if (advertiser != nullptr) {
					Vector<uint8_t> data = advertiser->get_service_manufacturer_information();
					if (!data.is_empty()) {
						String manufacturer = core_bind::Marshalls::get_singleton()->raw_to_base64(data);
						if (!manufacturer.is_empty()) {
							result = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&manufacturer), true);
						}
					}
				}
			}
			break;
		case BLUETOOTH_ADVERTISER_GET_SERVICES:
			if (advertisers.has(p_id)) {
				BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
				if (advertiser != nullptr) {
					Vector<String> services_vector;
					services_vector.push_back(advertiser->get_service_uuid());
					result = _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, (Variant *)(&services_vector));
				}
			}
			break;
		case BLUETOOTH_ADVERTISER_GET_CHARACTERISTICS:
			if (advertisers.has(p_id)) {
				BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
				if (advertiser != nullptr) {
					TypedArray<String> characteristics_array = advertiser->get_characteristics();
					Vector<String> characteristics_vector;
					for (int idx = 0; idx < characteristics_array.size(); ++idx) {
						String characteristic = characteristics_array[idx];
						if (!characteristic.is_empty()) {
							characteristics_vector.push_back(characteristic);
						}
					}
					if (!characteristics_vector.is_empty()) {
						result = _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, (Variant *)(&characteristics_vector));
					} else {
						result = _variant_to_jvalue(env, Variant::Type::PACKED_STRING_ARRAY, nullptr);
					}
				}
			}
			break;
		case BLUETOOTH_ADVERTISER_GET_CHARACTERISTC_PERMISSIONS:
			if (advertisers.has(p_id)) {
				BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
				if (advertiser != nullptr) {
					TypedArray<String> permissions_array = advertiser->get_characteristics();
					Vector<int> permissions_vector;
					for (int idx = 0; idx < permissions_array.size(); ++idx) {
						String permission = permissions_array[idx];
						if (permission.is_empty()) {
							continue;
						}
						if (advertiser->get_characteristic_permission(permission)) {
							permissions_vector.push_back(1);
						} else {
							permissions_vector.push_back(0);
						}
					}
					if (!permissions_vector.is_empty()) {
						result = _variant_to_jvalue(env, Variant::Type::PACKED_INT32_ARRAY, (Variant *)(&permissions_vector));
					} else {
						result = _variant_to_jvalue(env, Variant::Type::PACKED_INT32_ARRAY, nullptr);
					}
				}
			}
			break;
		case BLUETOOTH_ADVERTISER_GET_CHARACTERISTC_VALUE:
			if (advertisers.has(p_id)) {
				BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
				if (advertiser != nullptr) {
					Ref<BluetoothAdvertiser::BluetoothAdvertiserCharacteristic> characteristic = advertiser->get_characteristic_by_uuid(p_arg->stringify().strip_edges());
					if (characteristic.is_valid()) {
						String value = characteristic->value;
						if (!value.is_empty()) {
							result = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&value));
						}
					}
				}
			}
			break;
		case BLUETOOTH_ADVERTISER_ON_ERROR:
#ifdef DEBUG_ENABLED
			print_line("Error");
#endif
#if 0
            if (advertisers.has(p_id)) {
                BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
                if (advertiser != nullptr) {
                    //advertiser->on_error();
                }
            }
#endif
			break;
		case BLUETOOTH_ADVERTISER_ON_CONNECT:
#ifdef DEBUG_ENABLED
			print_line("Connect");
#endif
#if 0
            if (advertisers.has(p_id)) {
                BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
                if (advertiser != nullptr) {
                    //advertiser->on_connect(p_arg->stringify().strip_edges());
                }
            }
#endif
			break;
		case BLUETOOTH_ADVERTISER_ON_DISCONNECT:
#ifdef DEBUG_ENABLED
			print_line("Disconnect");
#endif
#if 0
            if (advertisers.has(p_id)) {
                BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
                if (advertiser != nullptr) {
                    //advertiser->on_disconnect(p_arg->stringify().strip_edges());
                }
            }
#endif
			break;
		case BLUETOOTH_ADVERTISER_ON_READ:
			if (advertisers.has(p_id)) {
				BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
				if (advertiser != nullptr) {
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
					if (advertiser->on_read(characteristic, atoi(request.utf8().get_data()), peer)) {
#ifdef DEBUG_ENABLED
						print_line("Write");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		case BLUETOOTH_ADVERTISER_ON_WRITE:
			if (advertisers.has(p_id)) {
				BluetoothAdvertiserAndroid *advertiser = advertisers.get(p_id);
				if (advertiser != nullptr) {
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
					if (advertiser->on_write(characteristic, atoi(request.utf8().get_data()), peer, value)) {
#ifdef DEBUG_ENABLED
						print_line("Read");
#endif
						result = _variant_to_jvalue(env, Variant::Type::BOOL, &truth, true);
					}
				}
			}
			if (!result.obj) {
				result = _variant_to_jvalue(env, Variant::Type::BOOL, &falsehood, true);
			}
			break;
		default:
			break;
	}
#endif

	return result.obj;
}

void BluetoothAndroid::setup(jobject p_bluetooth) {
	JNIEnv *env = get_jni_env();

	ERR_FAIL_COND_MSG(singleton != nullptr, "BluetoothAndroid singleton already done with setup");

	singleton = memnew(BluetoothAndroid);

	bluetooth = env->NewGlobalRef(p_bluetooth);

	jclass c = env->GetObjectClass(bluetooth);
	cls = (jclass)env->NewGlobalRef(c);

	_is_supported = env->GetMethodID(cls, "isSupported", "(Z)Z");

	_start_advertising = env->GetMethodID(cls, "startAdvertising", "(I)Z");
	_stop_advertising = env->GetMethodID(cls, "stopAdvertising", "(I)Z");
	_start_scanning = env->GetMethodID(cls, "startScanning", "(I)Z");
	_stop_scanning = env->GetMethodID(cls, "stopScanning", "(I)Z");

	_connect_enumerator_peer = env->GetMethodID(cls, "connectEnumeratorPeer", "(ILjava/lang/String;)Z");
	_read_enumerator_characteristic = env->GetMethodID(cls, "readEnumeratorCharacteristic", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
	_write_enumerator_characteristic = env->GetMethodID(cls, "writeEnumeratorCharacteristic", "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
	_respond_advertiser_characteristic_read_request = env->GetMethodID(cls, "respondAdvertiserCharacteristicReadRequest", "(ILjava/lang/String;Ljava/lang/String;I)Z");
	_respond_advertiser_characteristic_write_request = env->GetMethodID(cls, "respondAdvertiserCharacteristicWriteRequest", "(ILjava/lang/String;Ljava/lang/String;I)Z");

	_register_advertiser = env->GetMethodID(cls, "registerAdvertiser", "(I)Z");
	_register_enumerator = env->GetMethodID(cls, "registerEnumerator", "(I)Z");
	_unregister_advertiser = env->GetMethodID(cls, "unregisterAdvertiser", "(I)Z");
	_unregister_enumerator = env->GetMethodID(cls, "unregisterEnumerator", "(I)Z");
}

bool BluetoothAndroid::is_supported(bool p_role) {
	if (_is_supported) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		return env->CallBooleanMethod(bluetooth, _is_supported, p_role);
	} else {
		return false;
	}
}

bool BluetoothAndroid::start_advertising(int p_advertiser_id) {
	if (_start_advertising) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		return env->CallBooleanMethod(bluetooth, _start_advertising, p_advertiser_id);
	} else {
		return false;
	}
}

bool BluetoothAndroid::stop_advertising(int p_advertiser_id) {
	if (_stop_advertising) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		return env->CallBooleanMethod(bluetooth, _stop_advertising, p_advertiser_id);
	} else {
		return false;
	}
}

bool BluetoothAndroid::start_scanning(int p_enumerator_id) {
	if (_start_scanning) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		return env->CallBooleanMethod(bluetooth, _start_scanning, p_enumerator_id);
	} else {
		return false;
	}
}

bool BluetoothAndroid::stop_scanning(int p_enumerator_id) {
	if (_stop_scanning) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		return env->CallBooleanMethod(bluetooth, _stop_scanning, p_enumerator_id);
	} else {
		return false;
	}
}

String BluetoothAndroid::get_name() {
	String str = "";
	if (_get_name) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, str);
		jstring res = (jstring)env->CallObjectMethod(bluetooth, _get_name);
		if (res) {
			str += _jobject_to_variant(env, res).stringify();
			env->DeleteLocalRef(res);
		}
	}
	return str;
}

String BluetoothAndroid::get_address() {
	String str = "";
	if (_get_address) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, str);
		jstring res = (jstring)env->CallObjectMethod(bluetooth, _get_address);
		if (res) {
			str += _jobject_to_variant(env, res).stringify();
			env->DeleteLocalRef(res);
		}
	}
	return str;
}

bool BluetoothAndroid::connect_enumerator_peer(int p_enumerator_id, String p_peer_uuid) {
	if (_connect_enumerator_peer) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		jvalret peer = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_peer_uuid));
		jboolean res = JNI_FALSE;
		if (peer.obj) {
			res = env->CallBooleanMethod(bluetooth, _connect_enumerator_peer, p_enumerator_id, peer.obj);
			env->DeleteLocalRef(peer.obj);
		}
		return res;
	} else {
		return false;
	}
}

bool BluetoothAndroid::read_enumerator_characteristic(int p_enumerator_id, String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid) {
	if (_read_enumerator_characteristic) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		jvalret peer = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_peer_uuid));
		jvalret service = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_service_uuid));
		jvalret characteristic = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_characteristic_uuid));
		jboolean res = JNI_FALSE;
		if (!peer.obj || !service.obj || !characteristic.obj) {
			if (peer.obj) {
				env->DeleteLocalRef(peer.obj);
			}
			if (service.obj) {
				env->DeleteLocalRef(service.obj);
			}
			if (characteristic.obj) {
				env->DeleteLocalRef(characteristic.obj);
			}
			return res;
		}
		res = env->CallBooleanMethod(bluetooth, _read_enumerator_characteristic, p_enumerator_id, peer.obj, service.obj, characteristic.obj);
		env->DeleteLocalRef(peer.obj);
		env->DeleteLocalRef(service.obj);
		env->DeleteLocalRef(characteristic.obj);
		return res;
	} else {
		return false;
	}
}

bool BluetoothAndroid::write_enumerator_characteristic(int p_enumerator_id, String p_peer_uuid, String p_service_uuid, String p_characteristic_uuid, String p_value) {
	if (_write_enumerator_characteristic) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		jvalret peer = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_peer_uuid));
		jvalret service = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_service_uuid));
		jvalret characteristic = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_characteristic_uuid));
		jvalret value = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_value));
		jboolean res = JNI_FALSE;
		if (!peer.obj || !service.obj || !characteristic.obj || !value.obj) {
			if (peer.obj) {
				env->DeleteLocalRef(peer.obj);
			}
			if (service.obj) {
				env->DeleteLocalRef(service.obj);
			}
			if (characteristic.obj) {
				env->DeleteLocalRef(characteristic.obj);
			}
			if (value.obj) {
				env->DeleteLocalRef(value.obj);
			}
			return res;
		}
		res = env->CallBooleanMethod(bluetooth, _write_enumerator_characteristic, p_enumerator_id, peer.obj, service.obj, characteristic.obj, value.obj);
		env->DeleteLocalRef(peer.obj);
		env->DeleteLocalRef(service.obj);
		env->DeleteLocalRef(characteristic.obj);
		env->DeleteLocalRef(value.obj);
		return res;
	} else {
		return false;
	}
}

bool BluetoothAndroid::respond_advertiser_characteristic_read_request(int p_advertiser_id, String p_characteristic_uuid, String p_response, int p_request) {
	if (_respond_advertiser_characteristic_read_request) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		jvalret characteristic = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_characteristic_uuid));
		jvalret response = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_response));
		jboolean res = JNI_FALSE;
		if (!characteristic.obj || !response.obj) {
			if (characteristic.obj) {
				env->DeleteLocalRef(characteristic.obj);
			}
			if (response.obj) {
				env->DeleteLocalRef(response.obj);
			}
			return res;
		}
		res = env->CallBooleanMethod(bluetooth, _respond_advertiser_characteristic_read_request, p_advertiser_id, characteristic.obj, response.obj, p_request);
		env->DeleteLocalRef(characteristic.obj);
		env->DeleteLocalRef(response.obj);
		return res;
	} else {
		return false;
	}
}

bool BluetoothAndroid::respond_advertiser_characteristic_write_request(int p_advertiser_id, String p_characteristic_uuid, String p_response, int p_request) {
	if (_respond_advertiser_characteristic_write_request) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		jvalret characteristic = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_characteristic_uuid));
		jvalret response = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_response));
		jboolean res = JNI_FALSE;
		if (!characteristic.obj || !response.obj) {
			if (characteristic.obj) {
				env->DeleteLocalRef(characteristic.obj);
			}
			if (response.obj) {
				env->DeleteLocalRef(response.obj);
			}
			return res;
		}
		res = env->CallBooleanMethod(bluetooth, _respond_advertiser_characteristic_write_request, p_advertiser_id, characteristic.obj, response.obj, p_request);
		env->DeleteLocalRef(characteristic.obj);
		env->DeleteLocalRef(response.obj);
		return res;
	} else {
		return false;
	}
}

int BluetoothAndroid::register_advertiser(BluetoothAdvertiserAndroid *p_advertiser) {
	_THREAD_SAFE_METHOD_

	if (_register_advertiser) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, -1);
		int new_id = advertiser_id + 1;
		if (advertisers.has(new_id)) {
			return -1;
		}

		if (env->CallBooleanMethod(bluetooth, _register_advertiser, new_id)) {
			advertiser_id = new_id;
			advertisers.insert(new_id, p_advertiser);
			return new_id;
		}
	}

	return -1;
}

int BluetoothAndroid::register_enumerator(BluetoothEnumeratorAndroid *p_enumerator) {
	_THREAD_SAFE_METHOD_

	if (_register_enumerator) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, -1);
		int new_id = enumerator_id + 1;
		if (enumerators.has(new_id)) {
			return -1;
		}

		if (env->CallBooleanMethod(bluetooth, _register_enumerator, new_id)) {
			enumerator_id = new_id;
			enumerators.insert(new_id, p_enumerator);
			return new_id;
		}
	}

	return -1;
}

bool BluetoothAndroid::unregister_advertiser(int p_advertiser_id) {
	_THREAD_SAFE_METHOD_

	if (_unregister_advertiser) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, -1);
		if (advertisers.has(p_advertiser_id)) {
			advertisers.erase(p_advertiser_id);

			return env->CallBooleanMethod(bluetooth, _unregister_advertiser, p_advertiser_id);
		}
	}

	return false;
}

bool BluetoothAndroid::unregister_enumerator(int p_enumerator_id) {
	_THREAD_SAFE_METHOD_

	if (_unregister_enumerator) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, -1);
		if (enumerators.has(p_enumerator_id)) {
			enumerators.erase(p_enumerator_id);

			return env->CallBooleanMethod(bluetooth, _unregister_enumerator, p_enumerator_id);
		}
	}

	return false;
}
