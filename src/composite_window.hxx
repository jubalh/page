/*
 * composite_window.hxx
 *
 *  copyright (2012) Benoit Gschwind
 *
 */

#ifndef COMPOSITE_WINDOW_HXX_
#define COMPOSITE_WINDOW_HXX_

#include <X11/Xlib.h>
#include <set>
#include <map>

#include "xconnection.hxx"
#include "region.hxx"
#include "icon.hxx"
#include "composite_window.hxx"

namespace page {

/**
 * This class is an handler of window, it just store some data cache about
 * an X window, and provide some macro.
 **/

class composite_window_t {
	/** short cut **/
	typedef region_t<int> _region_t;
	typedef box_t<int>	_box_t;

	Display * _dpy;
	Window _wid;
	Visual * _visual;
	Damage _damage;
	int _c_class;
	_region_t _region;
	_box_t _position;

	cairo_surface_t * _surf;

	bool _has_alpha;
	bool _has_shape;
	bool _is_map;


	/* avoid copy */
	composite_window_t(composite_window_t const &);
	composite_window_t & operator=(composite_window_t const &);
public:
	composite_window_t();
	~composite_window_t();
	void update(Display * dpy, Window w, XWindowAttributes & wa);
	void draw_to(cairo_t * cr, _box_t const & area);
	bool has_alpha();
	void init_cairo();
	void destroy_cairo();
	void update_map_state(bool is_map);
	void update_position(_box_t const & position);
	void read_shape();
	_region_t get_region();

	void create_damage();
	void destroy_damage();

	/**
	 * Inlined functions
	 **/

	bool is_visible() {
		return _is_map && _c_class == InputOutput;
	}

	box_int_t const & position() {
		return _position;
	}
};

}



#endif /* COMPOSITE_WINDOW_HXX_ */