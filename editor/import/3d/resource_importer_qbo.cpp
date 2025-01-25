/**************************************************************************/
/*  resource_importer_qbo.cpp                                             */
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

#include "resource_importer_qbo.h"
#include "resource_importer_obj.h"

#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/3d/importer_mesh.h"
#include "scene/resources/3d/skin.h"
#include "scene/resources/animation.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/surface_tool.h"

#define BVH_X_POSITION 1
#define BVH_Y_POSITION 2
#define BVH_Z_POSITION 3
#define BVH_X_ROTATION 4
#define BVH_Y_ROTATION 5
#define BVH_Z_ROTATION 6
#define BVH_W_ROTATION 7

uint32_t EditorQBOImporter::get_import_flags() const {
	return IMPORT_SCENE | IMPORT_ANIMATION;
}

static Error _parse_motion(Ref<FileAccess> f, List<Skeleton3D *> &r_skeletons, AnimationPlayer **r_animation) {
	bool motion = false;
	int frame_count = -1;
	double frame_time = 0.03333333;
	HashMap<int, int> tracks;
	HashMap<String, int> bone_names;
	Vector<String> frames;
	Vector<int> bones;
	Vector<Vector3> offsets;
	Vector<Vector<int>> channels;
	Ref<Animation> animation;
	Ref<AnimationLibrary> animation_library;
	String animation_library_name = f->get_path().get_basename().strip_edges();
	if (animation_library_name.contains(".")) {
		animation_library_name = animation_library_name.substr(0, animation_library_name.find("."));
	}
	animation_library_name = AnimationLibrary::validate_library_name(animation_library_name);
	if (r_animation != nullptr) {
		if (*r_animation == nullptr) {
			animation_library.instantiate();
			animation_library->set_name(animation_library_name);
			*r_animation = memnew(AnimationPlayer);
			(*r_animation)->add_animation_library(animation_library_name, animation_library);
		} else {
			List<StringName> libraries;
			(*r_animation)->get_animation_library_list(&libraries);
			for (int i = 0; i < libraries.size(); i++) {
				String library_name = libraries.get(i);
				if (library_name.is_empty()) {
					continue;
				}
				animation_library = (*r_animation)->get_animation_library(animation_library_name);
				if (animation_library.is_valid()) {
					animation_library_name = animation_library->get_name();
					break;
				}
			}
		}
	} else if (OS::get_singleton()->has_feature("debug")) {
		print_verbose("NO ANIMATION");
	}

	int loops = 0;
	int blanks = 0;
	while (true) {
		String l = f->get_line().strip_edges();
		//print_verbose(l);
		if (++loops % 100 == 0 && OS::get_singleton()->has_feature("debug")) {
			print_verbose(String::num_int64(loops) + " BVH loops");
		}
		if (l.is_empty() && f->eof_reached()) {
			break;
		}
		if (l.is_empty()) {
			if (++blanks > 1) {
				break;
			}
			continue;
		}
		if (l.begins_with("#")) {
			continue;
		}

		if (motion) {
			if (l.begins_with("HIERARCHY")) {
				motion = false;
			} else if (l.begins_with("Frame Time: ")) {
				Vector<String> s = l.split(":");
				l = s[1].strip_edges();
				frame_time = l.to_float();
			} else if (l.begins_with("Frames: ")) {
				Vector<String> s = l.split(":");
				l = s[1].strip_edges();
				frame_count = l.to_int();
			} else {
				frames.append(l);
				if (frames.size() == frame_count) {
					motion = false;
				}
			}
			continue;
		}

		if (l.begins_with("HIERARCHY")) {
			l = l.substr(10);
			if (!l.is_empty()) {
				animation_library_name = l;
			}
			continue;
		} else if (l.begins_with("ROOT")) {
			String bone_name = "";
			int bone = -1;
			bones.clear();
			offsets.clear();
			channels.clear();
			r_skeletons.push_back(memnew(Skeleton3D));
			l = l.substr(5);
			if (!l.is_empty()) {
				bone_name += l;
			} else {
				bone_name += "ROOT";
			}
			if (!bone_name.is_empty()) {
				if (bone_names.has(bone_name)) {
					bone_names[bone_name] += 1;
					bone_name += String::num_int64(bone_names[bone_name]);
				} else {
					bone_names[bone_name] = 1;
				}
				r_skeletons.back()->get()->set_name(animation_library_name);
				bone = r_skeletons.back()->get()->add_bone(bone_name);
			}
			if (bone >= 0) {
				bones.append(bone);
				offsets.append(Vector3());
				channels.append(Vector<int>({bones[bones.size() - 1]}));
			}
		} else if (l.begins_with("MOTION")) {
			motion = true;
			if (animation_library.is_valid() && animation.is_valid() && r_animation != nullptr && frames.size() == frame_count) {
				if (!channels.is_empty() && !r_skeletons.is_empty()) {
					tracks.clear();
					for (int i = 0; i < channels.size(); i++) {
						if (channels[i].size() < 2) {
							continue;
						}
						tracks[channels[i][0]] = animation->add_track(Animation::TrackType::TYPE_POSITION_3D);
						tracks[channels.size() + channels[i][0]] = animation->add_track(Animation::TrackType::TYPE_ROTATION_3D);
					}
					for (int i = 0; i < frame_count; i++) {
						int bone_index = 0;
						int channel_index = -1;
						String frame = frames[i];
						Vector<String> s;
						if (frame.contains(" ")) {
							s = frame.split(" ");
						} else {
							s = frame.split("\t");
						}
						for (int j = 0; j < s.size(); j++) {
							channel_index++;
							if (channel_index >= channels[bone_index].size() - 1 || channels[bone_index].size() < 2) {
								do {
									bone_index++;
									if (bone_index >= channels.size()) {
										break;
									}
								} while (channels[bone_index].size() < 2);
								channel_index = 0;
							}
							if (bone_index < 0 || bone_index >= channels.size()) {
								break;
							}
							Vector3 position;
							Quaternion rotation;
							String bone_name = r_skeletons.back()->get()->get_bone_name(channels[bone_index][0]);
							int position_track = tracks[channels[bone_index][0]];
							int rotation_track = tracks[channels.size() + channels[bone_index][0]];
							int insertion = -1;
							switch (channels[bone_index][channel_index + 1]) {
								case BVH_X_POSITION:
								case BVH_Y_POSITION:
								case BVH_Z_POSITION:
									if (channel_index + 3 < channels[bone_index].size()) {
										for (int k = j; k < j + 3 && k < s.size(); k++) {
											switch (channels[bone_index][channel_index + 1 + (k - j)]) {
												case BVH_X_POSITION:
													position.x = s[k].to_float();
													break;
												case BVH_Y_POSITION:
													position.y = s[k].to_float();
													break;
												case BVH_Z_POSITION:
													position.z = s[k].to_float();
													break;
											}
										}
										animation->track_set_path(position_track, "" + r_skeletons.back()->get()->get_name() + ":" + bone_name);
										insertion = animation->track_insert_key(position_track, frame_time * static_cast<double>(i), position);
										j += 2;
										channel_index += 2;
									} else {
										//print_verbose(String::num_int64(channel_index) + " " + String::num_int64(channels[bone_index].size()));
									}
									break;
								case BVH_X_ROTATION:
								case BVH_Y_ROTATION:
								case BVH_Z_ROTATION:
								case BVH_W_ROTATION:
									if (channel_index + 4 < channels[bone_index].size()) {
										for (int k = j; k < j + 4 && k < s.size(); k++) {
											switch (channels[bone_index][channel_index + 1 + (k - j)]) {
												case BVH_X_ROTATION:
													rotation.x = s[k].to_float();
													break;
												case BVH_Y_ROTATION:
													rotation.y = s[k].to_float();
													break;
												case BVH_Z_ROTATION:
													rotation.z = s[k].to_float();
													break;
												case BVH_W_ROTATION:
													rotation.w = s[k].to_float();
													break;
											}
										}
										animation->track_set_path(rotation_track, "" + r_skeletons.back()->get()->get_name() + ":" + bone_name);
										insertion = animation->track_insert_key(rotation_track, frame_time * static_cast<double>(i), rotation.normalized());
										j += 3;
										channel_index += 3;
									} else {
										//print_verbose(String::num_int64(channel_index) + " " + String::num_int64(channels[bone_index].size()));
									}
									break;
								default:
									//print_verbose(String::num_int64(position_track) + " " + String::num_int64(rotation_track) + bone_name + " @ " + String::num_int64(channel_index));
									break;
							}
							if (insertion < 0 && OS::get_singleton()->has_feature("debug")) {
								//print_verbose(String::num_int64(insertion) + " BVH track insertion");
							}
						}
					}
				}
				if (!animation->get_name().is_empty()) {
					animation_library_name = animation->get_name();
				}
				animation->set_step(frame_time);
				animation_library->add_animation(animation_library_name, animation);
				if (r_animation != nullptr) {
					(*r_animation)->set_assigned_animation(animation_library->get_name() + "/" + animation->get_name());
				}
			}
			animation.instantiate();
			frame_count = -1;
			frames.clear();
			l = l.substr(7);
			if (!l.is_empty()) {
				animation->set_name(l.strip_edges());
			} else {
				animation->set_name("MOTION");
			}
		} else if (l.begins_with("End ")) {
			ERR_FAIL_COND_V(r_skeletons.is_empty(), ERR_FILE_CORRUPT);
			l = l.substr(4);
			if (!l.is_empty()) {
				if (bone_names.has(l)) {
					bone_names[l] += 1;
					l += String::num_int64(bone_names[l]);
				} else {
					bone_names[l] = 1;
				}
				bones.append(r_skeletons.back()->get()->add_bone(l));
				offsets.append(Vector3());
				channels.append(Vector<int>({bones[bones.size() - 1]}));
			}
		} else {
			Vector<String> s;
			if (l.contains(" ")) {
				s = l.split(" ");
			} else {
				s = l.split("\t");
			}
			if (s.size() > 1) {
				if (s[0].casecmp_to("OFFSET") == 0) {
					ERR_FAIL_COND_V(s.size() < 4, ERR_FILE_CORRUPT);
					Vector3 offset;
					offset.x = s[1].to_float();
					offset.y = s[2].to_float();
					offset.z = s[3].to_float();
					offsets.append(offset);
				} else if (s[0].casecmp_to("CHANNELS") == 0) {
					int channel_count = s[1].to_int();
					ERR_FAIL_COND_V(s.size() < channel_count + 2 || bones.is_empty(), ERR_FILE_CORRUPT);
					Vector<int> channel;
					channel.append(bones[bones.size() - 1]);
					for (int i = 0; i < channel_count; i++) {
						String channel_name = s[i + 2].strip_edges();
						//print_verbose(channel_name);
						if (channel_name.casecmp_to("Xposition") == 0) {
							channel.append(BVH_X_POSITION);
						} else if (channel_name.casecmp_to("Yposition") == 0) {
							channel.append(BVH_Y_POSITION);
						} else if (channel_name.casecmp_to("Zposition") == 0) {
							channel.append(BVH_Z_POSITION);
						} else if (channel_name.casecmp_to("Xrotation") == 0) {
							channel.append(BVH_X_ROTATION);
						} else if (channel_name.casecmp_to("Yrotation") == 0) {
							channel.append(BVH_Y_ROTATION);
						} else if (channel_name.casecmp_to("Zrotation") == 0) {
							channel.append(BVH_Z_ROTATION);
						} else if (channel_name.casecmp_to("Wrotation") == 0) {
							channel.append(BVH_W_ROTATION);
						} else {
							channel_name.clear();
						}
						ERR_FAIL_COND_V(channel_name.is_empty(), ERR_FILE_CORRUPT);
					}
					ERR_FAIL_COND_V(channel.size() < 2, ERR_FILE_CORRUPT);
					if (channels.is_empty()) {
						channels.append(channel);
					} else if (channels[channels.size() - 1].size() < 2) {
						channels.remove_at(channels.size() - 1);
						channels.append(channel);
					}
				} else if (s[0].casecmp_to("JOINT") == 0) {
					ERR_FAIL_COND_V(r_skeletons.is_empty() || bones.is_empty(), ERR_FILE_CORRUPT);
					int parent = bones[bones.size() - 1];
					String bone_name = s[1];
					if (bone_names.has(bone_name)) {
						bone_names[bone_name] += 1;
						bone_name += String::num_int64(bone_names[bone_name]);
					} else {
						bone_names[bone_name] = 1;
					}
					bones.append(r_skeletons.back()->get()->add_bone(bone_name));
					channels.append(Vector<int>({bones[bones.size() - 1]}));
					r_skeletons.back()->get()->set_bone_parent(bones[bones.size() - 1], parent);
				}
			} else {
				if (l.casecmp_to("{") == 0) {
					// TODO
				} else if (l.casecmp_to("}") == 0) {
					ERR_FAIL_COND_V(r_skeletons.is_empty() || bones.is_empty() || offsets.is_empty() || channels.is_empty(), ERR_FILE_CORRUPT);
					int bone = bones[bones.size() - 1];
					Vector3 offset = offsets[offsets.size() - 1];
					Vector<int> channel = channels[channels.size() - 1];
					bones.remove_at(bones.size() - 1);
					offsets.remove_at(offsets.size() - 1);
					//channels.remove_at(channels.size() - 1);
					r_skeletons.back()->get()->set_bone_rest(bone, Transform3D(Basis(), offset));
				}
			}
		}
	}

	//print_verbose(String::num_int64(frames.size())+" "+String::num_int64(frame_count));
	if (animation_library.is_valid() && animation.is_valid() && r_animation != nullptr && frames.size() == frame_count) {
		if (!channels.is_empty() && !r_skeletons.is_empty()) {
			tracks.clear();
			for (int i = 0; i < channels.size(); i++) {
				//print_verbose(channels[i]);
				if (channels[i].size() < 2) {
					continue;
				}
				tracks[channels[i][0]] = animation->add_track(Animation::TrackType::TYPE_POSITION_3D);
				tracks[channels.size() + channels[i][0]] = animation->add_track(Animation::TrackType::TYPE_ROTATION_3D);
			}
			for (int i = 0; i < frame_count; i++) {
				int bone_index = 0;
				int channel_index = -1;
				String frame = frames[i];
				Vector<String> s;
				if (frame.contains(" ")) {
					s = frame.split(" ");
				} else {
					s = frame.split("\t");
				}
				for (int j = 0; j < s.size(); j++) {
					channel_index++;
					if (channel_index >= channels[bone_index].size() - 1 || channels[bone_index].size() < 2) {
						do {
							bone_index++;
							if (bone_index >= channels.size()) {
								break;
							}
						} while (channels[bone_index].size() < 2);
						channel_index = 0;
					}
					if (bone_index < 0 || bone_index >= channels.size()) {
						break;
					}
					Vector3 position;
					Quaternion rotation;
					String bone_name = r_skeletons.back()->get()->get_bone_name(channels[bone_index][0]);
					int position_track = tracks[channels[bone_index][0]];
					int rotation_track = tracks[channels.size() + channels[bone_index][0]];
					int insertion = -1;
					switch (channels[bone_index][channel_index + 1]) {
						case BVH_X_POSITION:
						case BVH_Y_POSITION:
						case BVH_Z_POSITION:
							if (channel_index + 3 < channels[bone_index].size()) {
								for (int k = j; k < j + 3 && k < s.size(); k++) {
									switch (channels[bone_index][channel_index + 1 + (k - j)]) {
										case BVH_X_POSITION:
											position.x = s[k].to_float();
											break;
										case BVH_Y_POSITION:
											position.y = s[k].to_float();
											break;
										case BVH_Z_POSITION:
											position.z = s[k].to_float();
											break;
									}
								}
								animation->track_set_path(position_track, "" + r_skeletons.back()->get()->get_name() + ":" + bone_name);
								insertion = animation->track_insert_key(position_track, frame_time * static_cast<double>(i), position);
								j += 2;
								channel_index += 2;
							} else {
								//print_verbose(String::num_int64(channel_index) + " " + String::num_int64(channels[bone_index].size()));
							}
							break;
						case BVH_X_ROTATION:
						case BVH_Y_ROTATION:
						case BVH_Z_ROTATION:
						case BVH_W_ROTATION:
							if (channel_index + 4 < channels[bone_index].size()) {
								for (int k = j; k < j + 4 && k < s.size(); k++) {
									switch (channels[bone_index][channel_index + 1 + (k - j)]) {
										case BVH_X_ROTATION:
											rotation.x = s[k].to_float();
											break;
										case BVH_Y_ROTATION:
											rotation.y = s[k].to_float();
											break;
										case BVH_Z_ROTATION:
											rotation.z = s[k].to_float();
											break;
										case BVH_W_ROTATION:
											rotation.w = s[k].to_float();
											break;
									}
								}
								animation->track_set_path(rotation_track, "" + r_skeletons.back()->get()->get_name() + ":" + bone_name);
								insertion = animation->track_insert_key(rotation_track, frame_time * static_cast<double>(i), rotation.normalized());
								j += 3;
								channel_index += 3;
							} else {
								//print_verbose(String::num_int64(channel_index) + " " + String::num_int64(channels[bone_index].size()));
							}
							break;
						default:
							//print_verbose(String::num_int64(position_track) + " " + String::num_int64(rotation_track) + bone_name + " @ " + String::num_int64(channel_index));
							break;
					}
					if (insertion < 0 && OS::get_singleton()->has_feature("debug")) {
						//print_verbose(String::num_int64(insertion) + " BVH track insertion");
					}
				}
			}
		}
		if (!animation->get_name().is_empty()) {
			animation_library_name = animation->get_name();
		}
		animation->set_step(frame_time);
		animation_library->add_animation(animation_library_name, animation);
		(*r_animation)->set_assigned_animation(animation_library->get_name() + "/" + animation->get_name());
		List<StringName> animations;
		animation_library->get_animation_list(&animations);
		for (int i = 0; i < animations.size(); i++) {
			Vector<int> duds;
			animation = animation_library->get_animation(animations.get(i));
			for (int j = 0; j < animation->get_track_count(); j++) {
				if (animation->track_get_path(j).is_empty()) {
					duds.append(j);
				}
			}
			for (int j = 0; j < duds.size(); j++) {
				for (int k = j + 1; k < duds.size(); k++) {
					duds.set(k, duds[k] - 1);
				}
				animation->remove_track(duds[j]);
			}
		}
	}

	return OK;
}

