/*
 * managed_window.cxx
 *
 * copyright (2010-2014) Benoit Gschwind
 *
 * This code is licensed under the GPLv3. see COPYING file for more details.
 *
 */

#include <cairo.h>
#include <cairo-xlib.h>
#include "managed_window.hxx"
#include "notebook.hxx"
#include "utils.hxx"

namespace page {

managed_window_t::managed_window_t(Atom net_wm_type, client_base_t * c,
		theme_t const * theme) : managed_window_base_t(*c),

		_theme(theme), _type(MANAGED_FLOATING), _net_wm_type(net_wm_type), _floating_wished_position(), _notebook_wished_position(), _wished_position(), _orig_position(), _base_position(), _surf(
				0), _top_buffer(0), _bottom_buffer(0), _left_buffer(0), _right_buffer(
				0), _icon(0), _title(0), _orig_visual(0), _orig_depth(
				-1), _deco_visual(0), _deco_depth(-1), _orig(_id), _base(None), _deco(
				None), _floating_area(0), _is_durty(true), _is_focused(false)

{

	_floating_wished_position = rectangle(wa.x, wa.y,
			wa.width, wa.height);
	_notebook_wished_position = rectangle(wa.x, wa.y,
			wa.width, wa.height);

	_orig_visual = wa.visual;
	_orig_depth = wa.depth;

	if (wm_normal_hints != 0) {
		unsigned w = _floating_wished_position.w;
		unsigned h = _floating_wished_position.h;

		::page::compute_client_size_with_constraint(*(wm_normal_hints),
				_floating_wished_position.w, _floating_wished_position.h, w, h);

		_floating_wished_position.w = w;
		_floating_wished_position.h = h;
	}

	if (_floating_wished_position.x == 0) {
		_floating_wished_position.x = (wa.width
				- _floating_wished_position.w) / 2;
	}

	if (_floating_wished_position.y == 0) {
		_floating_wished_position.y = (wa.height
				- _floating_wished_position.h) / 2;
	}

	/** check if the window has motif no border **/
	{
		_motif_has_border = _cnx->motif_has_border(_orig);
	}

	/**
	 * Create the base window, window that will content managed window
	 **/

	XSetWindowAttributes swa;
	Window wbase;
	Window wdeco;
	rectangle b = _floating_wished_position;

	/** Common window properties **/
	unsigned long value_mask = CWOverrideRedirect;
	swa.override_redirect = True;

	Visual * root_visual = wa.visual;
	int root_depth = wa.depth;

	/**
	 * If window visual is 32 bit (have alpha channel, and root do not
	 * have alpha channel, use the window visual, otherwise always prefer
	 * root visual.
	 **/
	if (_orig_depth == 32 && root_depth != 32) {
		_deco_visual = _orig_visual;
		_deco_depth = _orig_depth;

		/** if visual is 32 bits, this values are mandatory **/
		swa.colormap = XCreateColormap(_cnx->dpy, _cnx->get_root_window(),
				_orig_visual,
				AllocNone);
		swa.background_pixel = BlackPixel(_cnx->dpy, _cnx->screen());
		swa.border_pixel = BlackPixel(_cnx->dpy, _cnx->screen());
		value_mask |= CWColormap | CWBackPixel | CWBorderPixel;

		wbase = XCreateWindow(_cnx->dpy, _cnx->get_root_window(), -10, -10, 1, 1,
				0, 32,
				InputOutput, _orig_visual, value_mask, &swa);
		wdeco = XCreateWindow(_cnx->dpy, wbase, b.x, b.y, b.w, b.h, 0, 32,
		InputOutput, _orig_visual, value_mask, &swa);

	} else {

		/**
		 * Create RGB window for back ground
		 **/

		XVisualInfo vinfo;
		if (XMatchVisualInfo(_cnx->dpy, _cnx->screen(), 32, TrueColor, &vinfo)
				== 0) {
			throw std::runtime_error(
					"Unable to find valid visual for background windows");
		}

		/**
		 * To create RGBA window, the following field MUST bet set, for unknown
		 * reason. i.e. border_pixel, background_pixel and colormap.
		 **/
		swa.border_pixel = 0;
		swa.background_pixel = 0;
		swa.colormap = XCreateColormap(_cnx->dpy, _cnx->get_root_window(),
				vinfo.visual, AllocNone);

		_deco_visual = vinfo.visual;
		_deco_depth = 32;
		value_mask |= CWColormap | CWBackPixel | CWBorderPixel;

		wbase = XCreateWindow(_cnx->dpy, _cnx->get_root_window(), -10, -10, 1, 1,
				0, 32, InputOutput, vinfo.visual, value_mask, &swa);
		wdeco = XCreateWindow(_cnx->dpy, wbase, b.x, b.y, b.w, b.h, 0, 32,
				InputOutput, vinfo.visual, value_mask, &swa);
	}

	_base = wbase;
	_deco = wdeco;

	_cnx->select_input(_base, MANAGED_BASE_WINDOW_EVENT_MASK);
	_cnx->select_input(_deco, MANAGED_DECO_WINDOW_EVENT_MASK);
	_cnx->select_input(_orig, MANAGED_ORIG_WINDOW_EVENT_MASK);

	/* Grab button click */
	grab_button_unfocused();

	_cnx->reparentwindow(_orig, _base, 0, 0);

	_surf = cairo_xlib_surface_create(_cnx->dpy, _deco, _deco_visual, b.w, b.h);

	reconfigure();

}

managed_window_t::~managed_window_t() {

	if(_surf != 0) {
		cairo_surface_destroy(_surf);
	}

	if(_icon != 0) {
		delete _icon;
	}

	if(_title != 0) {
		delete _title;
	}

	if (_floating_area != 0) {
		delete _floating_area;
	}

	destroy_back_buffer();

	_cnx->unmap(_orig);
	_cnx->reparentwindow(_orig, _cnx->get_root_window(), _wished_position.x,
			_wished_position.y);
	XDeleteProperty(_cnx->dpy, _orig, A(WM_STATE));
	XDestroyWindow(_cnx->dpy, _deco);
	XDestroyWindow(_cnx->dpy, _base);

}


void managed_window_t::reconfigure() {

	if(is(MANAGED_FLOATING)) {
		_wished_position = _floating_wished_position;

		if (_motif_has_border) {
			_base_position.x = _wished_position.x
					- _theme->floating_margin.left;
			_base_position.y = _wished_position.y - _theme->floating_margin.top;
			_base_position.w = _wished_position.w + _theme->floating_margin.left
					+ _theme->floating_margin.right;
			_base_position.h = _wished_position.h + _theme->floating_margin.top
					+ _theme->floating_margin.bottom;

			if (_base_position.x < 0)
				_base_position.x = 0;
			if (_base_position.y < 0)
				_base_position.y = 0;

			_orig_position.x = _theme->floating_margin.left;
			_orig_position.y = _theme->floating_margin.top;
			_orig_position.w = _wished_position.w;
			_orig_position.h = _wished_position.h;

		} else {
			_base_position = _wished_position;

			if (_base_position.x < 0)
				_base_position.x = 0;
			if (_base_position.y < 0)
				_base_position.y = 0;

			_orig_position.x = 0;
			_orig_position.y = 0;
			_orig_position.w = _wished_position.w;
			_orig_position.h = _wished_position.h;
		}

		cairo_xlib_surface_set_size(_surf, _base_position.w, _base_position.h);

		destroy_back_buffer();
		create_back_buffer();

		region r(_orig_position);
		set_opaque_region(_base, r);

	} else {
		_wished_position = _notebook_wished_position;
		_base_position = _notebook_wished_position;
		_orig_position = rectangle(0, 0, _base_position.w, _base_position.h);

		destroy_back_buffer();

		region r(_orig_position);
		set_opaque_region(_base, r);

	}

	_cnx->move_resize(_base, _base_position);
	_cnx->move_resize(_deco, rectangle(0, 0, _base_position.w, _base_position.h));
	_cnx->move_resize(_orig, _orig_position);

	fake_configure();

	mark_durty();
	expose();

}

void managed_window_t::fake_configure() {
	//printf("fake_reconfigure = %dx%d+%d+%d\n", _wished_position.w,
	//		_wished_position.h, _wished_position.x, _wished_position.y);
	_cnx->fake_configure(_orig, _wished_position, 0);
}

void managed_window_t::delete_window(Time t) {
	XEvent ev;
	ev.xclient.display = _cnx->dpy;
	ev.xclient.type = ClientMessage;
	ev.xclient.format = 32;
	ev.xclient.message_type = A(WM_PROTOCOLS);
	ev.xclient.window = _orig;
	ev.xclient.data.l[0] = A(WM_DELETE_WINDOW);
	ev.xclient.data.l[1] = t;
	_cnx->send_event(_orig, False, NoEventMask, &ev);
}

bool managed_window_t::check_orig_position(rectangle const & position) {
	return position == _orig_position;
}

bool managed_window_t::check_base_position(rectangle const & position) {
	return position == _base_position;
}

void managed_window_t::init_managed_type(managed_window_type_e type) {
	_type = type;
}

void managed_window_t::set_managed_type(managed_window_type_e type) {

	_is_durty = true;

	list<Atom> net_wm_allowed_actions;
	net_wm_allowed_actions.push_back(A(_NET_WM_ACTION_CLOSE));
	net_wm_allowed_actions.push_back(A(_NET_WM_ACTION_FULLSCREEN));
	_cnx->write_net_wm_allowed_actions(_orig, net_wm_allowed_actions);

	_type = type;
	reconfigure();

}

void managed_window_t::focus(Time t) {
	set_focus_state(true);
	icccm_focus(t);
}

rectangle managed_window_t::get_base_position() const {
	return _base_position;
}

managed_window_type_e managed_window_t::get_type() {
	return _type;
}

void managed_window_t::set_theme(theme_t const * theme) {
	this->_theme = theme;
}

bool managed_window_t::is(managed_window_type_e type) {
	return _type == type;
}

void managed_window_t::expose() {
	if (is(MANAGED_FLOATING)) {

		if (_is_durty) {
			_is_durty = false;
			_theme->render_floating(this);
		}

		cairo_xlib_surface_set_size(_surf, _base_position.w, _base_position.h);

		cairo_t * _cr = cairo_create(_surf);

		/** top **/
		if (_top_buffer != 0) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr, 0, 0, _base_position.w,
					_theme->floating_margin.top);
			cairo_set_source_surface(_cr, _top_buffer, 0, 0);
			cairo_fill(_cr);
		}

