#include "drawing3/FaceTranslateOP.h"

#include <model/Model.h>

namespace dw3
{
namespace mesh
{

FaceTranslateOP::FaceTranslateOP(const std::shared_ptr<pt0::Camera>& camera,
	                             const pt3::Viewport& vp,
	                             const ee0::SubjectMgrPtr& sub_mgr,
	                             const MeshPointQuery::Selected& selected,
	                             const ee0::SelectionSet<pm3::BrushFacePtr>& selection,
	                             std::function<void()> update_cb)
	: MeshTranslateBaseOP<pm3::BrushFacePtr>(camera, vp, sub_mgr, selected, selection, update_cb)
{
}

bool FaceTranslateOP::QueryByPos(const sm::vec2& pos, const pm3::BrushFacePtr& face,
	                             const sm::mat4& cam_mat) const
{
	assert(!face->vertices.empty());
	sm::vec3 c3;
	for (auto& v : face->vertices) {
		c3 += v->pos;
	}
	c3 /= static_cast<float>(face->vertices.size());
	c3 *= model::BrushBuilder::VERTEX_SCALE;
	auto c2 = m_vp.TransPosProj3ToProj2(c3, cam_mat);
	if (sm::dis_pos_to_pos(c2, pos) < NODE_QUERY_RADIUS) {
		m_last_pos3 = c3;
		return true;
	} else {
		return false;
	}
}

void FaceTranslateOP::TranslateSelected(const sm::vec3& offset)
{
	auto& faces = m_selected.poly->GetFaces();
	auto _offset = offset / model::BrushBuilder::VERTEX_SCALE;
	m_selection.Traverse([&](const pm3::BrushFacePtr& face)->bool
	{
		// update helfedge geo
		sm::vec3 c0;
		for (auto& v : face->vertices) {
			c0 += v->pos;
		}
		c0 /= static_cast<float>(face->vertices.size());
		for (auto& f : faces)
		{
			sm::vec3 c1;
			auto curr = f->start_edge;
			int count = 0;
			do {
				c1 += curr->origin->position;
				curr = curr->next;
				++count;
			} while (curr != f->start_edge);
			c1 /= static_cast<float>(count);
			auto d = c0 * model::BrushBuilder::VERTEX_SCALE - c1;
			if (fabs(d.x) < SM_LARGE_EPSILON &&
				fabs(d.y) < SM_LARGE_EPSILON &&
				fabs(d.z) < SM_LARGE_EPSILON)
			{
				auto curr = f->start_edge;
				do {
					curr->origin->position += offset;
					curr = curr->next;
				} while (curr != f->start_edge);
				break;
			}
		}

		// update polymesh3 brush
		for (auto& v : face->vertices) {
			v->pos += _offset;
		}

		return true;
	});

	// update helfedge geo
	m_selected.poly->UpdateAABB();

	// update model aabb
    auto brush = m_selected.GetBrush();
    assert(brush);
	sm::cube model_aabb;
	model_aabb.Combine(brush->impl.geometry->GetAABB());
	m_selected.model->aabb = model_aabb;

	// update vbo
	model::BrushBuilder::UpdateVBO(*m_selected.model, brush->impl, brush->desc);
}

}
}