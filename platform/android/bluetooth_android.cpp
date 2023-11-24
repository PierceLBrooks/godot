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

jmethodID BluetoothAndroid::_connect_enumerator_peer = nullptr;

HashMap<int, BluetoothAdvertiserAndroid *> BluetoothAndroid::advertisers;
HashMap<int, BluetoothEnumeratorAndroid *> BluetoothAndroid::enumerators;
int BluetoothAndroid::advertiser_id = 0;
int BluetoothAndroid::enumerator_id = 0;

BluetoothAndroid *BluetoothAndroid::singleton = nullptr;

BluetoothAndroid *BluetoothAndroid::get_singleton() {
	return singleton;
}

jobject BluetoothAndroid::_java_bluetooth_callback(int p_event, int p_id, Variant p_arg) {
	jvalret result;

	Variant truth = Variant(true);
	Variant falsehood = Variant(false);

#ifdef MODULE_BLUETOOTH_ENABLED
	JNIEnv *env = get_jni_env();

#ifdef DEBUG_ENABLED
	print_line(String((std::string("Bluetooth event ") + std::to_string(p_event) + " @ " + std::to_string(p_id)).c_str()) + " = " + p_arg.stringify());
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
						if (sought_service.size() > 0) {
							sought_services_vector.push_back(sought_service);
						}
					}
					if (sought_services_vector.size() > 0) {
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
					if (p_arg.has_key("address", valid) && valid) {
						address += p_arg.get("address").stringify().strip_edges();
					}
					if (p_arg.has_key("name", valid) && valid) {
						name += p_arg.get("name").stringify().strip_edges();
					}
					if (p_arg.has_key("data", valid) && valid) {
						data += p_arg.get("data").stringify().strip_edges();
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
					if (enumerator->on_connect(p_arg.stringify())) {
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
					if (enumerator->on_connect(p_arg.stringify())) {
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
					if (p_arg.iter_init(iter, valid) && valid) {
						do {
							args.push_back(p_arg.iter_get(iter, valid).stringify().strip_edges());
							if (!valid) {
								args.remove_at(args.size() - 1);
								break;
							}
						} while (!p_arg.iter_next(iter, valid) && valid);
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

	_is_supported = env->GetMethodID(cls, "isSupported", "()Z");

	_start_advertising = env->GetMethodID(cls, "startAdvertising", "(I)Z");
	_stop_advertising = env->GetMethodID(cls, "stopAdvertising", "(I)Z");
	_start_scanning = env->GetMethodID(cls, "startScanning", "(I)Z");
	_stop_scanning = env->GetMethodID(cls, "stopScanning", "(I)Z");

	_connect_enumerator_peer = env->GetMethodID(cls, "connectEnumeratorPeer", "(ILjava/lang/String;)Z");

	_register_advertiser = env->GetMethodID(cls, "registerAdvertiser", "(I)Z");
	_register_enumerator = env->GetMethodID(cls, "registerEnumerator", "(I)Z");
	_unregister_advertiser = env->GetMethodID(cls, "unregisterAdvertiser", "(I)Z");
	_unregister_enumerator = env->GetMethodID(cls, "unregisterEnumerator", "(I)Z");
}

bool BluetoothAndroid::is_supported() {
	if (_is_supported) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		return env->CallBooleanMethod(bluetooth, _is_supported);
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

bool BluetoothAndroid::connect_enumerator_peer(int p_enumerator_id, String p_peer_uuid) {
	if (_connect_enumerator_peer) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_COND_V(env == nullptr, false);
		jvalret str = _variant_to_jvalue(env, Variant::Type::STRING, (Variant *)(&p_peer_uuid));
		jboolean res = JNI_FALSE;
		if (str.obj) {
			res = env->CallBooleanMethod(bluetooth, _connect_enumerator_peer, p_enumerator_id, str.obj);
			env->DeleteLocalRef(str.obj);
		}
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
