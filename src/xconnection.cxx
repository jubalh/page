/*
 * xconnection.cxx
 *
 *  Created on: Oct 30, 2011
 *      Author: gschwind
 */

#include "xconnection.hxx"
#include "X11/Xproto.h"

namespace page {

long int xconnection_t::last_serial = 0;
std::map<Display *, xconnection_t *> xconnection_t::open_connections;

bool xconnection_t::filter(event_t e) {
	return (e.serial < xconnection_t::last_serial);
}

xconnection_t::xconnection_t() {
	old_error_handler = XSetErrorHandler(error_handler);

	dpy = XOpenDisplay(0);
	if (dpy == NULL) {
		throw std::runtime_error("Could not open display");
	} else {
		printf("Open display : Success\n");
	}

	//XSynchronize(dpy, True);

	connection_fd = ConnectionNumber(dpy);

	grab_count = 0;
	screen = DefaultScreen(dpy);
	xroot = DefaultRootWindow(dpy) ;
	if (!get_window_attributes(xroot, &root_wa)) {
		throw std::runtime_error("Cannot get root window attributes");
	} else {
		printf("Get root windows attribute Success\n");
	}

	root_size.x = 0;
	root_size.y = 0;
	root_size.w = root_wa.width;
	root_size.h = root_wa.height;

	// Check if composite is supported.
	if (XQueryExtension(dpy, COMPOSITE_NAME, &composite_opcode,
			&composite_event, &composite_error)) {
		int major = 0, minor = 0; // The highest version we support
		XCompositeQueryVersion(dpy, &major, &minor);
		if (major != 0 || minor < 4) {
			throw std::runtime_error("X Server doesn't support Composite 0.4");
		} else {
			printf("using composite %d.%d\n", major, minor);
			extension_request_name_map[composite_opcode] =
					xcomposite_request_name;
		}
	} else {
		throw std::runtime_error("X Server doesn't support Composite");
	}

	// check/init Damage.
	if (!XQueryExtension(dpy, DAMAGE_NAME, &damage_opcode, &damage_event,
			&damage_error)) {
		throw std::runtime_error("Damage extension is not supported");
	} else {
		int major = 0, minor = 0;
		XDamageQueryVersion(dpy, &major, &minor);
		printf("Damage Extension version %d.%d found\n", major, minor);
		printf("Damage error %d, Damage event %d\n", damage_error,
				damage_event);
		extension_request_name_map[damage_opcode] = xdamage_func;
	}

	/* No macro for XINERAMA_NAME, use one we know */
	if (!XQueryExtension(dpy, "XINERAMA", &xinerama_opcode, &xinerama_event,
			&xinerama_error)) {
		throw std::runtime_error("Fixes extension is not supported");
	} else {
		int major = 0, minor = 0;
		XineramaQueryVersion(dpy, &major, &minor);
		printf("Xinerama Extension version %d.%d found\n", major, minor);
	}

	if (!XQueryExtension(dpy, SHAPENAME, &xshape_opcode, &xshape_event,
			&xinerama_error)) {
		throw std::runtime_error("Fixes extension is not supported");
	} else {
		int major = 0, minor = 0;
		XShapeQueryVersion(dpy, &major, &minor);
		printf("Shape Extension version %d.%d found\n", major, minor);
	}

	/* map & passtrough the overlay */
	composite_overlay = XCompositeGetOverlayWindow(dpy, xroot);
	allow_input_passthrough(composite_overlay);

	//XCompositeRedirectSubwindows(dpy, xroot, CompositeRedirectManual);

	/* initialize all atoms for this connection */
#define ATOM_INIT(name) atoms.name = XInternAtom(dpy, #name, False)

	ATOM_INIT(ATOM);
	ATOM_INIT(CARDINAL);
	ATOM_INIT(WINDOW);

	ATOM_INIT(WM_STATE);
	ATOM_INIT(WM_NAME);
	ATOM_INIT(WM_DELETE_WINDOW);
	ATOM_INIT(WM_PROTOCOLS);
	ATOM_INIT(WM_TAKE_FOCUS);

	ATOM_INIT(WM_NORMAL_HINTS);
	ATOM_INIT(WM_CHANGE_STATE);

	ATOM_INIT(_NET_SUPPORTED);
	ATOM_INIT(_NET_WM_NAME);
	ATOM_INIT(_NET_WM_STATE);
	ATOM_INIT(_NET_WM_STRUT_PARTIAL);

	ATOM_INIT(_NET_WM_WINDOW_TYPE);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_DOCK);

