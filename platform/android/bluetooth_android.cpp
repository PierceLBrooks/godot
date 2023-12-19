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
    std::function<jvalret()> callback = [=](){return result;};

#ifdef MODULE_BLUETOOTH_ENABLED
    if (OS::get_singleton()->has_feature("debug")) {
	    print_line(String((std::string("Bluetooth event ") + std::to_string(p_event) + " @ " + std::to_string(p_id)).c_str()) + " = " + Variant::get_type_name(p_arg->get_type()) + " " +p_arg->stringify());
    }
    if (p_event <= static_cast<int>(BluetoothEvent::BLUETOOTH_ENUMERATOR_ON_ERROR)) {
        BluetoothEnumeratorAndroid *enumerator = get_enumerator(p_id);
        if (enumerator != nullptr) {
            callback = enumerator->on_java_callback(p_event, p_arg);
        }
    } else {
        BluetoothAdvertiserAndroid *advertiser = get_advertiser(p_id);
        if (advertiser != nullptr) {
            callback = advertiser->on_java_callback(p_event, p_arg);
        }
    }
#endif

    result = callback();

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

BluetoothAdvertiserAndroid *BluetoothAndroid::get_advertiser(int p_advertiser_id) {
    _THREAD_SAFE_METHOD_

    if (advertisers.has(p_advertiser_id)) {
        return advertisers[p_advertiser_id];
    }

    return nullptr;
}

BluetoothEnumeratorAndroid *BluetoothAndroid::get_enumerator(int p_enumerator_id) {
    _THREAD_SAFE_METHOD_

    if (enumerators.has(p_enumerator_id)) {
        return enumerators[p_enumerator_id];
    }

    return nullptr;
}