		/** bottom **/
		if (_bottom_buffer != 0) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr, 0,
					_base_position.h - _theme->floating_margin.bottom,
					_base_position.w, _theme->floating_margin.bottom);
			cairo_set_source_surface(_cr, _bottom_buffer, 0,
					_base_position.h - _theme->floating_margin.bottom);
			cairo_fill(_cr);
		}

		/** left **/
		if (_left_buffer != 0) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr, 0.0, _theme->floating_margin.top,
					_theme->floating_margin.left,
					_base_position.h - _theme->floating_margin.top
							- _theme->floating_margin.bottom);
			cairo_set_source_surface(_cr, _left_buffer, 0.0,
					_theme->floating_margin.top);
			cairo_fill(_cr);
		}

		/** right **/
		if (_right_buffer != 0) {
			cairo_set_operator(_cr, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(_cr,
					_base_position.w - _theme->floating_margin.right,
					_theme->floating_margin.top, _theme->floating_margin.right,
					_base_position.h - _theme->floating_margin.top
							- _theme->floating_margin.bottom);
			cairo_set_source_surface(_cr, _right_buffer,
					_base_position.w - _theme->floating_margin.right,
					_theme->floating_margin.top);
			cairo_fill(_cr);
		}

		cairo_surface_flush(_surf);
	}
}