	ATOM_INIT(_NET_WM_WINDOW_TYPE);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_DESKTOP);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_DOCK);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_TOOLBAR);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_MENU);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_UTILITY);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_SPLASH);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_DIALOG);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_DROPDOWN_MENU);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_POPUP_MENU);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_TOOLTIP);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_NOTIFICATION);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_COMBO);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_DND);
	ATOM_INIT(_NET_WM_WINDOW_TYPE_NORMAL);

	ATOM_INIT(_NET_WM_USER_TIME);

	ATOM_INIT(_NET_CLIENT_LIST);
	ATOM_INIT(_NET_CLIENT_LIST_STACKING);

	ATOM_INIT(_NET_NUMBER_OF_DESKTOPS);
	ATOM_INIT(_NET_DESKTOP_GEOMETRY);
	ATOM_INIT(_NET_DESKTOP_VIEWPORT);
	ATOM_INIT(_NET_CURRENT_DESKTOP);

	ATOM_INIT(_NET_SHOWING_DESKTOP);
	ATOM_INIT(_NET_WORKAREA);

	ATOM_INIT(_NET_ACTIVE_WINDOW);

	ATOM_INIT(_NET_WM_STATE_MODAL);
	ATOM_INIT(_NET_WM_STATE_STICKY);
	ATOM_INIT(_NET_WM_STATE_MAXIMIZED_VERT);
	ATOM_INIT(_NET_WM_STATE_MAXIMIZED_HORZ);
	ATOM_INIT(_NET_WM_STATE_SHADED);
	ATOM_INIT(_NET_WM_STATE_SKIP_TASKBAR);
	ATOM_INIT(_NET_WM_STATE_SKIP_PAGER);
	ATOM_INIT(_NET_WM_STATE_HIDDEN);
	ATOM_INIT(_NET_WM_STATE_FULLSCREEN);
	ATOM_INIT(_NET_WM_STATE_ABOVE);
	ATOM_INIT(_NET_WM_STATE_BELOW);
	ATOM_INIT(_NET_WM_STATE_DEMANDS_ATTENTION);
	ATOM_INIT(_NET_WM_STATE_FOCUSED);

	ATOM_INIT(_NET_WM_ALLOWED_ACTIONS);

	/* _NET_WM_ALLOWED_ACTIONS */
	ATOM_INIT(_NET_WM_ACTION_MOVE);
	/*never allowed */
	ATOM_INIT(_NET_WM_ACTION_RESIZE);
	/* never allowed */
	ATOM_INIT(_NET_WM_ACTION_MINIMIZE);
	/* never allowed */
	ATOM_INIT(_NET_WM_ACTION_SHADE);
	/* never allowed */
	ATOM_INIT(_NET_WM_ACTION_STICK);
	/* never allowed */
	ATOM_INIT(_NET_WM_ACTION_MAXIMIZE_HORZ);
	/* never allowed */
	ATOM_INIT(_NET_WM_ACTION_MAXIMIZE_VERT);
	/* never allowed */
	ATOM_INIT(_NET_WM_ACTION_FULLSCREEN);
	/* allowed */
	ATOM_INIT(_NET_WM_ACTION_CHANGE_DESKTOP);
	/* never allowed */
	ATOM_INIT(_NET_WM_ACTION_CLOSE);
	/* always allowed */
	ATOM_INIT(_NET_WM_ACTION_ABOVE);
	/* never allowed */
	ATOM_INIT(_NET_WM_ACTION_BELOW);
	/* never allowed */

	ATOM_INIT(_NET_CLOSE_WINDOW);

	ATOM_INIT(_NET_FRAME_EXTENTS);

	ATOM_INIT(_NET_WM_ICON);

	ATOM_INIT(PAGE_QUIT);

#undef ATOM_INIT

	/* try to register composite manager */
	if (!register_cm())
		throw std::runtime_error("Another compositor running");

	open_connections[dpy] = this;

}

void xconnection_t::grab() {
	if (grab_count == 0) {
		unsigned long serial = XNextRequest(dpy);
		printf(">%08lu XGrabServer\n", serial);
		XGrabServer(dpy);
		serial = XNextRequest(dpy);
		printf(">%08lu XSync\n", serial);
		XSync(dpy, False);
	}
	++grab_count;
}

void xconnection_t::ungrab() {
	if (grab_count == 0) {
		fprintf(stderr, "TRY TO UNGRAB NOT GRABBED CONNECTION!\n");
		return;
	}
	--grab_count;
	if (grab_count == 0) {
		unsigned long serial = XNextRequest(dpy);
		printf(">%08lu XUngrabServer\n", serial);
		XUngrabServer(dpy);
		serial = XNextRequest(dpy);
		printf(">%08lu XFlush\n", serial);
		XFlush(dpy);
	}
}

bool xconnection_t::is_not_grab() {
	return grab_count == 0;
}