static Error _parse_qbo(const String &p_path, List<Ref<ImporterMesh>> &r_meshes, bool p_single_mesh, bool p_generate_tangents, bool p_optimize, Vector3 p_scale_mesh, Vector3 p_offset_mesh, bool p_disable_compression, List<String> *r_missing_deps, List<Skeleton3D *> &r_skeletons, AnimationPlayer **r_animation) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, vformat("Couldn't open QBO file '%s', it may not exist or not be readable.", p_path));

	Ref<ImporterMesh> mesh;
	mesh.instantiate();

	bool generate_tangents = p_generate_tangents;
	Vector3 scale_mesh = p_scale_mesh;
	Vector3 offset_mesh = p_offset_mesh;

	HashMap<uint32_t, HashMap<String, float>> weights;
	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Vector2> uvs;
	Vector<Color> colors;
	const String default_name = "QBO";
	String name = default_name;

	HashMap<String, HashMap<String, Ref<StandardMaterial3D>>> material_map;

	Ref<SurfaceTool> surf_tool = memnew(SurfaceTool);
	surf_tool->begin(Mesh::PRIMITIVE_TRIANGLES);

	String current_material_library;
	String current_material;
	String current_group;
	uint32_t smooth_group = 0;
	bool smoothing = true;
	const uint32_t no_smoothing_smooth_group = (uint32_t)-1;

	Error err = _parse_motion(f, r_skeletons, r_animation);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Couldn't parse QBO file '%s', it may be corrupt.", p_path));
	if (r_animation != nullptr) {
		List<StringName> animations;
		(*r_animation)->get_animation_list(&animations);
		for (int i = 0; i < animations.size(); ++i) {
			print_verbose(animations.get(i));
		}
	}

	while (true) {
		String l = f->get_line().strip_edges();
		while (l.length() && l[l.length() - 1] == '\\') {
			String add = f->get_line().strip_edges();
			l += add;
			if (add.is_empty()) {
				break;
			}
		}

		if (l.begins_with("v ")) {
			//vertex
			Vector<String> v = l.split(" ", false);
			ERR_FAIL_COND_V(v.size() < 4, ERR_FILE_CORRUPT);
			Vector3 vtx;
			vtx.x = v[1].to_float() * scale_mesh.x + offset_mesh.x;
			vtx.y = v[2].to_float() * scale_mesh.y + offset_mesh.y;
			vtx.z = v[3].to_float() * scale_mesh.z + offset_mesh.z;
			vertices.push_back(vtx);
			//vertex color
			if (v.size() >= 7) {
				while (colors.size() < vertices.size() - 1) {
					colors.push_back(Color(1.0, 1.0, 1.0));
				}
				Color c;
				c.r = v[4].to_float();
				c.g = v[5].to_float();
				c.b = v[6].to_float();
				colors.push_back(c);
			} else if (!colors.is_empty()) {
				colors.push_back(Color(1.0, 1.0, 1.0));
			}
		} else if (l.begins_with("vt ")) {
			//uv
			Vector<String> v = l.split(" ", false);
			ERR_FAIL_COND_V(v.size() < 3, ERR_FILE_CORRUPT);
			Vector2 uv;
			uv.x = v[1].to_float();
			uv.y = 1.0 - v[2].to_float();
			uvs.push_back(uv);
		} else if (l.begins_with("vn ")) {
			//normal
			Vector<String> v = l.split(" ", false);
			ERR_FAIL_COND_V(v.size() < 4, ERR_FILE_CORRUPT);
			Vector3 nrm;
			nrm.x = v[1].to_float();
			nrm.y = v[2].to_float();
			nrm.z = v[3].to_float();
			normals.push_back(nrm);
		} else if (l.begins_with("vw ")) {
			//weight ( https://github.com/tinyobjloader/tinyobjloader/blob/v2.0.0rc13/tiny_obj_loader.h#L2696 )
			Vector<String> v = l.split(" ", false);
			ERR_FAIL_COND_V(r_skeletons.is_empty() || v.size() < 4 || v.size() % 2 != 0, ERR_FILE_CORRUPT);
			int idx = v[1].to_int() - 1;
			ERR_FAIL_COND_V(idx < 0, ERR_FILE_CORRUPT);
			if (!weights.has(idx)) {
				weights[idx] = HashMap<String, float>();
			}
			for (int i = 2; i < v.size() - 1; i += 2) {
				String bone = v[i];
				float weight = v[i + 1].to_float();
				weights[idx][bone] = weight;
				for (int j = 0; j < r_skeletons.size(); j++) {
					if (r_skeletons.get(j)->find_bone(bone) > -1) {
						bone.clear();
						break;
					}
				}
				ERR_FAIL_COND_V(!bone.is_empty(), ERR_FILE_CORRUPT);
				if (weights[idx].size() > 4) {
					surf_tool->set_skin_weight_count(SurfaceTool::SkinWeightCount::SKIN_8_WEIGHTS);
				}
			}
		} else if (l.begins_with("f ")) {
			//vertex

			Vector<String> v = l.split(" ", false);
			ERR_FAIL_COND_V(v.size() < 4, ERR_FILE_CORRUPT);

			//not very fast, could be sped up

			Vector<String> face[3];
			face[0] = v[1].split("/");
			face[1] = v[2].split("/");
			ERR_FAIL_COND_V(face[0].is_empty(), ERR_FILE_CORRUPT);

			ERR_FAIL_COND_V(face[0].size() != face[1].size(), ERR_FILE_CORRUPT);
			for (int i = 2; i < v.size() - 1; i++) {
				face[2] = v[i + 1].split("/");

				ERR_FAIL_COND_V(face[0].size() != face[2].size(), ERR_FILE_CORRUPT);
				for (int j = 0; j < 3; j++) {
					int idx = j;

					if (idx < 2) {
						idx = 1 ^ idx;
					}

					if (face[idx].size() == 3) {
						int norm = face[idx][2].to_int() - 1;
						if (norm < 0) {
							norm += normals.size() + 1;
						}
						ERR_FAIL_INDEX_V(norm, normals.size(), ERR_FILE_CORRUPT);
						surf_tool->set_normal(normals[norm]);
						if (generate_tangents && uvs.is_empty()) {
							// We can't generate tangents without UVs, so create dummy tangents.
							Vector3 tan = Vector3(normals[norm].z, -normals[norm].x, normals[norm].y).cross(normals[norm].normalized()).normalized();
							surf_tool->set_tangent(Plane(tan.x, tan.y, tan.z, 1.0));
						}
					} else {
						// No normals, use a dummy tangent since normals and tangents will be generated.
						if (generate_tangents && uvs.is_empty()) {
							// We can't generate tangents without UVs, so create dummy tangents.
							surf_tool->set_tangent(Plane(1.0, 0.0, 0.0, 1.0));
						}
					}

					if (face[idx].size() >= 2 && !face[idx][1].is_empty()) {
						int uv = face[idx][1].to_int() - 1;
						if (uv < 0) {
							uv += uvs.size() + 1;
						}
						ERR_FAIL_INDEX_V(uv, uvs.size(), ERR_FILE_CORRUPT);
						surf_tool->set_uv(uvs[uv]);
					}

					int vtx = face[idx][0].to_int() - 1;
					if (vtx < 0) {
						vtx += vertices.size() + 1;
					}
					ERR_FAIL_INDEX_V(vtx, vertices.size(), ERR_FILE_CORRUPT);

					Vector3 vertex = vertices[vtx];
					if (!colors.is_empty()) {
						surf_tool->set_color(colors[vtx]);
					}
					if (!weights.is_empty() && weights.has(vtx)) {
						Vector<int> bones;
						Vector<float> weight;
						for (HashMap<String, float>::Iterator itr = weights[vtx].begin(); itr; ++itr) {
							if (itr->key.is_numeric()) {
								bones.append(itr->key.to_int());
							} else if (!r_skeletons.is_empty()) {
								bones.append(r_skeletons.back()->get()->find_bone(itr->key));
							} else {
								continue;
							}
							if (bones[bones.size() - 1] < 0) {
								bones.remove_at(bones.size() - 1);
								continue;
							}
							weight.append(itr->value);
						}
						if (!bones.is_empty()) {
							surf_tool->set_bones(bones);
							surf_tool->set_weights(weight);
						}
					}
					surf_tool->set_smooth_group(smoothing ? smooth_group : no_smoothing_smooth_group);
					surf_tool->add_vertex(vertex);
				}

				face[1] = face[2];
			}
		} else if (l.begins_with("s ")) { //smoothing
			String what = l.substr(2, l.length()).strip_edges();
			bool do_smooth;
			if (what == "off") {
				do_smooth = false;
			} else {
				do_smooth = true;
			}
			if (do_smooth != smoothing) {
				smoothing = do_smooth;
				if (smoothing) {
					smooth_group++;
				}
			}
		} else if (/*l.begins_with("g ") ||*/ l.begins_with("usemtl ") || (l.begins_with("o ") || f->eof_reached())) { //commit group to mesh
			uint64_t mesh_flags = RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES;

			if (p_disable_compression) {
				mesh_flags = 0;
			} else {
				bool is_mesh_2d = true;

				// Disable compression if all z equals 0 (the mesh is 2D).
				for (int i = 0; i < vertices.size(); i++) {
					if (!Math::is_zero_approx(vertices[i].z)) {
						is_mesh_2d = false;
						break;
					}
				}

				if (is_mesh_2d) {
					mesh_flags = 0;
				}
			}

			//groups are too annoying
			if (surf_tool->get_vertex_array().size()) {
				//another group going on, commit it
				if (normals.size() == 0) {
					surf_tool->generate_normals();
				}

				if (generate_tangents && uvs.size()) {
					surf_tool->generate_tangents();
				}

				surf_tool->index();

				print_verbose("OBJ: Current material library " + current_material_library + " has " + itos(material_map.has(current_material_library)));
				print_verbose("OBJ: Current material " + current_material + " has " + itos(material_map.has(current_material_library) && material_map[current_material_library].has(current_material)));
				Ref<StandardMaterial3D> material;
				if (material_map.has(current_material_library) && material_map[current_material_library].has(current_material)) {
					material = material_map[current_material_library][current_material];
					if (!colors.is_empty()) {
						material->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
					}
					surf_tool->set_material(material);
				}

				Array array = surf_tool->commit_to_arrays();

				if (mesh_flags & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES && generate_tangents) {
					// Compression is enabled, so let's validate that the normals and tangents are correct.
					Vector<Vector3> norms = array[Mesh::ARRAY_NORMAL];
					Vector<float> tangents = array[Mesh::ARRAY_TANGENT];
					for (int vert = 0; vert < norms.size(); vert++) {
						Vector3 tan = Vector3(tangents[vert * 4 + 0], tangents[vert * 4 + 1], tangents[vert * 4 + 2]);
						if (abs(tan.dot(norms[vert])) > 0.0001) {
							// Tangent is not perpendicular to the normal, so we can't use compression.
							mesh_flags &= ~RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES;
						}
					}
				}

				mesh->add_surface(Mesh::PRIMITIVE_TRIANGLES, array, TypedArray<Array>(), Dictionary(), material, name, mesh_flags);
				print_verbose("OBJ: Added surface :" + mesh->get_surface_name(mesh->get_surface_count() - 1));

				if (!current_material.is_empty()) {
					if (mesh->get_surface_count() >= 1) {
						mesh->set_surface_name(mesh->get_surface_count() - 1, current_material.get_basename());
					}
				} else if (!current_group.is_empty()) {
					if (mesh->get_surface_count() >= 1) {
						mesh->set_surface_name(mesh->get_surface_count() - 1, current_group);
					}
				}

				surf_tool->clear();
				surf_tool->begin(Mesh::PRIMITIVE_TRIANGLES);
			}

			if (l.begins_with("o ") || f->eof_reached()) {
				if (!p_single_mesh) {
					if (mesh->get_surface_count() > 0) {
						mesh->set_name(name);
						r_meshes.push_back(mesh);
						mesh.instantiate();
					}
					name = default_name;
					current_group = "";
					current_material = "";
				}
			}

			if (f->eof_reached()) {
				break;
			}

			if (l.begins_with("o ")) {
				name = l.substr(2, l.length()).strip_edges();
			}

			if (l.begins_with("usemtl ")) {
				current_material = l.replace("usemtl", "").strip_edges();
			}

			if (l.begins_with("g ")) {
				current_group = l.substr(2, l.length()).strip_edges();
			}

		} else if (l.begins_with("mtllib ")) { //parse material

			current_material_library = l.replace("mtllib", "").strip_edges();
			if (!material_map.has(current_material_library)) {
				HashMap<String, Ref<StandardMaterial3D>> lib;
				String lib_path = current_material_library;
				if (lib_path.is_relative_path()) {
					lib_path = p_path.get_base_dir().path_join(current_material_library);
				}
				err = OBJ::_parse_material_library(lib_path, lib, r_missing_deps);
				if (err == OK) {
					material_map[current_material_library] = lib;
				}
			}
		}
	}

	if (p_single_mesh && mesh->get_surface_count() > 0) {
		r_meshes.push_back(mesh);
	}

	return OK;
}

