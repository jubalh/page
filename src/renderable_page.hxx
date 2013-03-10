/*
 * renderable_page.hxx
 *
 *  Created on: 27 déc. 2012
 *      Author: gschwind
 */

#ifndef RENDERABLE_PAGE_HXX_
#define RENDERABLE_PAGE_HXX_

#include "render_tree.hxx"

namespace page {


class renderable_page_t : public renderable_t {
	bool is_durty;

public:

	render_tree_t & render;

	std::set<split_t *> & splits;
	std::set<notebook_t *> & notebooks;
	std::set<viewport_t *> & viewports;

	renderable_page_t(render_tree_t & render, std::set<split_t *> & splits, std::set<notebook_t *> & notebooks, std::set<viewport_t *> & viewports);

	void mark_durty();
	void render_if_needed();

	virtual void repair1(cairo_t * cr, box_int_t const & area);
	virtual region_t<int> get_area();
	virtual void reconfigure(box_int_t const & area);
	virtual bool is_visible();

};

}


#endif /* RENDERABLE_PAGE_HXX_ */
