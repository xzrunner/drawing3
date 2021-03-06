#include "draft3/FacePushPullState.h"

#include <ee0/color_config.h>
#include <ee0/SubjectMgr.h>
#include <ee0/MessageID.h>

#include <SM_RayIntersect.h>
#include <halfedge/Utility.h>
#include <polymesh3/Polytope.h>
#include <tessellation/Painter.h>
#include <unirender/RenderState.h>
#include <painting2/OrthoCamera.h>
#include <painting2/RenderSystem.h>
#include <painting3/Viewport.h>
#include <painting3/PerspCam.h>
#include <model/BrushModel.h>
#include <model/Model.h>
#include <model/BrushBuilder.h>

namespace draft3
{

FacePushPullState::FacePushPullState(const std::shared_ptr<pt0::Camera>& camera,
	                                 const pt3::Viewport& vp,
	                                 const ee0::SubjectMgrPtr& sub_mgr,
	                                 const MeshPointQuery::Selected& selected,
	                                 std::function<void()> update_cb)
	: ee0::EditOpState(camera)
	, m_vp(vp)
	, m_sub_mgr(sub_mgr)
	, m_selected(selected)
	, m_update_cb(update_cb)
	, m_cam2d(std::make_shared<pt2::OrthoCamera>())
{
	m_cam2d->OnSize(m_vp.Width(), m_vp.Height());

	m_last_pos3d.MakeInvalid();
}

bool FacePushPullState::OnMousePress(int x, int y)
{
	if (!m_selected.face) {
		return false;
	}

	if (m_camera->TypeID() == pt0::GetCamTypeID<pt3::PerspCam>())
	{
		auto p_cam = std::dynamic_pointer_cast<pt3::PerspCam>(m_camera);

		m_first_pos2 = m_cam2d->TransPosScreenToProject(x, y,
			static_cast<int>(m_vp.Width()), static_cast<int>(m_vp.Height()));

		sm::vec3 ray_dir = m_vp.TransPos3ScreenToDir(sm::vec2((float)x, (float)y), *p_cam);
		sm::Ray ray(p_cam->GetPos(), ray_dir);
		sm::vec3 intersect;
		sm::Plane plane;
        he::Utility::LoopToPlane(*m_selected.face, plane);
		bool crossed = false;
		if (crossed = sm::ray_plane_intersect(ray, plane, &intersect)) {
			m_last_pos3d = intersect;
			m_move_path3d.origin = intersect;
		}
		assert(crossed);
		m_move_path3d.dir = plane.normal;

		m_cam_mat = m_camera->GetProjectionMat() * m_camera->GetViewMat();
		auto next_pos2 = m_vp.TransPosProj3ToProj2(m_move_path3d.origin + m_move_path3d.dir, m_cam_mat);
		m_first_dir2 = (next_pos2 - m_first_pos2).Normalized();
	}

	return false;
}

bool FacePushPullState::OnMouseDrag(int x, int y)
{
	if (m_camera->TypeID() == pt0::GetCamTypeID<pt3::PerspCam>())
	{
		auto p_cam = std::dynamic_pointer_cast<pt3::PerspCam>(m_camera);

		auto curr_pos2 = m_cam2d->TransPosScreenToProject(x, y,
			static_cast<int>(m_vp.Width()), static_cast<int>(m_vp.Height()));
		auto fixed_curr_pos2 = m_first_pos2 + m_first_dir2 * ((curr_pos2 - m_first_pos2).Dot(m_first_dir2));
		auto screen_fixed_curr_pos2 = m_cam2d->TransPosProjectToScreen(fixed_curr_pos2,
			static_cast<int>(m_vp.Width()), static_cast<int>(m_vp.Height()));

		auto move_path3d = m_move_path3d;
		if ((curr_pos2 - m_first_pos2).Dot(m_first_dir2) < 0) {
			move_path3d.dir = -move_path3d.dir;
		}

		sm::vec3 ray_dir = m_vp.TransPos3ScreenToDir(screen_fixed_curr_pos2, *p_cam);
		sm::Ray ray(p_cam->GetPos(), ray_dir);
		sm::vec3 cross;
		if (sm::ray_ray_intersect(ray, move_path3d, &cross)) {
			if (m_last_pos3d.IsValid()) {
				TranslateFace(cross - m_last_pos3d);
			}
			m_last_pos3d = cross;
		} else {
			m_last_pos3d.MakeInvalid();
		}

		m_sub_mgr->NotifyObservers(ee0::MSG_SET_CANVAS_DIRTY);

		// update m_selected border
		m_update_cb();

		return true;
	}
	else
	{
		return false;
	}
}

bool FacePushPullState::OnDraw(const ur::Device& dev, ur::Context& ctx) const
{
	// debug draw
	if (m_last_pos3d.IsValid())
	{
		auto cam_mat = m_camera->GetProjectionMat() * m_camera->GetViewMat();
		tess::Painter pt;
		pt.AddLine3D(m_move_path3d.origin, m_last_pos3d, [&](const sm::vec3& pos3)->sm::vec2 {
			return m_vp.TransPosProj3ToProj2(pos3, cam_mat);
		}, 0xffffffff);

        ur::RenderState rs;
		pt2::RenderSystem::DrawPainter(dev, ctx, rs, pt);
	}

	return false;
}

void FacePushPullState::TranslateFace(const sm::vec3& offset)
{
	if (!m_selected.face) {
		return;
	}

	// polymesh3 brush data
	assert(m_selected.model->ext && m_selected.model->ext->Type() == model::EXT_BRUSH);
	auto brush_model = static_cast<model::BrushModel*>(m_selected.model->ext.get());
	auto& brushes = brush_model->GetBrushes();
	assert(m_selected.brush_idx >= 0 && m_selected.brush_idx < static_cast<int>(brushes.size()));
    auto& brush = brushes[m_selected.brush_idx];
	auto& faces = brush.impl->Faces();
	assert(m_selected.face_idx < static_cast<int>(faces.size()));
	auto& face = faces[m_selected.face_idx];
	for (auto& vert : face->border) {
        brush.impl->Points()[vert]->pos += offset;
	}

	// halfedge geo
    auto edge = m_selected.face->edge;
    do {
        edge->vert->position += offset;
        edge = edge->next;
    } while (edge != m_selected.face->edge);
	m_selected.poly->UpdateAABB();

	// update model aabb
	sm::cube model_aabb;
	for (auto& brush : brushes) {
		model_aabb.Combine(brush.impl->GetTopoPoly()->GetAABB());
	}
	m_selected.model->aabb = model_aabb;

	// update vbo
	model::BrushBuilder::UpdateVBO(*m_selected.model, brush);
}

}