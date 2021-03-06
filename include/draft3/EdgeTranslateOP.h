#pragma once

#include "draft3/MeshTranslateBaseOP.h"

#include <polymesh3/Polytope.h>

namespace draft3
{

class EdgeTranslateOP : public MeshTranslateBaseOP<pm3::Polytope::EdgePtr>
{
public:
	EdgeTranslateOP(const std::shared_ptr<pt0::Camera>& camera, const pt3::Viewport& vp,
		const ee0::SubjectMgrPtr& sub_mgr, const MeshPointQuery::Selected& selected,
		const ee0::SelectionSet<pm3::Polytope::EdgePtr>& selection, std::function<void()> update_cb);

protected:
	virtual bool QueryByPos(const sm::vec2& pos, const pm3::Polytope::EdgePtr& edge,
		const sm::mat4& cam_mat) const override;
	virtual void TranslateSelected(const sm::vec3& offset) override;

}; // EdgeTranslateOP

}