void managed_window_t::icccm_focus(Time t) {
	//fprintf(stderr, "Focus time = %lu\n", t);

	if (wm_hints != 0) {
		if (wm_hints->input == True) {
			_cnx->set_input_focus(_orig, RevertToParent, t);
		}
	} else {
		/** no WM_HINTS, guess hints.input == True **/
		_cnx->set_input_focus(_orig, RevertToParent, t);
	}

	if (wm_protocols != 0) {
		if (::std::find(wm_protocols->begin(),
				wm_protocols->end(), A(WM_TAKE_FOCUS))
				!= wm_protocols->end()) {
			XEvent ev;
			ev.xclient.display = _cnx->dpy;
			ev.xclient.type = ClientMessage;
			ev.xclient.format = 32;
			ev.xclient.message_type = A(WM_PROTOCOLS);
			ev.xclient.window = _orig;
			ev.xclient.data.l[0] = A(WM_TAKE_FOCUS);
			ev.xclient.data.l[1] = t;
			_cnx->send_event(_orig, False, NoEventMask, &ev);
		}
	}

}

void managed_window_t::set_opaque_region(Window w, region & region) {
	vector<long> data(region.size() * 4);
	region::iterator i = region.begin();
	int k = 0;
	while (i != region.end()) {
		data[k++] = (*i).x;
		data[k++] = (*i).y;
		data[k++] = (*i).w;
		data[k++] = (*i).h;
		++i;
	}

	_cnx->change_property(w, _NET_WM_OPAQUE_REGION, CARDINAL, 32,
			PropModeReplace, reinterpret_cast<unsigned char *>(&data[0]),
			data.size());

}


}