int xconnection_t::error_handler(Display * dpy, XErrorEvent * ev) {
	printf("#%08lu ERROR, major_code: %u, minor_code: %u, error_code: %u\n",
			ev->serial, ev->request_code, ev->minor_code, ev->error_code);

	if (open_connections.find(dpy) == open_connections.end()) {
		printf("Error on unknow connection\n");
		return 0;
	}

	xconnection_t * ths = open_connections[dpy];

	/* TODO: dump some use full information */
	char buffer[1024];
	XGetErrorText(dpy, ev->error_code, buffer, 1024);

	char const * func = 0;
	if (ev->request_code < 127 && ev->request_code > 0) {
		func = x_function_codes[ev->request_code];
	} else if (ths->extension_request_name_map.find(ev->request_code)
			!= ths->extension_request_name_map.end()) {
		func =
				ths->extension_request_name_map[ev->request_code][ev->minor_code];
	}

	if (func != 0) {
		printf("\e[1;31m%s: %s %lu\e[m\n", func, buffer, ev->serial);
	} else {
		printf("XXXXX %u: %s %lu\n", (unsigned) ev->request_code, buffer,
				ev->serial);
	}

	return 0;
}

void xconnection_t::allow_input_passthrough(Window w) {
	// undocumented : http://lists.freedesktop.org/pipermail/xorg/2005-January/005954.html
	XserverRegion region = XFixesCreateRegion(dpy, NULL, 0);
	// Shape for the entire of window.
	XFixesSetWindowShapeRegion(dpy, w, ShapeBounding, 0, 0, 0);
	// input shape was introduced by Keith Packard to define an input area of window
	// by default is the ShapeBounding which is used.
	// here we set input area an empty region.
	XFixesSetWindowShapeRegion(dpy, w, ShapeInput, 0, 0, region);
	XFixesDestroyRegion(dpy, region);
}

void xconnection_t::unmap(Window w) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu X_UnmapWindow: win = %lu\n", serial, w);
	XUnmapWindow(dpy, w);
	event_t e;
	e.serial = serial;
	e.type = UnmapNotify;
	pending.push_back(e);
}

void xconnection_t::reparentwindow(Window w, Window parent, int x, int y) {
	unsigned long serial = XNextRequest(dpy);
	//printf("Reparent serial: #%lu win: #%lu\n", serial, w);
	XReparentWindow(dpy, w, parent, x, y);
	event_t e;
	e.serial = serial;
	e.type = UnmapNotify;
	pending.push_back(e);
}

void xconnection_t::map(Window w) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu X_MapWindow: win = %lu\n", serial, w);
	XMapWindow(dpy, w);
	event_t e;
	e.serial = serial;
	e.type = MapNotify;
	pending.push_back(e);
}

bool xconnection_t::find_pending_event(event_t & e) {
	std::list<event_t>::iterator i = pending.begin();
	while (i != pending.end()) {
		if (e.type == (*i).type && e.serial == (*i).serial)
			return true;
		++i;
	}
	return false;
}

void xconnection_t::xnextevent(XEvent * ev) {
	XNextEvent(dpy, ev);
	xconnection_t::last_serial = ev->xany.serial;
	std::remove_if(pending.begin(), pending.end(), filter);
}

/* this fonction come from xcompmgr
 * it is intend to make page as composite manager */
bool xconnection_t::register_cm() {
	Window w;
	Atom a;
	static char net_wm_cm[] = "_NET_WM_CM_Sxx";

	snprintf(net_wm_cm, sizeof(net_wm_cm), "_NET_WM_CM_S%d", screen);
	a = XInternAtom(dpy, net_wm_cm, False);

	w = XGetSelectionOwner(dpy, a);
	if (w != None) {
		XTextProperty tp;
		char **strs;
		int count;
		Atom winNameAtom = XInternAtom(dpy, "_NET_WM_NAME", False);

		if (!get_text_property(w, &tp, winNameAtom)
				&& !get_text_property(w, &tp, XA_WM_NAME )) {
			fprintf(stderr,
					"Another composite manager is already running (0x%lx)\n",
					(unsigned long) w);
			return false;
		}
		if (XmbTextPropertyToTextList(dpy, &tp, &strs, &count) == Success) {
			fprintf(stderr,
					"Another composite manager is already running (%s)\n",
					strs[0]);

			XFreeStringList(strs);
		}

		XFree(tp.value);

		return false;
	}

	w = XCreateSimpleWindow(dpy, RootWindow (dpy, screen) , 0, 0, 1, 1, 0,
	None, None);

	Xutf8SetWMProperties(dpy, w, "page", "page", NULL, 0, NULL, NULL, NULL);

	XSetSelectionOwner(dpy, a, w, CurrentTime);

	return true;
}

void xconnection_t::add_to_save_set(Window w) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XAddToSaveSet: win = %lu\n", serial, w);
	XAddToSaveSet(dpy, w);
}

