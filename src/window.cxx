/*
 * window.cxx
 *
 * copyright (2012) Benoit Gschwind
 *
 */

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <cairo.h>
#include <cairo-xlib.h>

#include <iostream>
#include <sstream>
#include <cassert>
#include <stdint.h>

#include "window.hxx"

namespace page {

long int const ClientEventMask = (StructureNotifyMask | PropertyChangeMask);

window_t::window_map_t window_t::created_window;

window_t::window_t(xconnection_t &cnx, Window w, XWindowAttributes const & wa) : cnx(cnx),
		xwin(w), has_partial_struct(false), lock_count(0), is_lock(
				false), window_surf(0), damage(None), opacity(1.0), has_wm_name(
				false), has_net_wm_name(false), wm_name(""), net_wm_name(""), name(
				"") {

	assert(created_window.find(w) == created_window.end());
	created_window[w] = this;

	icon.data = 0;
	icon_surf = 0;

	if (wa.map_state == IsUnmapped) {
		_is_map = false;
	} else {
		_is_map = true;
	}

	memset(partial_struct, 0, sizeof(partial_struct));
	size.x = wa.x;
	size.y = wa.y;
	size.w = wa.width;
	size.h = wa.height;

	requested_size = size;

	visual = wa.visual;
	w_class = wa.c_class;

	_override_redirect = (wa.override_redirect ? true : false);
	if(_override_redirect) {
		opacity = 1.0;
	}

	wm_input_focus = false;
	wm_state = WithdrawnState;
	_is_composite_redirected = false;

	if (wa.c_class == InputOutput) {
		create_render_context();
	}

	/*
	 * to ensure the state of transient_for and struct_partial, we select input for property change and then
	 * we sync wuth server, and then check window state, if window property change
	 * we get it.
	 */
	cnx.select_input(xwin, PropertyChangeMask);
	/* sync and flush, now we are sure that property change are listen */
	XFlush(cnx.dpy);
	XSync(cnx.dpy, False);

	XGrabButton(cnx.dpy, Button1, AnyModifier, xwin, False, ButtonPressMask, GrabModeSync, GrabModeAsync, cnx.xroot, None);

	/* read transient_for state */
	update_transient_for();
	update_icon();
	update_partial_struct();
	wm_normal_hints = XAllocSizeHints();
	wm_hints = XAllocWMHints();

}

void window_t::create_render_context() {
	if (window_surf == 0 && damage == None) {
		/* redirect to back buffer */
		// using XCompositeRedirectSubwindows
		//XCompositeRedirectWindow(cnx.dpy, xwin, CompositeRedirectManual);
		_is_composite_redirected = true;

		/* create the cairo surface */
		window_surf = cairo_xlib_surface_create(cnx.dpy, xwin, visual, size.w,
				size.h);
		if(!window_surf)
			printf("WARNING CAIRO FAIL\n");
		/* track update */
		damage = XDamageCreate(cnx.dpy, xwin, XDamageReportNonEmpty);
		if (damage)
			XDamageSubtract(cnx.dpy, damage, None, None);
		else
			printf("DAMAGE FAIL.\n");
	}

}

void window_t::destroy_render_context() {
	if (damage != None) {
		XDamageDestroy(cnx.dpy, damage);
		damage = None;
	}

	if (window_surf != 0) {
		cairo_surface_destroy(window_surf);
		window_surf = 0;
	}

	if (_is_composite_redirected) {
		// using XCompositeRedirectSubwindows
		//XCompositeUnredirectWindow(cnx.dpy, xwin, CompositeRedirectManual);
		_is_composite_redirected = false;
	}
}

void window_t::setup_extends() {
	/* set border */
	cnx.set_window_border_width(xwin, 0);

	/* set frame extend to 0 (I don't know why a client need this data) */
	long frame_extends[4] = { 0, 0, 0, 0 };
	cnx.change_property(xwin, cnx.atoms._NET_FRAME_EXTENTS,
			cnx.atoms.CARDINAL, 32, PropModeReplace,
			reinterpret_cast<unsigned char *>(frame_extends), 4);

}

window_t::~window_t() {

	XFree(wm_normal_hints);
	XFree(wm_hints);

	if (icon_surf != 0) {
		cairo_surface_destroy(icon_surf);
		icon_surf = 0;
	}

	if (icon.data != 0) {
		free(icon.data);
		icon.data = 0;
	}

	assert(created_window.find(xwin) != created_window.end());
	created_window.erase(xwin);
}

void window_t::read_all() {

	wm_input_focus = true;
	XWMHints const * hints = read_wm_hints();
	if (hints) {
		if ((hints->flags & InputHint)&& hints->input != True)
			wm_input_focus = false;
	}

	read_net_vm_name();
	read_vm_name();
	read_title();
	read_wm_normal_hints();
	read_net_wm_type();
	read_net_wm_state();
	read_net_wm_protocols();
	update_net_wm_user_time();

	wm_state = read_wm_state();

}

long window_t::read_wm_state() {
	int format;
	long result = -1;
	long * p = NULL;
	unsigned long n, extra;
	Atom real;

	if (cnx.get_window_property(xwin, cnx.atoms.WM_STATE, 0L, 2L, False,
			cnx.atoms.WM_STATE, &real, &format, &n, &extra,
			(unsigned char **) &p) != Success)
		return WithdrawnState;

	if (n != 0 && format == 32 && real == cnx.atoms.WM_STATE) {
		result = p[0];
	} else {
		printf("Error in WM_STATE %lu %d %lu\n", n, format, real);
		return WithdrawnState;
	}
	XFree(p);
	return result;
}

void window_t::map() {
	_is_map = true;
	cnx.map(xwin);
}

void window_t::unmap() {
	_is_map = false;
	cnx.unmap(xwin);
}

/* check if client is still alive */
bool window_t::try_lock() {
	cnx.grab();
	XEvent e;
	if (XCheckTypedWindowEvent(cnx.dpy, xwin, DestroyNotify, &e)) {
		XPutBackEvent(cnx.dpy, &e);
		cnx.ungrab();
		return false;
	}
	is_lock = true;
	return true;
}

void window_t::unlock() {
	is_lock = false;
	cnx.ungrab();
}

void window_t::focus() {

	if (_is_map && wm_input_focus) {
		cnx.set_input_focus(xwin, RevertToParent, cnx.last_know_time);
	}

	if (net_wm_protocols.find(cnx.atoms.WM_TAKE_FOCUS)
			!= net_wm_protocols.end()) {
		XEvent ev;
		ev.xclient.display = cnx.dpy;
		ev.xclient.type = ClientMessage;
		ev.xclient.format = 32;
		ev.xclient.message_type = cnx.atoms.WM_PROTOCOLS;
		ev.xclient.window = xwin;
		ev.xclient.data.l[0] = cnx.atoms.WM_TAKE_FOCUS;
		ev.xclient.data.l[1] = cnx.last_know_time;
		cnx.send_event(xwin, False, NoEventMask, &ev);
	}

	net_wm_state.insert(cnx.atoms._NET_WM_STATE_FOCUSED);
	write_net_wm_state();

	cnx.change_property(cnx.xroot, cnx.atoms._NET_ACTIVE_WINDOW,
			cnx.atoms.WINDOW, 32, PropModeReplace,
			reinterpret_cast<unsigned char *>(&(xwin)), 1);

}

void window_t::read_wm_normal_hints() {
	long size_hints_flags;
	if (!XGetWMNormalHints(cnx.dpy, xwin, wm_normal_hints, &size_hints_flags)) {
		/* size is uninitialized, ensure that size.flags aren't used */
		wm_normal_hints->flags = 0;
		printf("no WMNormalHints\n");
	}
}

XWMHints const *
window_t::read_wm_hints() {
	XFree(wm_hints);
	wm_hints = cnx.get_wm_hints(xwin);
	return wm_hints;
}

std::string const &
window_t::read_vm_name() {
	has_wm_name = false;
	char **list = NULL;
	XTextProperty name;
	cnx.get_text_property(xwin, &name, cnx.atoms.WM_NAME);
	if (!name.nitems) {
		XFree(name.value);
		wm_name = "none";
		return wm_name;
	}
	has_wm_name = true;
	wm_name = (char const *) name.value;
	XFree(name.value);
	return wm_name;
}

std::string const &
window_t::read_net_vm_name() {
	has_net_wm_name = false;
	char **list = NULL;
	XTextProperty name;
	cnx.get_text_property(xwin, &name, cnx.atoms._NET_WM_NAME);
	if (!name.nitems) {
		XFree(name.value);
		net_wm_name = "none";
		return net_wm_name;
	}
	has_net_wm_name = true;
	net_wm_name = (char const *) name.value;
	XFree(name.value);
	return net_wm_name;
}

/* inspired from dwm */
std::string const &
window_t::read_title() {
	if (has_net_wm_name) {
		name = net_wm_name;
		return name;
	}
	if (has_wm_name) {
		name = wm_name;
		return name;
	}

	std::stringstream s(std::stringstream::in | std::stringstream::out);
	s << "#" << (xwin) << " (noname)";
	name = s.str();
	return name;
}

void window_t::read_net_wm_type() {
	unsigned int num;
	long * val = get_properties32(cnx.atoms._NET_WM_WINDOW_TYPE, cnx.atoms.ATOM,
			&num);
	if (val) {
		net_wm_type.clear();
		/* use the first value that we know about in the array */
		for (unsigned i = 0; i < num; ++i) {
			net_wm_type.insert(val[i]);
		}
		delete[] val;
	}
}

void window_t::read_net_wm_state() {
	/* take _NET_WM_STATE */
	unsigned int n;
	long * net_wm_state = get_properties32(cnx.atoms._NET_WM_STATE,
			cnx.atoms.ATOM, &n);
	if (net_wm_state) {
		this->net_wm_state.clear();
		for (int i = 0; i < n; ++i) {
			this->net_wm_state.insert(net_wm_state[i]);
		}
		delete[] net_wm_state;
	}
}

void window_t::read_net_wm_protocols() {
	net_wm_protocols.clear();
	int count;
	Atom * atoms_list;
	if (XGetWMProtocols(cnx.dpy, xwin, &atoms_list, &count)) {
		for (int i = 0; i < count; ++i) {
			net_wm_protocols.insert(atoms_list[i]);
		}
		XFree(atoms_list);
	}
}

void window_t::write_net_wm_state() const {
	long * new_state = new long[net_wm_state.size() + 1]; // a less one long
	atom_set_t::const_iterator iter = net_wm_state.begin();
	int i = 0;
	while (iter != net_wm_state.end()) {
		new_state[i] = *iter;
		++iter;
		++i;
	}

	cnx.change_property(xwin, cnx.atoms._NET_WM_STATE, cnx.atoms.ATOM, 32,
			PropModeReplace, (unsigned char *) new_state, i);
	delete[] new_state;
}

void window_t::write_net_wm_allowed_actions() {
	long * data = new long[net_wm_allowed_actions.size() + 1]; // at less one long
	std::set<Atom>::iterator iter = net_wm_allowed_actions.begin();
	int i = 0;
	while (iter != net_wm_allowed_actions.end()) {
		data[i] = *iter;
		++iter;
		++i;
	}

	cnx.change_property(xwin, cnx.atoms._NET_WM_ALLOWED_ACTIONS,
			cnx.atoms.ATOM, 32, PropModeReplace,
			reinterpret_cast<unsigned char *>(data), i);
	delete[] data;
}

void window_t::move_resize(box_int_t const & location) {
	requested_size = location;
	if(size == location)
		return;
	cnx.move_resize(xwin, location);
	if (window_surf && (size.w != location.w || size.h != location.h)) {
		cairo_surface_flush(window_surf);
		cairo_xlib_surface_set_size(window_surf, location.w, location.h);
		mark_dirty();
	}
	size = location;
}

/**
 * Update the ICCCM WM_STATE window property
 * @input state should be NormalState, IconicState or WithDrawnState
 **/
void window_t::write_wm_state(long state) {

	/* NOTE : ICCCM CARD32 mean "long" C type */
	wm_state = state;

	struct {
		long state;
		Window icon;
	} data;

	data.state = state;
	data.icon = None;

	if (wm_state != WithdrawnState) {
		cnx.change_property(xwin, cnx.atoms.WM_STATE, cnx.atoms.WM_STATE,
				32, PropModeReplace, reinterpret_cast<unsigned char *>(&data),
				2);
	} else {
		XDeleteProperty(cnx.dpy, xwin, cnx.atoms.WM_STATE);
	}
}

bool window_t::is_dock() {
	return net_wm_type.find(cnx.atoms._NET_WM_WINDOW_TYPE_DOCK)
			!= net_wm_type.end();
}

bool window_t::is_fullscreen() {
	return net_wm_state.find(cnx.atoms._NET_WM_STATE_FULLSCREEN)
			!= net_wm_state.end();
}

bool window_t::demands_atteniion() {
	return net_wm_state.find(cnx.atoms._NET_WM_STATE_DEMANDS_ATTENTION)
			!= net_wm_state.end();
}


void window_t::set_net_wm_state(Atom x) {
	net_wm_state.insert(x);
	write_net_wm_state();
}

void window_t::unset_net_wm_state(Atom x) {
	net_wm_state.erase(x);
	write_net_wm_state();
}

void window_t::toggle_net_wm_state(Atom x) {
	if(net_wm_state.find(x)
			!= net_wm_state.end()) {
		unset_net_wm_state(x);
	} else {
		set_net_wm_state(x);
	}
}

void window_t::update_net_wm_user_time() {
	unsigned int n;
	long * time = get_properties32(cnx.atoms._NET_WM_USER_TIME, cnx.atoms.CARDINAL, &n);
	if(time) {
		user_time = time[0];
	} else {
		user_time = 0;
	}
}

void window_t::update_icon() {

	if (icon_surf != 0) {
		cairo_surface_destroy(icon_surf);
		icon_surf = 0;
	}

	if (icon.data != 0) {
		free(icon.data);
		icon.data = 0;
	}

	long * icon_data;
	int32_t * icon_data32;
	int icon_data_size;
	icon_t selected;
	std::list<struct icon_t> icons;
	bool has_icon = false;
	unsigned int n;
	icon_data_size = n;

	if (icon_data != 0) {
		icon_data32 = new int32_t[n];
		for (int i = 0; i < n; ++i)
			icon_data32[i] = icon_data[i];

		int offset = 0;
		while (offset < icon_data_size) {
			icon_t tmp;
			tmp.width = icon_data[offset + 0];
			tmp.height = icon_data[offset + 1];
			tmp.data = (unsigned char *) &icon_data32[offset + 2];
			offset += 2 + tmp.width * tmp.height;
			icons.push_back(tmp);
		}

		icon_t ic;
		int x = 0;
		/* find an icon */
		std::list<icon_t>::iterator i = icons.begin();
		while (i != icons.end()) {
			if ((*i).width <= 16 && x < (*i).width) {
				x = (*i).width;
				ic = (*i);
				has_icon = true;
			}
			++i;
		}

		if (has_icon) {
			selected = ic;
		} else {
			if (!icons.empty()) {
				selected = icons.front();
				has_icon = true;
			} else {
				has_icon = false;
			}

		}

		if (has_icon) {

			int target_height;
			int target_width;
			double ratio;

			if (selected.width > selected.height) {
				target_width = 16;
				target_height = (double) selected.height
						/ (double) selected.width * 16;
				ratio = (double) target_width / (double) selected.width;
			} else {
				target_height = 16;
				target_width = (double) selected.width
						/ (double) selected.height * 16;
				ratio = (double) target_height / (double) selected.height;
			}

			int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
					target_width);

			printf("reformat from %dx%d to %dx%d %f\n", selected.width,
					selected.height, target_width, target_height, ratio);

			icon.width = target_width;
			icon.height = target_height;
			icon.data = (unsigned char *) malloc(stride * target_height);

			for (int j = 0; j < target_height; ++j) {
				for (int i = 0; i < target_width; ++i) {
					int x = i / ratio;
					int y = j / ratio;
					if (x < selected.width && x >= 0 && y < selected.height
							&& y >= 0) {
						((uint32_t *) (icon.data + stride * j))[i] =
								((uint32_t*) selected.data)[x
										+ selected.width * y];
					}
				}
			}

			icon_surf = cairo_image_surface_create_for_data(
					(unsigned char *) icon.data, CAIRO_FORMAT_ARGB32,
					icon.width, icon.height, stride);

		} else {
			icon_surf = 0;
		}

		delete[] icon_data;
		delete[] icon_data32;

	} else {
		icon_surf = 0;
	}

}


