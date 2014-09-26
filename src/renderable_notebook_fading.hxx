/*
 * renderable_surface.hxx
 *
 *  Created on: 23 sept. 2014
 *      Author: gschwind
 */

#ifndef RENDERABLE_NOTEBOOK_FADING_HXX_
#define RENDERABLE_NOTEBOOK_FADING_HXX_

#include <cairo/cairo.h>

#include "pixmap.hxx"
#include "utils.hxx"
#include "region.hxx"
#include "renderable.hxx"
#include "box.hxx"

namespace page {

using namespace std;

class renderable_notebook_fading_t : public renderable_t {

	double ratio;

	i_rect prev_pos;
	i_rect next_pos;

	i_rect tmp_pos;

	ptr<pixmap_t> prev_surf;
	ptr<pixmap_t> next_surf;

	cairo_surface_t * tmpa;

public:

	renderable_notebook_fading_t(ptr<pixmap_t> prev, ptr<pixmap_t> next, i_rect prev_pos, i_rect next_pos) : prev_surf(prev), next_surf(next), prev_pos(prev_pos), next_pos(next_pos), ratio(0.5) {
		tmpa = nullptr;
		tmp_pos.x = std::min(prev_pos.x, next_pos.x);
		tmp_pos.y = std::min(prev_pos.y, next_pos.y);
		tmp_pos.w = std::max(prev_pos.x+prev_pos.w, next_pos.x+next_pos.w) - tmp_pos.x;
		tmp_pos.h = std::max(prev_pos.y+prev_pos.h, next_pos.y+next_pos.h) - tmp_pos.y;

	}

	virtual ~renderable_notebook_fading_t() {
		if(tmpa != nullptr)
			cairo_surface_destroy(tmpa);

	}

	/**
	 * draw the area of a renderable to the destination surface
	 * @param cr the destination surface context
	 * @param area the area to redraw
	 **/
	virtual void render(cairo_t * cr, region const & area) {

		cairo_surface_t * target = cairo_get_target(cr);

		/**
		 * old frame is static, just create a back buffer once
		 * Hope that the background doesn't change
		 **/
		if (tmpa == nullptr) {
			tmpa = cairo_surface_create_similar(target, CAIRO_CONTENT_COLOR, tmp_pos.w, tmp_pos.h);
			cairo_t * cra = cairo_create(tmpa);
			cairo_set_operator(cra, CAIRO_OPERATOR_ATOP);
			cairo_set_source_surface(cra, target, - tmp_pos.x, - tmp_pos.y);
			cairo_paint(cra);
			if (prev_surf != nullptr) {
				i_rect cl{prev_pos.x - tmp_pos.x, prev_pos.y - tmp_pos.y, prev_pos.w, prev_pos.h};
				cairo_clip(cra, cl);
				cairo_set_source_rgba(cra, 1.0, 1.0, 1.0, 0.0);
				cairo_set_source_surface(cra, prev_surf->get_cairo_surface(), cl.x, cl.y);
				cairo_mask_surface(cra, prev_surf->get_cairo_surface(), cl.x, cl.y);
			}
			cairo_destroy(cra);
		}

		cairo_surface_t * tmpb = cairo_surface_create_similar(target, CAIRO_CONTENT_COLOR, tmp_pos.w, tmp_pos.h);

		{
			cairo_t * crb = cairo_create(tmpb);
			cairo_set_operator(crb, CAIRO_OPERATOR_OVER);
			cairo_set_source_surface(crb, target, - tmp_pos.x, - tmp_pos.y);
			cairo_paint(crb);

			if (next_surf != nullptr) {
				i_rect cl{next_pos.x - tmp_pos.x, next_pos.y - tmp_pos.y, next_pos.w, next_pos.h};
				cairo_clip(crb, cl);
				cairo_set_source_surface(crb, next_surf->get_cairo_surface(), cl.x, cl.y);
				cairo_mask_surface(crb, next_surf->get_cairo_surface(), cl.x, cl.y);
			}
			cairo_destroy(crb);
		}

		for (auto &cl : area) {
			cairo_save(cr);
			cairo_clip(cr, tmp_pos&cl);

			cairo_pattern_t * p1 = cairo_pattern_create_rgba(1.0, 1.0, 1.0, 1.0);
			cairo_set_source_surface(cr, tmpa, tmp_pos.x, tmp_pos.y);
			cairo_mask(cr, p1);
			cairo_pattern_destroy(p1);

			cairo_pattern_t * p0 = cairo_pattern_create_rgba(1.0, 1.0, 1.0, ratio);
			cairo_set_source_surface(cr, tmpb, tmp_pos.x, tmp_pos.y);
			cairo_mask(cr, p0);
			cairo_pattern_destroy(p0);
		}

		if(tmpb != nullptr)
			cairo_surface_destroy(tmpb);

	}

	/**
	 * Derived class must return opaque region for this object,
	 * If unknown it's safe to leave this empty.
	 **/
	virtual region get_opaque_region() {
		return region();
	}

	/**
	 * Derived class must return visible region,
	 * If unknow the whole screen can be returned, but draw will be called each time.
	 **/
	virtual region get_visible_region() {
		return region(tmp_pos);
	}

	virtual region get_damaged() {
		return region(tmp_pos);
	}

	void set_ratio(double x) {
		ratio = std::min(std::max(0.0, x), 1.0);
	}


};



}



#endif /* RENDERABLE_SURFACE_HXX_ */
