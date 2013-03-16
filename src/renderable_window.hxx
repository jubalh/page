/*
 * renderable_window.hxx
 *
 *  Created on: 15 févr. 2013
 *      Author: gschwind
 */

#ifndef RENDERABLE_WINDOW_HXX_
#define RENDERABLE_WINDOW_HXX_

#include <X11/Xlib.h>
#include <set>
#include <map>
#include "xconnection.hxx"
#include "renderable.hxx"
#include "region.hxx"
#include "icon.hxx"
#include "renderable_window.hxx"
#include "window.hxx"

namespace page {

class renderable_window_t: public renderable_t {

public:
	Display * dpy;
	Window window;
	Visual * visual;

	box_int_t position;

	bool _is_map;

	Damage damage;
	double opacity;
	/* window surface */
	cairo_surface_t * window_surf;

private:
	/* avoid copy */
	renderable_window_t(renderable_window_t const &);
	renderable_window_t & operator=(renderable_window_t const &);
public:
	renderable_window_t(Display *, Window , Visual *, box_int_t const &);
	virtual ~renderable_window_t();

	void create_render_context();
	void destroy_render_context();

	virtual void repair1(cairo_t * cr, box_int_t const & area);
	virtual box_int_t get_absolute_extend();
	box_int_t get_requested_size();
	virtual region_t<int> get_area();
	virtual void reconfigure(box_int_t const & area);
	virtual void mark_dirty();
	virtual void mark_dirty_retangle(box_int_t const & area);
	virtual bool is_visible();

	void set_opacity(double x);

	void set_map(bool status);

};

typedef std::list<window_t *> window_list_t;
typedef std::set<window_t *> window_set_t;

}



#endif /* RENDERABLE_WINDOW_HXX_ */