Node *EditorQBOImporter::import_scene(const String &p_path, uint32_t p_flags, const HashMap<StringName, Variant> &p_options, List<String> *r_missing_deps, Error *r_err) {
	List<Ref<ImporterMesh>> meshes;
	List<Skeleton3D *> skeletons;
	AnimationPlayer *animation = nullptr;

	Error err = _parse_qbo(p_path, meshes, false, p_flags & IMPORT_GENERATE_TANGENT_ARRAYS, false, Vector3(1, 1, 1), Vector3(0, 0, 0), p_flags & IMPORT_FORCE_DISABLE_MESH_COMPRESSION, r_missing_deps, skeletons, (p_flags & IMPORT_ANIMATION) ? &animation : nullptr);

	if (err != OK) {
		if (r_err) {
			*r_err = err;
		}
		return nullptr;
	}

	Node3D *scene = memnew(Node3D);

	for (Ref<ImporterMesh> m : meshes) {
		ImporterMeshInstance3D *mi = memnew(ImporterMeshInstance3D);
		mi->set_mesh(m);
		mi->set_name(m->get_name());
		if (p_flags & IMPORT_ANIMATION) {
			for (Skeleton3D *s : skeletons) {
				Ref<Skin> skin = s->create_skin_from_rest_transforms();
				if (!skin.is_valid()) {
					break;
				}
				scene->add_child(s, true);
				s->set_owner(scene);
				s->add_child(mi, true);
				mi->set_owner(s);
				mi->set_skin(skin);
				mi->set_skeleton_path(mi->get_path_to(s));
				mi->set_transform(Transform3D());
				break;
			}
		}
		if (!mi->get_skin().is_valid()) {
			scene->add_child(mi, true);
			mi->set_owner(scene);
		}
	}
	if (animation != nullptr) {
		scene->add_child(animation, true);
		animation->set_owner(scene);
	}

	if (r_err) {
		*r_err = OK;
	}

	return scene;
}

