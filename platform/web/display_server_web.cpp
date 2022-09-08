/*************************************************************************/
/*  display_server_web.cpp                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "display_server_web.h"

#ifdef GLES3_ENABLED
#include "drivers/gles3/rasterizer_gles3.h"
#endif
#include "platform/web/os_web.h"
#include "servers/rendering/dummy/rasterizer_dummy.h"

#include <emscripten.h>
#include <png.h>

#include "dom_keys.inc"
#include "godot_js.h"

#define DOM_BUTTON_LEFT 0
#define DOM_BUTTON_MIDDLE 1
#define DOM_BUTTON_RIGHT 2
#define DOM_BUTTON_XBUTTON1 3
#define DOM_BUTTON_XBUTTON2 4

DisplayServerWeb *DisplayServerWeb::get_singleton() {
	return static_cast<DisplayServerWeb *>(DisplayServer::get_singleton());
}

// Window (canvas)
bool DisplayServerWeb::check_size_force_redraw() {
	return godot_js_display_size_update() != 0;
}

void DisplayServerWeb::fullscreen_change_callback(int p_fullscreen) {
	DisplayServerWeb *display = get_singleton();
	if (p_fullscreen) {
		display->window_mode = WINDOW_MODE_FULLSCREEN;
	} else {
		display->window_mode = WINDOW_MODE_WINDOWED;
	}
}

// Drag and drop callback.
void DisplayServerWeb::drop_files_js_callback(char **p_filev, int p_filec) {
	DisplayServerWeb *ds = get_singleton();
	if (!ds) {
		ERR_FAIL_MSG("Unable to drop files because the DisplayServer is not active");
	}
	if (ds->drop_files_callback.is_null()) {
		return;
	}
	Vector<String> files;
	for (int i = 0; i < p_filec; i++) {
		files.push_back(String::utf8(p_filev[i]));
	}
	Variant v = files;
	Variant *vp = &v;
	Variant ret;
	Callable::CallError ce;
	ds->drop_files_callback.callp((const Variant **)&vp, 1, ret, ce);
}

// Web quit request callback.
void DisplayServerWeb::request_quit_callback() {
	DisplayServerWeb *ds = get_singleton();
	if (ds && !ds->window_event_callback.is_null()) {
		Variant event = int(DisplayServer::WINDOW_EVENT_CLOSE_REQUEST);
		Variant *eventp = &event;
		Variant ret;
		Callable::CallError ce;
		ds->window_event_callback.callp((const Variant **)&eventp, 1, ret, ce);
	}
}

// Keys

void DisplayServerWeb::dom2godot_mod(Ref<InputEventWithModifiers> ev, int p_mod) {
	ev->set_shift_pressed(p_mod & 1);
	ev->set_alt_pressed(p_mod & 2);
	ev->set_ctrl_pressed(p_mod & 4);
	ev->set_meta_pressed(p_mod & 8);
}

void DisplayServerWeb::key_callback(int p_pressed, int p_repeat, int p_modifiers) {
	DisplayServerWeb *ds = get_singleton();
	JSKeyEvent &key_event = ds->key_event;
	// Resume audio context after input in case autoplay was denied.
	OS_Web::get_singleton()->resume_audio();

	Ref<InputEventKey> ev;
	ev.instantiate();
	ev->set_echo(p_repeat);
	ev->set_keycode(dom_code2godot_scancode(key_event.code, key_event.key, false));
	ev->set_physical_keycode(dom_code2godot_scancode(key_event.code, key_event.key, true));
	ev->set_pressed(p_pressed);
	dom2godot_mod(ev, p_modifiers);

	String unicode = String::utf8(key_event.key);
	if (unicode.length() == 1) {
		ev->set_unicode(unicode[0]);
	}
	Input::get_singleton()->parse_input_event(ev);

	// Make sure to flush all events so we can call restricted APIs inside the event.
	Input::get_singleton()->flush_buffered_events();
}

// Mouse

int DisplayServerWeb::mouse_button_callback(int p_pressed, int p_button, double p_x, double p_y, int p_modifiers) {
	DisplayServerWeb *ds = get_singleton();

	Point2 pos(p_x, p_y);
	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	ev->set_position(pos);
	ev->set_global_position(pos);
	ev->set_pressed(p_pressed);
	dom2godot_mod(ev, p_modifiers);

	switch (p_button) {
		case DOM_BUTTON_LEFT:
			ev->set_button_index(MouseButton::LEFT);
			break;
		case DOM_BUTTON_MIDDLE:
			ev->set_button_index(MouseButton::MIDDLE);
			break;
		case DOM_BUTTON_RIGHT:
			ev->set_button_index(MouseButton::RIGHT);
			break;
		case DOM_BUTTON_XBUTTON1:
			ev->set_button_index(MouseButton::MB_XBUTTON1);
			break;
		case DOM_BUTTON_XBUTTON2:
			ev->set_button_index(MouseButton::MB_XBUTTON2);
			break;
		default:
			return false;
	}

	if (p_pressed) {
		uint64_t diff = (OS::get_singleton()->get_ticks_usec() / 1000) - ds->last_click_ms;

		if (ev->get_button_index() == ds->last_click_button_index) {
			if (diff < 400 && Point2(ds->last_click_pos).distance_to(ev->get_position()) < 5) {
				ds->last_click_ms = 0;
				ds->last_click_pos = Point2(-100, -100);
				ds->last_click_button_index = MouseButton::NONE;
				ev->set_double_click(true);
			}

		} else {
			ds->last_click_button_index = ev->get_button_index();
		}

		if (!ev->is_double_click()) {
			ds->last_click_ms += diff;
			ds->last_click_pos = ev->get_position();
		}
	}

	MouseButton mask = Input::get_singleton()->get_mouse_button_mask();
	MouseButton button_flag = mouse_button_to_mask(ev->get_button_index());
	if (ev->is_pressed()) {
		mask |= button_flag;
	} else if ((mask & button_flag) != MouseButton::NONE) {
		mask &= ~button_flag;
	} else {
		// Received release event, but press was outside the canvas, so ignore.
		return false;
	}
	ev->set_button_mask(mask);

	Input::get_singleton()->parse_input_event(ev);
	// Resume audio context after input in case autoplay was denied.
	OS_Web::get_singleton()->resume_audio();

	// Make sure to flush all events so we can call restricted APIs inside the event.
	Input::get_singleton()->flush_buffered_events();

	// Prevent multi-click text selection and wheel-click scrolling anchor.
	// Context menu is prevented through contextmenu event.
	return true;
}

void DisplayServerWeb::mouse_move_callback(double p_x, double p_y, double p_rel_x, double p_rel_y, int p_modifiers) {
	MouseButton input_mask = Input::get_singleton()->get_mouse_button_mask();
	// For motion outside the canvas, only read mouse movement if dragging
	// started inside the canvas; imitating desktop app behaviour.
	if (!get_singleton()->cursor_inside_canvas && input_mask == MouseButton::NONE) {
		return;
	}

	Point2 pos(p_x, p_y);
	Ref<InputEventMouseMotion> ev;
	ev.instantiate();
	dom2godot_mod(ev, p_modifiers);
	ev->set_button_mask(input_mask);

	ev->set_position(pos);
	ev->set_global_position(pos);

	ev->set_relative(Vector2(p_rel_x, p_rel_y));
	ev->set_velocity(Input::get_singleton()->get_last_mouse_velocity());

	Input::get_singleton()->parse_input_event(ev);
}

// Cursor
const char *DisplayServerWeb::godot2dom_cursor(DisplayServer::CursorShape p_shape) {
	switch (p_shape) {
		case DisplayServer::CURSOR_ARROW:
			return "default";
		case DisplayServer::CURSOR_IBEAM:
			return "text";
		case DisplayServer::CURSOR_POINTING_HAND:
			return "pointer";
		case DisplayServer::CURSOR_CROSS:
			return "crosshair";
		case DisplayServer::CURSOR_WAIT:
			return "wait";
		case DisplayServer::CURSOR_BUSY:
			return "progress";
		case DisplayServer::CURSOR_DRAG:
			return "grab";
		case DisplayServer::CURSOR_CAN_DROP:
			return "grabbing";
		case DisplayServer::CURSOR_FORBIDDEN:
			return "no-drop";
		case DisplayServer::CURSOR_VSIZE:
			return "ns-resize";
		case DisplayServer::CURSOR_HSIZE:
			return "ew-resize";
		case DisplayServer::CURSOR_BDIAGSIZE:
			return "nesw-resize";
		case DisplayServer::CURSOR_FDIAGSIZE:
			return "nwse-resize";
		case DisplayServer::CURSOR_MOVE:
			return "move";
		case DisplayServer::CURSOR_VSPLIT:
			return "row-resize";
		case DisplayServer::CURSOR_HSPLIT:
			return "col-resize";
		case DisplayServer::CURSOR_HELP:
			return "help";
		default:
			return "default";
	}
}

bool DisplayServerWeb::tts_is_speaking() const {
	return godot_js_tts_is_speaking();
}

bool DisplayServerWeb::tts_is_paused() const {
	return godot_js_tts_is_paused();
}

void DisplayServerWeb::update_voices_callback(int p_size, const char **p_voice) {
	get_singleton()->voices.clear();
	for (int i = 0; i < p_size; i++) {
		Vector<String> tokens = String::utf8(p_voice[i]).split(";", true, 2);
		if (tokens.size() == 2) {
			Dictionary voice_d;
			voice_d["name"] = tokens[1];
			voice_d["id"] = tokens[1];
			voice_d["language"] = tokens[0];
			get_singleton()->voices.push_back(voice_d);
		}
	}
}

TypedArray<Dictionary> DisplayServerWeb::tts_get_voices() const {
	godot_js_tts_get_voices(update_voices_callback);
	return voices;
}

void DisplayServerWeb::tts_speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
	if (p_interrupt) {
		tts_stop();
	}

	if (p_text.is_empty()) {
		tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, p_utterance_id);
		return;
	}

	CharString string = p_text.utf8();
	utterance_ids[p_utterance_id] = string;

	godot_js_tts_speak(string.get_data(), p_voice.utf8().get_data(), CLAMP(p_volume, 0, 100), CLAMP(p_pitch, 0.f, 2.f), CLAMP(p_rate, 0.1f, 10.f), p_utterance_id, DisplayServerWeb::_js_utterance_callback);
}

void DisplayServerWeb::tts_pause() {
	godot_js_tts_pause();
}

void DisplayServerWeb::tts_resume() {
	godot_js_tts_resume();
}

void DisplayServerWeb::tts_stop() {
	for (const KeyValue<int, CharString> &E : utterance_ids) {
		tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, E.key);
	}
	utterance_ids.clear();
	godot_js_tts_stop();
}

void DisplayServerWeb::_js_utterance_callback(int p_event, int p_id, int p_pos) {
	DisplayServerWeb *ds = (DisplayServerWeb *)DisplayServer::get_singleton();
	if (ds->utterance_ids.has(p_id)) {
		int pos = 0;
		if ((TTSUtteranceEvent)p_event == DisplayServer::TTS_UTTERANCE_BOUNDARY) {
			// Convert position from UTF-8 to UTF-32.
			const CharString &string = ds->utterance_ids[p_id];
			for (int i = 0; i < MIN(p_pos, string.length()); i++) {
				uint8_t c = string[i];
				if ((c & 0xe0) == 0xc0) {
					i += 1;
				} else if ((c & 0xf0) == 0xe0) {
					i += 2;
				} else if ((c & 0xf8) == 0xf0) {
					i += 3;
				}
				pos++;
			}
		} else if ((TTSUtteranceEvent)p_event != DisplayServer::TTS_UTTERANCE_STARTED) {
			ds->utterance_ids.erase(p_id);
		}
		ds->tts_post_utterance_event((TTSUtteranceEvent)p_event, p_id, pos);
	}
}

void DisplayServerWeb::cursor_set_shape(CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	if (cursor_shape == p_shape) {
		return;
	}
	cursor_shape = p_shape;
	godot_js_display_cursor_set_shape(godot2dom_cursor(cursor_shape));
}

DisplayServer::CursorShape DisplayServerWeb::cursor_get_shape() const {
	return cursor_shape;
}

void DisplayServerWeb::cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	if (p_cursor.is_valid()) {
		Ref<Texture2D> texture = p_cursor;
		Ref<AtlasTexture> atlas_texture = p_cursor;
		Ref<Image> image;
		Size2 texture_size;
		Rect2 atlas_rect;

		if (texture.is_valid()) {
			image = texture->get_image();
		}

		if (!image.is_valid() && atlas_texture.is_valid()) {
			texture = atlas_texture->get_atlas();

			atlas_rect.size.width = texture->get_width();
			atlas_rect.size.height = texture->get_height();
			atlas_rect.position.x = atlas_texture->get_region().position.x;
			atlas_rect.position.y = atlas_texture->get_region().position.y;

			texture_size.width = atlas_texture->get_region().size.x;
			texture_size.height = atlas_texture->get_region().size.y;
		} else if (image.is_valid()) {
			texture_size.width = texture->get_width();
			texture_size.height = texture->get_height();
		}

		ERR_FAIL_COND(!texture.is_valid());
		ERR_FAIL_COND(p_hotspot.x < 0 || p_hotspot.y < 0);
		ERR_FAIL_COND(texture_size.width > 256 || texture_size.height > 256);
		ERR_FAIL_COND(p_hotspot.x > texture_size.width || p_hotspot.y > texture_size.height);

		image = texture->get_image();

		ERR_FAIL_COND(!image.is_valid());

		image = image->duplicate();

		if (atlas_texture.is_valid()) {
			image->crop_from_point(
					atlas_rect.position.x,
					atlas_rect.position.y,
					texture_size.width,
					texture_size.height);
		}

		if (image->get_format() != Image::FORMAT_RGBA8) {
			image->convert(Image::FORMAT_RGBA8);
		}

		png_image png_meta;
		memset(&png_meta, 0, sizeof png_meta);
		png_meta.version = PNG_IMAGE_VERSION;
		png_meta.width = texture_size.width;
		png_meta.height = texture_size.height;
		png_meta.format = PNG_FORMAT_RGBA;

		PackedByteArray png;
		size_t len;
		PackedByteArray data = image->get_data();
		ERR_FAIL_COND(!png_image_write_get_memory_size(png_meta, len, 0, data.ptr(), 0, nullptr));

		png.resize(len);
		ERR_FAIL_COND(!png_image_write_to_memory(&png_meta, png.ptrw(), &len, 0, data.ptr(), 0, nullptr));

		godot_js_display_cursor_set_custom_shape(godot2dom_cursor(p_shape), png.ptr(), len, p_hotspot.x, p_hotspot.y);

	} else {
		godot_js_display_cursor_set_custom_shape(godot2dom_cursor(p_shape), nullptr, 0, 0, 0);
	}

	cursor_set_shape(cursor_shape);
}

// Mouse mode
void DisplayServerWeb::mouse_set_mode(MouseMode p_mode) {
	ERR_FAIL_COND_MSG(p_mode == MOUSE_MODE_CONFINED || p_mode == MOUSE_MODE_CONFINED_HIDDEN, "MOUSE_MODE_CONFINED is not supported for the Web platform.");
	if (p_mode == mouse_get_mode()) {
		return;
	}

	if (p_mode == MOUSE_MODE_VISIBLE) {
		godot_js_display_cursor_set_visible(1);
		godot_js_display_cursor_lock_set(0);

	} else if (p_mode == MOUSE_MODE_HIDDEN) {
		godot_js_display_cursor_set_visible(0);
		godot_js_display_cursor_lock_set(0);

	} else if (p_mode == MOUSE_MODE_CAPTURED) {
		godot_js_display_cursor_set_visible(1);
		godot_js_display_cursor_lock_set(1);
	}
}

DisplayServer::MouseMode DisplayServerWeb::mouse_get_mode() const {
	if (godot_js_display_cursor_is_hidden()) {
		return MOUSE_MODE_HIDDEN;
	}

	if (godot_js_display_cursor_is_locked()) {
		return MOUSE_MODE_CAPTURED;
	}
	return MOUSE_MODE_VISIBLE;
}

Point2i DisplayServerWeb::mouse_get_position() const {
	return Input::get_singleton()->get_mouse_position();
}

// Wheel
int DisplayServerWeb::mouse_wheel_callback(double p_delta_x, double p_delta_y) {
	if (!godot_js_display_canvas_is_focused()) {
		if (get_singleton()->cursor_inside_canvas) {
			godot_js_display_canvas_focus();
		} else {
			return false;
		}
	}

	Input *input = Input::get_singleton();
	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	ev->set_position(input->get_mouse_position());
	ev->set_global_position(ev->get_position());

	ev->set_shift_pressed(input->is_key_pressed(Key::SHIFT));
	ev->set_alt_pressed(input->is_key_pressed(Key::ALT));
	ev->set_ctrl_pressed(input->is_key_pressed(Key::CTRL));
	ev->set_meta_pressed(input->is_key_pressed(Key::META));

	if (p_delta_y < 0) {
		ev->set_button_index(MouseButton::WHEEL_UP);
	} else if (p_delta_y > 0) {
		ev->set_button_index(MouseButton::WHEEL_DOWN);
	} else if (p_delta_x > 0) {
		ev->set_button_index(MouseButton::WHEEL_LEFT);
	} else if (p_delta_x < 0) {
		ev->set_button_index(MouseButton::WHEEL_RIGHT);
	} else {
		return false;
	}

	// Different browsers give wildly different delta values, and we can't
	// interpret deltaMode, so use default value for wheel events' factor.

	MouseButton button_flag = mouse_button_to_mask(ev->get_button_index());

	ev->set_pressed(true);
	ev->set_button_mask(input->get_mouse_button_mask() | button_flag);
	input->parse_input_event(ev);

	Ref<InputEventMouseButton> release = ev->duplicate();
	release->set_pressed(false);
	release->set_button_mask(MouseButton(input->get_mouse_button_mask() & ~button_flag));
	input->parse_input_event(release);

	return true;
}

// Touch
void DisplayServerWeb::touch_callback(int p_type, int p_count) {
	DisplayServerWeb *ds = get_singleton();

	const JSTouchEvent &touch_event = ds->touch_event;
	for (int i = 0; i < p_count; i++) {
		Point2 point(touch_event.coords[i * 2], touch_event.coords[i * 2 + 1]);
		if (p_type == 2) {
			// touchmove
			Ref<InputEventScreenDrag> ev;
			ev.instantiate();
			ev->set_index(touch_event.identifier[i]);
			ev->set_position(point);

			Point2 &prev = ds->touches[i];
			ev->set_relative(ev->get_position() - prev);
			prev = ev->get_position();

			Input::get_singleton()->parse_input_event(ev);
		} else {
			// touchstart/touchend
			Ref<InputEventScreenTouch> ev;

			// Resume audio context after input in case autoplay was denied.
			OS_Web::get_singleton()->resume_audio();

			ev.instantiate();
			ev->set_index(touch_event.identifier[i]);
			ev->set_position(point);
			ev->set_pressed(p_type == 0);
			ds->touches[i] = point;

			Input::get_singleton()->parse_input_event(ev);

			// Make sure to flush all events so we can call restricted APIs inside the event.
			Input::get_singleton()->flush_buffered_events();
		}
	}
}

bool DisplayServerWeb::screen_is_touchscreen(int p_screen) const {
	return godot_js_display_touchscreen_is_available();
}

// Virtual Keyboard
void DisplayServerWeb::vk_input_text_callback(const char *p_text, int p_cursor) {
	DisplayServerWeb *ds = DisplayServerWeb::get_singleton();
	if (!ds || ds->input_text_callback.is_null()) {
		return;
	}
	// Call input_text
	Variant event = String::utf8(p_text);
	Variant *eventp = &event;
	Variant ret;
	Callable::CallError ce;
	ds->input_text_callback.callp((const Variant **)&eventp, 1, ret, ce);
	// Insert key right to reach position.
	Input *input = Input::get_singleton();
	Ref<InputEventKey> k;
	for (int i = 0; i < p_cursor; i++) {
		k.instantiate();
		k->set_pressed(true);
		k->set_echo(false);
		k->set_keycode(Key::RIGHT);
		input->parse_input_event(k);
		k.instantiate();
		k->set_pressed(false);
		k->set_echo(false);
		k->set_keycode(Key::RIGHT);
		input->parse_input_event(k);
	}
}

void DisplayServerWeb::virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect, VirtualKeyboardType p_type, int p_max_input_length, int p_cursor_start, int p_cursor_end) {
	godot_js_display_vk_show(p_existing_text.utf8().get_data(), p_type, p_cursor_start, p_cursor_end);
}

void DisplayServerWeb::virtual_keyboard_hide() {
	godot_js_display_vk_hide();
}

void DisplayServerWeb::window_blur_callback() {
	Input::get_singleton()->release_pressed_events();
}

// Gamepad
void DisplayServerWeb::gamepad_callback(int p_index, int p_connected, const char *p_id, const char *p_guid) {
	Input *input = Input::get_singleton();
	if (p_connected) {
		input->joy_connection_changed(p_index, true, String::utf8(p_id), String::utf8(p_guid));
	} else {
		input->joy_connection_changed(p_index, false, "");
	}
}

void DisplayServerWeb::process_joypads() {
	Input *input = Input::get_singleton();
	int32_t pads = godot_js_input_gamepad_sample_count();
	int32_t s_btns_num = 0;
	int32_t s_axes_num = 0;
	int32_t s_standard = 0;
	float s_btns[16];
	float s_axes[10];
	for (int idx = 0; idx < pads; idx++) {
		int err = godot_js_input_gamepad_sample_get(idx, s_btns, &s_btns_num, s_axes, &s_axes_num, &s_standard);
		if (err) {
			continue;
		}
		for (int b = 0; b < s_btns_num; b++) {
			// Buttons 6 and 7 in the standard mapping need to be
			// axis to be handled as JoyAxis::TRIGGER by Godot.
			if (s_standard && (b == 6 || b == 7)) {
				input->joy_axis(idx, (JoyAxis)b, s_btns[b]);
			} else {
				input->joy_button(idx, (JoyButton)b, s_btns[b]);
			}
		}
		for (int a = 0; a < s_axes_num; a++) {
			input->joy_axis(idx, (JoyAxis)a, s_axes[a]);
		}
	}
}

Vector<String> DisplayServerWeb::get_rendering_drivers_func() {
	Vector<String> drivers;
#ifdef GLES3_ENABLED
	drivers.push_back("opengl3");
#endif
	return drivers;
}

// Clipboard
void DisplayServerWeb::update_clipboard_callback(const char *p_text) {
	get_singleton()->clipboard = String::utf8(p_text);
}

void DisplayServerWeb::clipboard_set(const String &p_text) {
	clipboard = p_text;
	int err = godot_js_display_clipboard_set(p_text.utf8().get_data());
	ERR_FAIL_COND_MSG(err, "Clipboard API is not supported.");
}

String DisplayServerWeb::clipboard_get() const {
	godot_js_display_clipboard_get(update_clipboard_callback);
	return clipboard;
}

void DisplayServerWeb::send_window_event_callback(int p_notification) {
	DisplayServerWeb *ds = get_singleton();
	if (!ds) {
		return;
	}
	if (p_notification == DisplayServer::WINDOW_EVENT_MOUSE_ENTER || p_notification == DisplayServer::WINDOW_EVENT_MOUSE_EXIT) {
		ds->cursor_inside_canvas = p_notification == DisplayServer::WINDOW_EVENT_MOUSE_ENTER;
	}
	if (!ds->window_event_callback.is_null()) {
		Variant event = int(p_notification);
		Variant *eventp = &event;
		Variant ret;
		Callable::CallError ce;
		ds->window_event_callback.callp((const Variant **)&eventp, 1, ret, ce);
	}
}

void DisplayServerWeb::set_icon(const Ref<Image> &p_icon) {
	ERR_FAIL_COND(p_icon.is_null());
	Ref<Image> icon = p_icon;
	if (icon->is_compressed()) {
		icon = icon->duplicate();
		ERR_FAIL_COND(icon->decompress() != OK);
	}
	if (icon->get_format() != Image::FORMAT_RGBA8) {
		if (icon == p_icon) {
			icon = icon->duplicate();
		}
		icon->convert(Image::FORMAT_RGBA8);
	}

	png_image png_meta;
	memset(&png_meta, 0, sizeof png_meta);
	png_meta.version = PNG_IMAGE_VERSION;
	png_meta.width = icon->get_width();
	png_meta.height = icon->get_height();
	png_meta.format = PNG_FORMAT_RGBA;

	PackedByteArray png;
	size_t len;
	PackedByteArray data = icon->get_data();
	ERR_FAIL_COND(!png_image_write_get_memory_size(png_meta, len, 0, data.ptr(), 0, nullptr));

	png.resize(len);
	ERR_FAIL_COND(!png_image_write_to_memory(&png_meta, png.ptrw(), &len, 0, data.ptr(), 0, nullptr));

	godot_js_display_window_icon_set(png.ptr(), len);
}

void DisplayServerWeb::_dispatch_input_event(const Ref<InputEvent> &p_event) {
	Callable cb = get_singleton()->input_event_callback;
	if (!cb.is_null()) {
		Variant ev = p_event;
		Variant *evp = &ev;
		Variant ret;
		Callable::CallError ce;
		cb.callp((const Variant **)&evp, 1, ret, ce);
	}
}

DisplayServer *DisplayServerWeb::create_func(const String &p_rendering_driver, WindowMode p_window_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Size2i &p_resolution, Error &r_error) {
	return memnew(DisplayServerWeb(p_rendering_driver, p_window_mode, p_vsync_mode, p_flags, p_resolution, r_error));
}

DisplayServerWeb::DisplayServerWeb(const String &p_rendering_driver, WindowMode p_window_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Size2i &p_resolution, Error &r_error) {
	r_error = OK; // Always succeeds for now.

	// Ensure the canvas ID.
	godot_js_config_canvas_id_get(canvas_id, 256);

	// Handle contextmenu, webglcontextlost
	godot_js_display_setup_canvas(p_resolution.x, p_resolution.y, (p_window_mode == WINDOW_MODE_FULLSCREEN || p_window_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN), OS::get_singleton()->is_hidpi_allowed() ? 1 : 0);

	// Check if it's windows.
	swap_cancel_ok = godot_js_display_is_swap_ok_cancel() == 1;

	// Expose method for requesting quit.
	godot_js_os_request_quit_cb(request_quit_callback);

#ifdef GLES3_ENABLED
	// TODO "vulkan" defaults to webgl2 for now.
	bool wants_webgl2 = p_rendering_driver == "opengl3" || p_rendering_driver == "vulkan";
	bool webgl2_init_failed = wants_webgl2 && !godot_js_display_has_webgl(2);
	if (wants_webgl2 && !webgl2_init_failed) {
		EmscriptenWebGLContextAttributes attributes;
		emscripten_webgl_init_context_attributes(&attributes);
		attributes.alpha = OS::get_singleton()->is_layered_allowed();
		attributes.antialias = false;
		attributes.majorVersion = 2;
		attributes.explicitSwapControl = true;

		webgl_ctx = emscripten_webgl_create_context(canvas_id, &attributes);
		if (emscripten_webgl_make_context_current(webgl_ctx) != EMSCRIPTEN_RESULT_SUCCESS) {
			webgl2_init_failed = true;
		} else {
			RasterizerGLES3::make_current();
		}
	}
	if (webgl2_init_failed) {
		OS::get_singleton()->alert("Your browser does not seem to support WebGL2. Please update your browser version.",
				"Unable to initialize video driver");
	}
	if (!wants_webgl2 || webgl2_init_failed) {
		RasterizerDummy::make_current();
	}
#else
	RasterizerDummy::make_current();
#endif

	// JS Input interface (js/libs/library_godot_input.js)
	godot_js_input_mouse_button_cb(&DisplayServerWeb::mouse_button_callback);
	godot_js_input_mouse_move_cb(&DisplayServerWeb::mouse_move_callback);
	godot_js_input_mouse_wheel_cb(&DisplayServerWeb::mouse_wheel_callback);
	godot_js_input_touch_cb(&DisplayServerWeb::touch_callback, touch_event.identifier, touch_event.coords);
	godot_js_input_key_cb(&DisplayServerWeb::key_callback, key_event.code, key_event.key);
	godot_js_input_paste_cb(update_clipboard_callback);
	godot_js_input_drop_files_cb(drop_files_js_callback);
	godot_js_input_gamepad_cb(&DisplayServerWeb::gamepad_callback);

	// JS Display interface (js/libs/library_godot_display.js)
	godot_js_display_fullscreen_cb(&DisplayServerWeb::fullscreen_change_callback);
	godot_js_display_window_blur_cb(&window_blur_callback);
	godot_js_display_notification_cb(&send_window_event_callback,
			WINDOW_EVENT_MOUSE_ENTER,
			WINDOW_EVENT_MOUSE_EXIT,
			WINDOW_EVENT_FOCUS_IN,
			WINDOW_EVENT_FOCUS_OUT);
	godot_js_display_vk_cb(&vk_input_text_callback);

	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_event);
}

DisplayServerWeb::~DisplayServerWeb() {
#ifdef GLES3_ENABLED
	if (webgl_ctx) {
		emscripten_webgl_commit_frame();
		emscripten_webgl_destroy_context(webgl_ctx);
	}
#endif
}

bool DisplayServerWeb::has_feature(Feature p_feature) const {
	switch (p_feature) {
		//case FEATURE_GLOBAL_MENU:
		//case FEATURE_HIDPI:
		//case FEATURE_IME:
		case FEATURE_ICON:
		case FEATURE_CLIPBOARD:
		case FEATURE_CURSOR_SHAPE:
		case FEATURE_CUSTOM_CURSOR_SHAPE:
		case FEATURE_MOUSE:
		case FEATURE_TOUCHSCREEN:
			return true;
		//case FEATURE_MOUSE_WARP:
		//case FEATURE_NATIVE_DIALOG:
		//case FEATURE_NATIVE_ICON:
		//case FEATURE_WINDOW_TRANSPARENCY:
		//case FEATURE_KEEP_SCREEN_ON:
		//case FEATURE_ORIENTATION:
		case FEATURE_VIRTUAL_KEYBOARD:
			return godot_js_display_vk_available() != 0;
		case FEATURE_TEXT_TO_SPEECH:
			return godot_js_display_tts_available() != 0;
		default:
			return false;
	}
}

void DisplayServerWeb::register_web_driver() {
	register_create_function("web", create_func, get_rendering_drivers_func);
}

String DisplayServerWeb::get_name() const {
	return "web";
}

int DisplayServerWeb::get_screen_count() const {
	return 1;
}

Point2i DisplayServerWeb::screen_get_position(int p_screen) const {
	return Point2i(); // TODO offsetX/Y?
}

Size2i DisplayServerWeb::screen_get_size(int p_screen) const {
	int size[2];
	godot_js_display_screen_size_get(size, size + 1);
	return Size2(size[0], size[1]);
}

Rect2i DisplayServerWeb::screen_get_usable_rect(int p_screen) const {
	int size[2];
	godot_js_display_window_size_get(size, size + 1);
	return Rect2i(0, 0, size[0], size[1]);
}

int DisplayServerWeb::screen_get_dpi(int p_screen) const {
	return godot_js_display_screen_dpi_get();
}

float DisplayServerWeb::screen_get_scale(int p_screen) const {
	return godot_js_display_pixel_ratio_get();
}

float DisplayServerWeb::screen_get_refresh_rate(int p_screen) const {
	return SCREEN_REFRESH_RATE_FALLBACK; // Web doesn't have much of a need for the screen refresh rate, and there's no native way to do so.
}

Vector<DisplayServer::WindowID> DisplayServerWeb::get_window_list() const {
	Vector<WindowID> ret;
	ret.push_back(MAIN_WINDOW_ID);
	return ret;
}

DisplayServerWeb::WindowID DisplayServerWeb::get_window_at_screen_position(const Point2i &p_position) const {
	return MAIN_WINDOW_ID;
}

void DisplayServerWeb::window_attach_instance_id(ObjectID p_instance, WindowID p_window) {
	window_attached_instance_id = p_instance;
}

ObjectID DisplayServerWeb::window_get_attached_instance_id(WindowID p_window) const {
	return window_attached_instance_id;
}

void DisplayServerWeb::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {
	// Not supported.
}

void DisplayServerWeb::window_set_window_event_callback(const Callable &p_callable, WindowID p_window) {
	window_event_callback = p_callable;
}

void DisplayServerWeb::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {
	input_event_callback = p_callable;
}

void DisplayServerWeb::window_set_input_text_callback(const Callable &p_callable, WindowID p_window) {
	input_text_callback = p_callable;
}

void DisplayServerWeb::window_set_drop_files_callback(const Callable &p_callable, WindowID p_window) {
	drop_files_callback = p_callable;
}

void DisplayServerWeb::window_set_title(const String &p_title, WindowID p_window) {
	godot_js_display_window_title_set(p_title.utf8().get_data());
}

int DisplayServerWeb::window_get_current_screen(WindowID p_window) const {
	return 1;
}

void DisplayServerWeb::window_set_current_screen(int p_screen, WindowID p_window) {
	// Not implemented.
}

Point2i DisplayServerWeb::window_get_position(WindowID p_window) const {
	return Point2i(); // TODO Does this need implementation?
}

void DisplayServerWeb::window_set_position(const Point2i &p_position, WindowID p_window) {
	// Not supported.
}

void DisplayServerWeb::window_set_transient(WindowID p_window, WindowID p_parent) {
	// Not supported.
}

void DisplayServerWeb::window_set_max_size(const Size2i p_size, WindowID p_window) {
	// Not supported.
}

Size2i DisplayServerWeb::window_get_max_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerWeb::window_set_min_size(const Size2i p_size, WindowID p_window) {
	// Not supported.
}

Size2i DisplayServerWeb::window_get_min_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerWeb::window_set_size(const Size2i p_size, WindowID p_window) {
	godot_js_display_desired_size_set(p_size.x, p_size.y);
}

Size2i DisplayServerWeb::window_get_size(WindowID p_window) const {
	int size[2];
	godot_js_display_window_size_get(size, size + 1);
	return Size2i(size[0], size[1]);
}

Size2i DisplayServerWeb::window_get_real_size(WindowID p_window) const {
	return window_get_size(p_window);
}

void DisplayServerWeb::window_set_mode(WindowMode p_mode, WindowID p_window) {
	if (window_mode == p_mode) {
		return;
	}

	switch (p_mode) {
		case WINDOW_MODE_WINDOWED: {
			if (window_mode == WINDOW_MODE_FULLSCREEN) {
				godot_js_display_fullscreen_exit();
			}
			window_mode = WINDOW_MODE_WINDOWED;
		} break;
		case WINDOW_MODE_EXCLUSIVE_FULLSCREEN:
		case WINDOW_MODE_FULLSCREEN: {
			int result = godot_js_display_fullscreen_request();
			ERR_FAIL_COND_MSG(result, "The request was denied. Remember that enabling fullscreen is only possible from an input callback for the Web platform.");
		} break;
		case WINDOW_MODE_MAXIMIZED:
		case WINDOW_MODE_MINIMIZED:
			WARN_PRINT("WindowMode MAXIMIZED and MINIMIZED are not supported in Web platform.");
			break;
		default:
			break;
	}
}

DisplayServerWeb::WindowMode DisplayServerWeb::window_get_mode(WindowID p_window) const {
	return window_mode;
}

bool DisplayServerWeb::window_is_maximize_allowed(WindowID p_window) const {
	return false;
}

void DisplayServerWeb::window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window) {
	// Not supported.
}

bool DisplayServerWeb::window_get_flag(WindowFlags p_flag, WindowID p_window) const {
	return false;
}

void DisplayServerWeb::window_request_attention(WindowID p_window) {
	// Not supported.
}

void DisplayServerWeb::window_move_to_foreground(WindowID p_window) {
	// Not supported.
}

bool DisplayServerWeb::window_can_draw(WindowID p_window) const {
	return true;
}

bool DisplayServerWeb::can_any_window_draw() const {
	return true;
}

void DisplayServerWeb::process_events() {
	Input::get_singleton()->flush_buffered_events();
	if (godot_js_input_gamepad_sample() == OK) {
		process_joypads();
	}
}

int DisplayServerWeb::get_current_video_driver() const {
	return 1;
}

bool DisplayServerWeb::get_swap_cancel_ok() {
	return swap_cancel_ok;
}

void DisplayServerWeb::swap_buffers() {
#ifdef GLES3_ENABLED
	if (webgl_ctx) {
		emscripten_webgl_commit_frame();
	}
#endif
}