void xconnection_t::remove_from_save_set(Window w) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XRemoveFromSaveSet: win = %lu\n", serial, w);
	XRemoveFromSaveSet(dpy, w);
}

void xconnection_t::select_input(Window w, long int mask) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XSelectInput: win = %lu, mask = %08lx\n", serial, w,
			(unsigned long) mask);
	XSelectInput(dpy, w, mask);
}

void xconnection_t::move_resize(Window w, box_int_t const & size) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XMoveResizeWindow: win = %lu, %dx%d+%d+%d\n", serial, w,
			size.w, size.h, size.x, size.y);
	XMoveResizeWindow(dpy, w, size.x, size.y, size.w, size.h);
	event_t e;
	e.serial = serial;
	e.type = ConfigureNotify;
	pending.push_back(e);
}

void xconnection_t::set_window_border_width(Window w, unsigned int width) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XSetWindowBorderWidth: win = %lu, width = %u\n", serial, w,
			width);
	XSetWindowBorderWidth(dpy, w, width);
	event_t e;
	e.serial = serial;
	e.type = ConfigureNotify;
	pending.push_back(e);
}

void xconnection_t::raise_window(Window w) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XRaiseWindow: win = %lu\n", serial, w);
	XRaiseWindow(dpy, w);
	event_t e;
	e.serial = serial;
	e.type = ConfigureNotify;
	pending.push_back(e);
}

void xconnection_t::add_event_handler(xevent_handler_t * func) {
	event_handler_list.push_back(func);
}

void xconnection_t::remove_event_handler(xevent_handler_t * func) {
	assert(
			std::find(event_handler_list.begin(), event_handler_list.end(), func) != event_handler_list.end());
	event_handler_list.remove(func);
}

void xconnection_t::xconnection_t::process_next_event() {
	XEvent e;
	xnextevent(&e);

	/* since event handler can be removed on event, we copy it
	 * and check for event removed each time.
	 */
	std::vector<xevent_handler_t *> v(event_handler_list.begin(),
			event_handler_list.end());
	for (int i = 0; i < v.size(); ++i) {
		if (std::find(event_handler_list.begin(), event_handler_list.end(),
				v[i]) != event_handler_list.end()) {
			v[i]->process_event(e);
		}
	}
}

int xconnection_t::change_property(Window w, Atom property, Atom type,
		int format, int mode, unsigned char const * data, int nelements) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XChangeProperty: win = %lu\n", serial, w);
	return XChangeProperty(dpy, w, property, type, format, mode, data,
			nelements);
}

Status xconnection_t::get_window_attributes(Window w,
		XWindowAttributes * window_attributes_return) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XGetWindowAttributes: win = %lu\n", serial, w);
	return XGetWindowAttributes(dpy, w, window_attributes_return);
}

Status xconnection_t::get_text_property(Window w,
		XTextProperty * text_prop_return, Atom property) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XGetTextProperty: win = %lu\n", serial, w);
	return XGetTextProperty(dpy, w, text_prop_return, property);
}

int xconnection_t::lower_window(Window w) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XLowerWindow: win = %lu\n", serial, w);
	return XLowerWindow(dpy, w);
}

int xconnection_t::configure_window(Window w, unsigned int value_mask,
		XWindowChanges * values) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XConfigureWindow: win = %lu\n", serial, w);
	return XConfigureWindow(dpy, w, value_mask, values);
}

char * xconnection_t::get_atom_name(Atom atom) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XGetAtomName: atom = %lu\n", serial, atom);
	return XGetAtomName(dpy, atom);
}

Status xconnection_t::send_event(Window w, Bool propagate, long event_mask,
		XEvent* event_send) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XSendEvent: win = %lu\n", serial, w);
	return XSendEvent(dpy, w, propagate, event_mask, event_send);
}

int xconnection_t::set_input_focus(Window focus, int revert_to, Time time) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XSetInputFocus: win = %lu\n", serial, focus);
	return XSetInputFocus(dpy, focus, revert_to, time);
}

int xconnection_t::get_window_property(Window w, Atom property,
		long long_offset, long long_length, Bool c_delete, Atom req_type,
		Atom* actual_type_return, int* actual_format_return,
		unsigned long* nitems_return, unsigned long* bytes_after_return,
		unsigned char** prop_return) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XGetWindowProperty: win = %lu\n", serial, w);
	return XGetWindowProperty(dpy, w, property, long_offset, long_length,
			c_delete, req_type, actual_type_return, actual_format_return,
			nitems_return, bytes_after_return, prop_return);
}

XWMHints * xconnection_t::get_wm_hints(Window w) {
	unsigned long serial = XNextRequest(dpy);
	printf(">%08lu XGetWMHints: win = %lu\n", serial, w);
	return XGetWMHints(dpy, w);
}

}

