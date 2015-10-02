/*
 * Copyright (c) 2015, Chaos Software Ltd
 *
 * V-Ray For Blender
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vfb_render_view.h"
#include "vfb_scene_exporter.h"
#include "vfb_utils_blender.h"
#include "vfb_utils_math.h"

#include "BLI_rect.h"
#include "BKE_camera.h"
#include "DNA_view3d_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"


using namespace VRayForBlender;


const std::string VRayForBlender::ViewParams::renderViewPluginName("renderView");
const std::string VRayForBlender::ViewParams::physicalCameraPluginName("cameraPhysical");
const std::string VRayForBlender::ViewParams::defaultCameraPluginName("cameraDefault");


static float GetLensShift(BL::Object &ob)
{
	float shift = 0.0f;

	BL::Constraint constraint(PointerRNA_NULL);
	if (ob.constraints.length()) {
		BL::Object::constraints_iterator cIt;
		for (ob.constraints.begin(cIt); cIt != ob.constraints.end(); ++cIt) {
			BL::Constraint cn(*cIt);

			if ((cn.type() == BL::Constraint::type_TRACK_TO)     ||
			    (cn.type() == BL::Constraint::type_DAMPED_TRACK) ||
			    (cn.type() == BL::Constraint::type_LOCKED_TRACK)) {
				constraint = cn;
				break;
			}
		}
	}

	if (constraint) {
		BL::ConstraintTarget ct(constraint);
		BL::Object target(ct.target());
		if (target) {
			const float z_shift = ob.matrix_world().data[14] - target.matrix_world().data[14];
			const float l = Blender::GetDistanceObOb(ob, target);
			shift = -1.0f * z_shift / l;
		}
	}
	else {
		const float rx  = ob.rotation_euler().data[0];
		const float lsx = rx - M_PI_2;
		if (fabs(lsx) > 0.0001f) {
			shift = tanf(lsx);
		}
		if (fabs(shift) > M_PI) {
			shift = 0.0f;
		}
	}

	return shift;
}


AttrPlugin DataExporter::exportRenderView(const ViewParams &viewParams)
{
	PluginDesc viewDesc(ViewParams::renderViewPluginName, "RenderView");
	viewDesc.add("transform", AttrTransformFromBlTransform(viewParams.renderView.tm));
	viewDesc.add("fov", viewParams.renderView.fov);
	viewDesc.add("clipping", (viewParams.renderView.use_clip_start || viewParams.renderView.use_clip_end));
	viewDesc.add("clipping_near", viewParams.renderView.clip_start);
	viewDesc.add("clipping_far", viewParams.renderView.clip_end);
	viewDesc.add("orthographic", viewParams.renderView.ortho);
	viewDesc.add("orthographicWidth", viewParams.renderView.ortho_width);

	return m_exporter->export_plugin(viewDesc);
}


AttrPlugin DataExporter::exportCameraDefault(ViewParams &viewParams)
{
	PluginDesc defCamDesc(ViewParams::defaultCameraPluginName, "CameraDefault");
	defCamDesc.add("orthographic", viewParams.renderView.ortho);

	return m_exporter->export_plugin(defCamDesc);
}


AttrPlugin DataExporter::exportCameraPhysical(ViewParams &viewParams)
{
	AttrPlugin plugin;

	BL::Camera cameraData(viewParams.cameraObject.data());
	if (cameraData) {
		PointerRNA vrayCamera = RNA_pointer_get(&cameraData.ptr, "vray");
		PointerRNA physicalCamera = RNA_pointer_get(&vrayCamera, "CameraPhysical");

		const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);

		float horizontal_offset = -cameraData.shift_x();
		float vertical_offset   = -cameraData.shift_y();
		if (aspect < 1.0f) {
			const float offset_fix = 1.0 / aspect;
			horizontal_offset *= offset_fix;
			vertical_offset   *= offset_fix;
		}

		const float lens_shift = RNA_boolean_get(&physicalCamera, "auto_lens_shift")
		                         ? GetLensShift(viewParams.cameraObject)
		                         : RNA_float_get(&physicalCamera, "lens_shift");

		float focus_distance = Blender::GetCameraDofDistance(viewParams.cameraObject);
		if (focus_distance < 0.001f) {
			focus_distance = 5.0f;
		}

		PluginDesc physCamDesc(ViewParams::physicalCameraPluginName, "CameraPhysical");
		physCamDesc.add("fov", viewParams.renderView.fov);
		physCamDesc.add("horizontal_offset", horizontal_offset);
		physCamDesc.add("vertical_offset",   vertical_offset);
		physCamDesc.add("lens_shift",        lens_shift);
		physCamDesc.add("focus_distance",    focus_distance);

		setAttrsFromPropGroupAuto(physCamDesc, &physicalCamera, "CameraPhysical");

		plugin = m_exporter->export_plugin(physCamDesc);
	}

	return plugin;
}


void SceneExporter::get_view_from_viewport(ViewParams &viewParams)
{
	if (m_region3d.view_perspective() == BL::RegionView3D::view_perspective_CAMERA) {
		BL::Object cameraObject = m_view3d.lock_camera_and_layers()
		                          ? m_scene.camera()
		                          : m_view3d.camera();

		if (!(cameraObject && cameraObject.data())) {
			PRINT_ERROR("View camera is not found!")
		}
		else {
			rctf view_border;

			// NOTE: Taken from source/blender/editors/space_view3d/view3d_draw.c:
			// static void view3d_camera_border(...) {...}
			//
			bool no_zoom = false;
			bool no_shift = false;

			Scene *scene = (Scene *)m_scene.ptr.data;
			const ARegion *ar = (const ARegion*)m_region.ptr.data;
			const View3D *v3d = (const View3D *)m_view3d.ptr.data;
			const RegionView3D *rv3d = (const RegionView3D *)m_region3d.ptr.data;

			CameraParams params;
			rctf rect_view, rect_camera;

			/* get viewport viewplane */
			BKE_camera_params_init(&params);
			BKE_camera_params_from_view3d(&params, v3d, rv3d);
			if (no_zoom)
				params.zoom = 1.0f;
			BKE_camera_params_compute_viewplane(&params, ar->winx, ar->winy, 1.0f, 1.0f);
			rect_view = params.viewplane;

			/* get camera viewplane */
			BKE_camera_params_init(&params);

			/* fallback for non camera objects */
			params.clipsta = v3d->near;
			params.clipend = v3d->far;
			BKE_camera_params_from_object(&params, v3d->camera);
			if (no_shift) {
				params.shiftx = 0.0f;
				params.shifty = 0.0f;
			}

			BKE_camera_params_compute_viewplane(&params, scene->r.xsch, scene->r.ysch, scene->r.xasp, scene->r.yasp);
			rect_camera = params.viewplane;

			/* get camera border within viewport */
			view_border.xmin = ((rect_camera.xmin - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) * ar->winx;
			view_border.xmax = ((rect_camera.xmax - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) * ar->winx;
			view_border.ymin = ((rect_camera.ymin - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) * ar->winy;
			view_border.ymax = ((rect_camera.ymax - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) * ar->winy;

			viewParams.renderSize.offs_x = view_border.xmin;
			viewParams.renderSize.offs_y = view_border.ymin;
			// NOTE: +2 to match camera border
			viewParams.renderSize.w = view_border.xmax - view_border.xmin + 2;
			viewParams.renderSize.h = view_border.ymax - view_border.ymin + 2;

			const float aspect = float(viewParams.renderSize.w) / float(viewParams.renderSize.h);

			BL::Camera cameraData(cameraObject.data());

			PointerRNA vrayCamera = RNA_pointer_get(&cameraData.ptr, "vray");

			PointerRNA renderView = RNA_pointer_get(&vrayCamera, "RenderView");

			viewParams.renderView.fov = RNA_boolean_get(&vrayCamera, "override_fov")
			                            ? RNA_float_get(&vrayCamera, "fov")
			                            : cameraData.angle();

			viewParams.renderView.ortho = (cameraData.type() == BL::Camera::type_ORTHO);
			viewParams.renderView.ortho_width = cameraData.ortho_scale();

			if (aspect < 1.0f) {
				viewParams.renderView.fov = 2.0f * atanf(tanf(viewParams.renderView.fov / 2.0f) * aspect);
				viewParams.renderView.ortho_width *= aspect;
			}

			viewParams.renderView.use_clip_start = RNA_boolean_get(&renderView, "clip_near");
			viewParams.renderView.use_clip_end   = RNA_boolean_get(&renderView, "clip_far");

			viewParams.renderView.clip_start = cameraData.clip_start();
			viewParams.renderView.clip_end   = cameraData.clip_end();

			viewParams.renderView.tm = cameraObject.matrix_world();

			viewParams.cameraObject = cameraObject;
		}
	}
	else {
		// XXX: Check if it's possible to use only m_view3d.camera()
		//
		BL::Object cameraObject = m_view3d.lock_camera_and_layers()
		                          ? m_scene.camera()
		                          : m_view3d.camera();

		BL::Camera cameraData(cameraObject.data());

		const float sensor_size = (cameraData.sensor_fit() == BL::Camera::sensor_fit_VERTICAL)
		                          ? cameraData.sensor_height()
		                          : cameraData.sensor_width();

		viewParams.renderSize.offs_x = 0;
		viewParams.renderSize.offs_y = 0;
		viewParams.renderSize.w = m_region.width();
		viewParams.renderSize.h = m_region.height();

		float lens = m_view3d.lens() / 2.f;

		viewParams.renderView.ortho = (m_region3d.view_perspective() == BL::RegionView3D::view_perspective_ORTHO);
		viewParams.renderView.ortho_width = m_region3d.view_distance() * sensor_size / lens;

		const ARegion *ar = (const ARegion*)m_region.ptr.data;
		float aspect = 0.f;

		if (viewParams.renderView.ortho) {
			aspect = viewParams.renderView.ortho_width / 2.0f;
		} else {
			lens /= 2.f;
			aspect = float(ar->winx) / float(ar->winy);
		}

		viewParams.renderView.fov = 2.0f * atanf((0.5f * sensor_size) / lens / aspect);

		if (viewParams.renderView.ortho) {
			viewParams.renderView.use_clip_start = false;
			viewParams.renderView.use_clip_end   = false;
		}
		else {
			viewParams.renderView.use_clip_start = true;
			viewParams.renderView.use_clip_end   = true;
			viewParams.renderView.clip_start = m_view3d.clip_start();
			viewParams.renderView.clip_end = m_view3d.clip_end();
		}

		viewParams.renderView.tm = Math::InvertTm(m_region3d.view_matrix());

		// No physical camera for non camera views
		viewParams.usePhysicalCamera = false;

		viewParams.cameraObject = cameraObject;
	}
}


int SceneExporter::is_physical_view(BL::Object &cameraObject)
{
	int is_physical = false;

	if (cameraObject) {
		BL::Camera cameraData(cameraObject.data());
		if (cameraData) {
			PointerRNA vrayCamera = RNA_pointer_get(&cameraData.ptr, "vray");
			PointerRNA physicalCamera = RNA_pointer_get(&vrayCamera, "CameraPhysical");

			is_physical = RNA_boolean_get(&physicalCamera, "use");
		}
	}

	return is_physical;
}


void SceneExporter::get_view_from_camera(ViewParams &viewParams, BL::Object &cameraObject)
{
	viewParams.renderView.tm = cameraObject.matrix_world();

	BL::Camera camera(cameraObject.data());
	if (camera) {
		viewParams.cameraObject = cameraObject;

		// TODO: Export view from scene camera for final render
	}
}


void SceneExporter::sync_view(int)
{
	ViewParams viewParams;

	if (m_view3d) {
		get_view_from_viewport(viewParams);
	}
	else {
		BL::Object sceneCamera(m_scene.camera());
		if (!sceneCamera) {
			PRINT_ERROR("Active scene camera is not set!")
		}
		else {
			get_view_from_camera(viewParams, sceneCamera);
		}
	}

	viewParams.usePhysicalCamera = is_physical_view(viewParams.cameraObject);

	if (m_viewParams.changedSize(viewParams)) {
		resize(viewParams.renderSize.w, viewParams.renderSize.h);
	}

	if (m_viewParams.changedViewPosition(viewParams)) {
		tag_redraw();
	}

	const bool needReset = m_viewParams.needReset(viewParams);
	if (needReset) {
		m_exporter->stop();
		m_exporter->remove_plugin(ViewParams::renderViewPluginName);
		m_exporter->remove_plugin(ViewParams::defaultCameraPluginName);
		m_exporter->remove_plugin(ViewParams::physicalCameraPluginName);
	}

	AttrPlugin renView;
	AttrPlugin physCam;
	AttrPlugin defCam;

	if (viewParams.usePhysicalCamera) {
		physCam = m_data_exporter.exportCameraPhysical(viewParams);
	}
	else {
		defCam = m_data_exporter.exportCameraDefault(viewParams);
	}

	if (needReset || m_viewParams.changedParams(viewParams)) {
		renView = m_data_exporter.exportRenderView(viewParams);
	}

	if (needReset) {
#if 0
		if (physCam) {
			m_exporter->set_camera_plugin(physCam.plugin);
		}
		else if (defCam) {
			m_exporter->set_camera_plugin(defCam.plugin);
		}
#else
		m_exporter->start();
#endif
	}

	// Store new params
	m_viewParams = viewParams;
}