void window_t::update_partial_struct() {
	unsigned int n;
	long * p_struct = get_properties32(cnx.atoms._NET_WM_STRUT_PARTIAL,
			cnx.atoms.CARDINAL, &n);

	if (p_struct && n == 12) {
		printf(
				"partial struct %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n",
				partial_struct[0], partial_struct[1], partial_struct[2],
				partial_struct[3], partial_struct[4], partial_struct[5],
				partial_struct[6], partial_struct[7], partial_struct[8],
				partial_struct[9], partial_struct[10], partial_struct[11]);

		has_partial_struct = true;
		memcpy(partial_struct, p_struct, sizeof(long) * 12);
		delete[] p_struct;
	} else {
		has_partial_struct = false;
	}
}

long const * window_t::get_partial_struct() {
	if (has_partial_struct) {
		return partial_struct;
	} else {
		return 0;
	}
}

void window_t::repair1(cairo_t * cr, box_int_t const & area) {
	assert(window_surf != 0);
	box_int_t clip = area & size;
	//printf("repair window %s %dx%d+%d+%d\n", get_title().c_str(), clip.w, clip.h, clip.x, clip.y);
	if (clip.w > 0 && clip.h > 0) {
		cairo_save(cr);
		cairo_reset_clip(cr);
		cairo_set_source_surface(cr, window_surf, size.x, size.y);
		cairo_rectangle(cr, clip.x, clip.y, clip.w, clip.h);
		cairo_clip(cr);
		cairo_paint_with_alpha(cr, opacity);
		cairo_restore(cr);
	}

//	cairo_save(cr);
//	cairo_reset_clip(cr);
//	cairo_set_source_surface(cr, window_surf, size.x, size.y);
//	cairo_rectangle(cr, size.x, size.y, size.w, size.h);
//	cairo_clip(cr);
//	cairo_paint_with_alpha(cr, opacity);
//	cairo_restore(cr);
}

