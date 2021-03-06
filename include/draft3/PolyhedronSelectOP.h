#pragma once

#include "draft3/MeshPointQuery.h"

#include <ee0/EditOP.h>
#include <ee0/typedef.h>
#include <ee0/SelectionSet.h>
#include <ee0/GameObj.h>

namespace ee0 { class WxStagePage; }
namespace pt3 { class Viewport; }

namespace draft3
{

class PolyhedronSelectOP : public ee0::EditOP
{
public:
	PolyhedronSelectOP(const std::shared_ptr<pt0::Camera>& camera,
		ee0::WxStagePage& stage, const pt3::Viewport& vp);

	virtual bool OnKeyDown(int key_code) override;
	virtual bool OnKeyUp(int key_code) override;

	virtual bool OnMouseLeftDown(int x, int y) override;
	virtual bool OnMouseLeftUp(int x, int y) override;
	virtual bool OnMouseMove(int x, int y) override;
	virtual bool OnMouseDrag(int x, int y) override;

	virtual bool OnDraw(const ur::Device& dev, ur::Context& ctx) const override;

	auto& GetSelected() const { return m_selected; }
	void SetSelected(const n0::SceneNodePtr& node);

	void SetCanSelectNull(bool select_null) {
		m_select_null = select_null;
	}

	// for draw
	void UpdateCachedPolyBorder();

private:
	void SelectByPos(const sm::ivec2& pos, MeshPointQuery::Selected& selected);

	void ClearSelected();

private:
	ee0::WxStagePage&    m_stage;

	const pt3::Viewport& m_vp;
	ee0::SubjectMgrPtr   m_sub_mgr;

	const ee0::SelectionSet<ee0::GameObjWithPos>& m_selection;

	MeshPointQuery::Selected m_selected;

	// cache for draw
	std::vector<std::vector<sm::vec3>> m_selected_poly;
	std::vector<sm::vec3>              m_selected_face;

	sm::ivec2 m_first_pos;

	bool m_move_select = false;

	bool m_select_null = true;

}; // PolyhedronSelectOP

}