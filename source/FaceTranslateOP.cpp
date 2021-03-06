#include "draft3/FaceTranslateOP.h"

#include <polymesh3/Polytope.h>
#include <model/Model.h>

namespace draft3
{

FaceTranslateOP::FaceTranslateOP(const std::shared_ptr<pt0::Camera>& camera,
	                             const pt3::Viewport& vp,
	                             const ee0::SubjectMgrPtr& sub_mgr,
	                             const MeshPointQuery::Selected& selected,
	                             const ee0::SelectionSet<pm3::Polytope::FacePtr>& selection,
	                             std::function<void()> update_cb)
	: MeshTranslateBaseOP<pm3::Polytope::FacePtr>(camera, vp, sub_mgr, selected, selection, update_cb)
{
}

bool FaceTranslateOP::QueryByPos(const sm::vec2& pos, const pm3::Polytope::FacePtr& face,
	                             const sm::mat4& cam_mat) const
{
    auto brush = m_selected.GetBrush();
    if (!brush || !brush->impl) {
        return nullptr;
    }

	assert(!face->border.empty());
	sm::vec3 c3;
	for (auto& v : face->border) {
        c3 += brush->impl->Points()[v]->pos;
	}
	c3 /= static_cast<float>(face->border.size());
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
    auto brush = m_selected.GetBrush();
    if (!brush || !brush->impl) {
        return;
    }

	auto& faces = m_selected.poly->GetLoops();
	m_selection.Traverse([&](const pm3::Polytope::FacePtr& face)->bool
	{
		// update helfedge geo
		sm::vec3 c0;
		for (auto& v : face->border) {
			c0 += brush->impl->Points()[v]->pos;
		}
		c0 /= static_cast<float>(face->border.size());
        auto f = faces.Head();
        do {
			sm::vec3 c1;
			auto curr = f->edge;
			int count = 0;
			do {
				c1 += curr->vert->position;
				curr = curr->next;
				++count;
			} while (curr != f->edge);
			c1 /= static_cast<float>(count);
			auto d = c0 - c1;
			if (fabs(d.x) < SM_LARGE_EPSILON &&
				fabs(d.y) < SM_LARGE_EPSILON &&
				fabs(d.z) < SM_LARGE_EPSILON)
			{
				auto curr = f->edge;
				do {
					curr->vert->position += offset;
					curr = curr->next;
				} while (curr != f->edge);
				break;
			}

            f = f->linked_next;
        } while (f != faces.Head());

		// update polymesh3 brush
		for (auto& v : face->border) {
            brush->impl->Points()[v]->pos += offset;
		}

		return true;
	});

	// update helfedge geo
	m_selected.poly->UpdateAABB();

	// update model aabb
	sm::cube model_aabb;
	model_aabb.Combine(brush->impl->GetTopoPoly()->GetAABB());
	m_selected.model->aabb = model_aabb;

	// update vbo
	model::BrushBuilder::UpdateVBO(*m_selected.model, *brush);
}

}