box_int_t window_t::get_absolute_extend() {
	return size;
}

box_int_t window_t::get_requested_size() {
	return requested_size;
}


region_t<int> window_t::get_area() {
	return region_t<int>(size);
}

void window_t::reconfigure(box_int_t const & area) {
	move_resize(area);
}

void window_t::mark_dirty() {
	//cairo_surface_flush(window_surf);
	cairo_surface_mark_dirty(window_surf);
}

void window_t::mark_dirty_retangle(box_int_t const & area) {
	//cairo_surface_flush(window_surf);
	cairo_surface_mark_dirty_rectangle(window_surf, area.x, area.y, area.w,
			area.h);
}

void window_t::set_fullscreen() {
	/* update window state */
	net_wm_state.insert(cnx.atoms._NET_WM_STATE_FULLSCREEN);
	write_net_wm_state();
}

void window_t::unset_fullscreen() {
	/* update window state */
	net_wm_state.erase(cnx.atoms._NET_WM_STATE_FULLSCREEN);
	write_net_wm_state();
}

void window_t::process_configure_notify_event(XConfigureEvent const & e) {
	assert(e.window == xwin);

	size.x = e.x;
	size.y = e.y;
	size.w = e.width;
	size.h = e.height;

	if (window_surf) {
		cairo_surface_flush(window_surf);
		cairo_xlib_surface_set_size(window_surf, size.w, size.h);
		mark_dirty();
	}

}

void window_t::process_configure_request_event(
		XConfigureRequestEvent const & e) {
	assert(e.window == xwin);

	size.x = e.x;
	size.y = e.y;
	size.w = e.width;
	size.h = e.height;

	if (window_surf) {
		cairo_surface_flush(window_surf);
		cairo_xlib_surface_set_size(window_surf, size.w, size.h);
		mark_dirty();
	}

}

Window window_t::update_transient_for() {
	unsigned int n;
	long * data = get_properties32(cnx.atoms.WM_TRANSIENT_FOR, cnx.atoms.WINDOW, &n);
	if(data && n == 1) {
		_transient_for = data[0];
		delete[] data;
	} else {
		_transient_for = None;
	}
	return _transient_for;
}

}