void EditorQBOImporter::get_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("qbo");
}

EditorQBOImporter::EditorQBOImporter() {
}

////////////////////////////////////////////////////

#if 0
String ResourceImporterQBO::get_importer_name() const {
	return "quaternion_bvh_obj";
}

String ResourceImporterQBO::get_visible_name() const {
	return "QBO as Scene";
}

void ResourceImporterQBO::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("qbo");
}

String ResourceImporterQBO::get_save_extension() const {
	return "scn";
}

String ResourceImporterQBO::get_resource_type() const {
	return "Scene";
}

int ResourceImporterQBO::get_format_version() const {
	return 1;
}

int ResourceImporterQBO::get_preset_count() const {
	return 0;
}

String ResourceImporterQBO::get_preset_name(int p_idx) const {
	return "";
}

void ResourceImporterQBO::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "generate_tangents"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR3, "scale_mesh"), Vector3(1, 1, 1)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR3, "offset_mesh"), Vector3(0, 0, 0)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "optimize_mesh"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force_disable_mesh_compression"), false));
}

bool ResourceImporterQBO::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error ResourceImporterQBO::import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	List<Ref<ImporterMesh>> meshes;
	List<Skeleton3D *> skeletons;
	AnimationPlayer *animation = nullptr;

	Error err = _parse_qbo(p_source_file, meshes, true, p_options["generate_tangents"], p_options["optimize_mesh"], p_options["scale_mesh"], p_options["offset_mesh"], p_options["force_disable_mesh_compression"], nullptr, skeletons, &animation);

	ERR_FAIL_COND_V(err != OK, err);
	ERR_FAIL_COND_V(meshes.size() != 1, ERR_BUG);

	String save_path = p_save_path + ".scn";
	Ref<PackedScene> sdata;
	Node3D *scene = memnew(Node3D);

	for (Ref<ImporterMesh> m : meshes) {
		ImporterMeshInstance3D *mi = memnew(ImporterMeshInstance3D);
		mi->set_mesh(m);
		mi->set_name(m->get_name());
		scene->add_child(mi, true);
		mi->set_owner(scene);
		for (Skeleton3D *s : skeletons) {
			Ref<Skin> skin = s->create_skin_from_rest_transforms();
			scene->add_child(s, true);
			s->set_owner(scene);
			mi->get_parent()->remove_child(mi);
			mi->set_owner(nullptr);
			s->add_child(mi, true);
			mi->set_owner(s);
			mi->set_skin(skin);
			mi->set_skeleton_path(mi->get_path_to(s));
			mi->set_transform(Transform3D());
		}
	}
	if (animation != nullptr) {
		scene->add_child(animation, true);
		animation->set_owner(scene);
	}

	sdata.instantiate();
	err = sdata->pack(scene);

	ERR_FAIL_COND_V_MSG(err != OK, err, "Couldn't save QBO scene to file '" + save_path + "'. Likely dependencies (instances or inheritance) couldn't be satisfied.");

	err = ResourceSaver::save(sdata, save_path);

	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save QBO Scene to file '" + save_path + "'.");

	r_gen_files->push_back(save_path);

	memdelete(scene);

	return OK;
}

ResourceImporterQBO::ResourceImporterQBO() {
}
#endif
