/*
 * render_context.cxx
 *
 * copyright (2010-2014) Benoit Gschwind
 *
 * This code is licensed under the GPLv3. see COPYING file for more details.
 *
 */

#include <unistd.h>
#include <algorithm>
#include <list>

#include <cstdlib>

#include "compositor.hxx"
#include "atoms.hxx"

#include "display.hxx"

namespace page {

#define CHECK_CAIRO(x) do { \
	x;\
	print_cairo_status(cr, __FILE__, __LINE__); \
} while(false)


static void _draw_crossed_box(cairo_t * cr, rectangle const & box, double r, double g,
		double b) {

	return;

	cairo_save(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_identity_matrix(cr);

	cairo_set_source_rgb(cr, r, g, b);
	cairo_set_line_width(cr, 1.0);
	cairo_rectangle(cr, box.x + 0.5, box.y + 0.5, box.w - 1.0, box.h - 1.0);
	cairo_reset_clip(cr);
	cairo_stroke(cr);

	cairo_new_path(cr);
	cairo_move_to(cr, box.x + 0.5, box.y + 0.5);
	cairo_line_to(cr, box.x + box.w - 1.0, box.y + box.h - 1.0);

	cairo_move_to(cr, box.x + box.w - 1.0, box.y + 0.5);
	cairo_line_to(cr, box.x + 0.5, box.y + box.h - 1.0);
	cairo_stroke(cr);

	cairo_restore(cr);
}

void compositor_t::init_composite_overlay() {
	/* create and map the composite overlay window */
	composite_overlay = XCompositeGetOverlayWindow(_cnx->dpy(), _cnx->root());
	/* user input pass through composite overlay (mouse click for example)) */
	allow_input_passthrough(_cnx->dpy(), composite_overlay);
	/** Automatically redirect windows, but paint sub-windows manually */
	XCompositeRedirectSubwindows(_cnx->dpy(), _cnx->root(), CompositeRedirectManual);
}

void compositor_t::release_composite_overlay() {
	XCompositeUnredirectSubwindows(_cnx->dpy(), _cnx->root(), CompositeRedirectManual);
	disable_input_passthrough(_cnx->dpy(), composite_overlay);
	XCompositeReleaseOverlayWindow(_cnx->dpy(), composite_overlay);
	composite_overlay = None;
}

compositor_t::compositor_t(display_t * cnx, int damage_event, int xshape_event, int xrandr_event) : _cnx(cnx), damage_event(damage_event), xshape_event(xshape_event), xrandr_event(xrandr_event) {
	render_mode = COMPOSITOR_MODE_AUTO;

	composite_back_buffer = None;

	fade_in_length = 1000000000L;
	fade_out_length = 1000000000L;

	_A = shared_ptr<atom_handler_t>(new atom_handler_t(_cnx->dpy()));

	/* initialize composite */
	init_composite_overlay();

	/** some performance counter **/
	fast_region_surf_monitor = 0.0;
	slow_region_surf_monitor = 0.0;

	flush_count = 0;
	damage_count = 0;


	_back_buffer = nullptr;

	/* this will scan for all windows */
	update_layout();

}

compositor_t::~compositor_t() {

	if(composite_back_buffer != None)
		XdbeDeallocateBackBufferName(_cnx->dpy(), composite_back_buffer);

	cleanup_internal_data();
	release_composite_overlay();
};

void compositor_t::repair_area_region(region const & repair) {
	_pending_damage += repair;
}


void compositor_t::render() {

	region pending_damage;
	time_t tick;
	tick.get_time();

	/**
	 * go throw the list, update fading stuff, and remove outdated elements.
	 **/
	auto i = window_data.begin();
	while (i != window_data.end()) {
		if (i->second->fade_mode == composite_window_t::FADE_NONE) {
			++i;
			continue;
		} else if (i->second->fade_mode == composite_window_t::FADE_IN) {
			if (i->second->fade_start + fade_in_length > tick) {
				time_t diff = tick - i->second->fade_start;
				double alpha = 0.0;
				/* compute alpha with enough precision without reach bound */
				alpha = static_cast<double>(diff) / static_cast<double>(fade_in_length);

				//printf("%ld %ld %f\n", static_cast<int64_t>(diff), static_cast<int64_t>(fade_in_length), alpha);

				i->second->fade_step = alpha;
				pending_damage += i->second->get_region();
			} else {
				i->second->fade_step = 1.0;
				i->second->fade_mode = composite_window_t::FADE_NONE;
				pending_damage += i->second->get_region();
			}
			++i;
			continue;
		} else if (i->second->fade_mode == composite_window_t::FADE_OUT) {
			if (i->second->fade_start + fade_out_length > tick) {
				time_t diff = tick - i->second->fade_start;
				double alpha = 0.0;
				/* compute alpha with enough precision without reach bound */
				alpha = static_cast<double>(diff) / static_cast<double>(fade_out_length);

				//printf("%lu %lu %f\n", static_cast<int64_t>(diff), static_cast<int64_t>(fade_out_length), alpha);

				i->second->fade_step = 1.0 - alpha;
				pending_damage += i->second->get_region();
				++i;
				continue;
			} else {
				pending_damage += i->second->get_region();
				delete i->second;
				i = window_data.erase(i);
				continue;
			}
		}
	}

	repair_area_region(pending_damage);

	if(render_mode == COMPOSITOR_MODE_AUTO) {
		render_auto();
	} else {
		render_managed();
	}

}

struct _comp {
	bool operator()(renderable_t * a, renderable_t * b) {
		return a->z < b->z;
	}
};


void compositor_t::render_managed() {

	_comp compare;

	_graph_scene.sort(compare);

	_back_buffer = cairo_xlib_surface_create(_cnx->dpy(), composite_back_buffer,
			root_attributes.visual, root_attributes.width,
			root_attributes.height);
	cairo_t * cr = cairo_create(_back_buffer);
	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
	CHECK_CAIRO(cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
	CHECK_CAIRO(cairo_paint(cr));

	region area(0, 0, root_attributes.width, root_attributes.height);
	for(renderable_list_t::iterator i = _graph_scene.begin(); i != _graph_scene.end(); ++i) {
		(*i)->render(cr, area);
	}

	CHECK_CAIRO(cairo_surface_flush(_back_buffer));
	cairo_destroy(cr);
	cairo_surface_destroy(_back_buffer);
	_back_buffer = nullptr;

	XdbeSwapInfo si;
	si.swap_window = composite_overlay;
	si.swap_action = None;
	XdbeSwapBuffers(_cnx->dpy(), &si, 1);

}


void compositor_t::render_auto() {

	if(_pending_damage.empty()) {
		return;
	}

	_pending_damage.clear();

	/**
	 * list content is bottom window to upper window in stack a optimization.
	 **/
	std::list<composite_window_t *> visible;
	for (std::list<Window>::iterator i = window_stack.begin();
			i != window_stack.end(); ++i) {
		map<Window, composite_window_t *>::iterator x = window_data.find(*i);
		if (x != window_data.end()) {
			visible.push_back(x->second);
		}
	}

	_back_buffer = cairo_xlib_surface_create(_cnx->dpy(), composite_back_buffer,
			root_attributes.visual, root_attributes.width,
			root_attributes.height);

	if(_back_buffer == nullptr)
		return;

	cairo_t * cr = cairo_create(_back_buffer);
	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
	CHECK_CAIRO(cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
	CHECK_CAIRO(cairo_paint(cr));

	/**
	 * Find region where windows are not overlap each other. i.e. region where
	 * only one window will be rendered.
	 * This is often more than 80% of the screen.
	 * This kind of region will be directly rendered.
	 **/

	region region_without_overlapped_window;
	for (list<composite_window_t *>::iterator i = visible.begin();
			i != visible.end(); ++i) {
		region r = (*i)->get_region();
		for (list<composite_window_t *>::iterator j = visible.begin();
				j != visible.end(); ++j) {
			if (i != j) {
				r -= (*j)->get_region();
			}
		}
		region_without_overlapped_window += r;
	}

	//region_without_overlapped_window = region_without_overlapped_window;

	/**
	 * Find region where the windows on top is not a window with alpha.
	 * Small area, often dropdown menu.
	 * This kind of area will be directly rendered.
	 **/
	region region_without_alpha_on_top;

	/**
	 * Walk over all all window from bottom to top one. If window has alpha
	 * channel remove it from region with no alpha, if window do not have alpha
	 * add this window to the area.
	 **/
	for (list<composite_window_t *>::iterator i = visible.begin();
			i != visible.end(); ++i) {
		if ((*i)->fade_mode != composite_window_t::FADE_NONE) {
			region_without_alpha_on_top -= (*i)->get_region();
		} else {
			/* if not has_alpha, add this area */
			region_without_alpha_on_top -= (*i)->get_region();
			region_without_alpha_on_top += (*i)->get_opaque_region();
		}
	}

	//region_without_alpha_on_top = region_without_alpha_on_top;
	region_without_alpha_on_top -= region_without_overlapped_window;


	region slow_region = _desktop_region;
	slow_region -= region_without_overlapped_window;
	slow_region -= region_without_alpha_on_top;


	/* direct render, area where there is only one window visible */
	{
		CHECK_CAIRO(cairo_reset_clip(cr));
		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_OVER));
		CHECK_CAIRO(cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE));

		region::const_iterator i = region_without_overlapped_window.begin();
		while (i != region_without_overlapped_window.end()) {
			fast_region_surf_monitor += i->w * i->h;
			repair_buffer(visible, cr, *i);
			cairo_surface_flush(_back_buffer);
			// for debuging
			_draw_crossed_box(cr, (*i), 0.0, 1.0, 0.0);
			++i;
		}
	}

	/* directly render area where window on the has no alpha */

	{
		CHECK_CAIRO(cairo_reset_clip(cr));
		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_OVER));
		CHECK_CAIRO(cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE));

		/* from top to bottom */
		for (list<composite_window_t *>::reverse_iterator i =
				visible.rbegin(); i != visible.rend(); ++i) {
			composite_window_t * r = *i;
			region draw_area = region_without_alpha_on_top & r->get_region();
			if (!draw_area.empty()) {
				for (region::const_iterator j = draw_area.begin();
						j != draw_area.end(); ++j) {
					if (!j->is_null()) {
						r->draw_to(cr, *j);
						cairo_surface_flush(_back_buffer);
						/* this section show direct rendered screen */
						_draw_crossed_box(cr, *j, 0.0, 0.0, 1.0);
					}
				}
			}
			region_without_alpha_on_top -= r->get_region();
		}
	}

	if (!slow_region.empty()) {
		//region_t<int> slow_region = _desktop_region;
		CHECK_CAIRO(cairo_reset_clip(cr));
		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_OVER));

		for (auto & i : slow_region) {
			slow_region_surf_monitor += i.w * i.h;
			repair_buffer(visible, cr, i);
			cairo_surface_flush(_back_buffer);
			_draw_crossed_box(cr, i, 1.0, 0.0, 0.0);
		}
	}

	CHECK_CAIRO(cairo_surface_flush(_back_buffer));
	cairo_destroy(cr);
	cairo_surface_destroy(_back_buffer);
	_back_buffer = nullptr;

	XdbeSwapInfo si;
	si.swap_window = composite_overlay;
	/* do what you want with current front buffer, maybe destroy it */
	si.swap_action = XdbeUndefined;
	XdbeSwapBuffers(_cnx->dpy(), &si, 1);

	_pending_damage.clear();

}


void compositor_t::repair_buffer(std::list<composite_window_t *> & visible,
		cairo_t * cr, rectangle const & area) {
	for (auto i : visible) {
		composite_window_t * r = i;
		region const & barea = r->get_region();
		for (auto & j : barea) {
			rectangle clip = area & (j);
			if (!clip.is_null()) {
				r->draw_to(cr, clip);
			}
		}
	}
}


void compositor_t::repair_overlay(cairo_t * cr, rectangle const & area,
		cairo_surface_t * src) {

	cairo_reset_clip(cr);
	cairo_identity_matrix(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, src, 0, 0);
	cairo_rectangle(cr, area.x, area.y, area.w, area.h);
	cairo_fill(cr);

	/* for debug purpose */
	if (false) {
		static int color = 0;
		switch (color % 3) {
		case 0:
			cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
			break;
		case 1:
			cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
			break;
		case 2:
			cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
			break;
		}
		//++color;
		cairo_set_line_width(cr, 1.0);
		cairo_rectangle(cr, area.x + 0.5, area.y + 0.5, area.w - 1.0,
				area.h - 1.0);
		cairo_clip(cr);
		cairo_paint_with_alpha(cr, 0.3);
		cairo_reset_clip(cr);
		//cairo_stroke(composite_overlay_cr);
	}

	// Never flush composite_overlay_s because this surface is never a source surface.
	//cairo_surface_flush(composite_overlay_s);

}


void compositor_t::process_event(XCreateWindowEvent const & e) {
	if(e.send_event == True)
		return;

	if (e.window == composite_overlay)
		return;
	if (e.parent != _cnx->root())
		return;

	stack_window_place_on_top(e.window);
}

void compositor_t::process_event(XReparentEvent const & e) {

	/** Ignore fake event **/
	if(e.send_event == True)
		return;


	/**
	 * Reparent are preceded by UnmapWindow and followed by MapWindow when
	 * needed.
	 *
	 * Reparent notify is usefull to maintaint window stack at root window level.
	 *
	 **/

	if (e.parent == _cnx->root()) {
		stack_window_place_on_top(e.window);
	} else {
		stack_window_remove(e.window);
	}
}

void compositor_t::process_event(XMapEvent const & e) {
	if (e.send_event == True)
		return;
	if (e.event != _cnx->root())
		return;

	/** don't make composite overlay visible **/
	if (e.window == composite_overlay)
		return;

	XWindowAttributes wa;
	if (XGetWindowAttributes(_cnx->dpy(), e.window, &wa)) {
		if (wa.c_class == InputOutput) {

			create_damage(e.window, wa);
			_pending_damage += rectangle(wa.x, wa.y, wa.width, wa.height);

			p_composite_surface_t x = get_composite_surface(e.window, wa);
			if(has_key(window_data, e.window)) {
				delete window_data[e.window];
				window_data.erase(e.window);
			}
			composite_window_t * w = new composite_window_t(_cnx, e.window, &wa, x);;
			window_data[e.window] = w;
			w->fade_start.get_time();
			w->fade_mode = composite_window_t::FADE_IN;
			repair_area_region(w->get_region());
		}
	}

	/* Recreate offscreen pixmap for this window */
	auto i = window_to_composite_surface.find(e.window);
	if(i != window_to_composite_surface.end()) {
		i->second->onmap();
	}
}

void compositor_t::process_event(XUnmapEvent const & e) {
	if (e.send_event == True)
		return;
	auto x = window_data.find(e.window);
	if (x != window_data.end()) {
		x->second->fade_start.get_time();
		x->second->fade_mode = composite_window_t::FADE_OUT;
		repair_area_region(x->second->get_region());
	}
	destroy_damage(e.window);
	destroy_composite_surface(e.window);
}

void compositor_t::process_event(XDestroyWindowEvent const & e) {
	if (e.send_event == True)
		return;

	auto x = window_data.find(e.window);
	if (x != window_data.end()) {
		if (x->second->fade_mode == composite_window_t::FADE_NONE) {
			x->second->fade_start.get_time();
			x->second->fade_mode = composite_window_t::FADE_OUT;
			repair_area_region(x->second->get_region());
		}
	}
}

void compositor_t::process_event(XConfigureEvent const & e) {
	if (e.send_event == True)
		return;

	if (e.event == _cnx->root()
			&& e.window != _cnx->root()) {

		stack_window_update(e.window, e.above);

		auto x = window_data.find(e.window);
		if (x != window_data.end()) {
			region r = x->second->position();
			x->second->update_position(e);
			r += x->second->position();
			repair_area_region(r);
		}

		auto i = window_to_composite_surface.find(e.window);
		if (i != window_to_composite_surface.end()) {
			i->second->onresize(e.width, e.height);
		}

	}
}


void compositor_t::process_event(XCirculateEvent const & e) {
	if(e.send_event == True)
		return;

	if(e.event != _cnx->root())
		return;

	if(e.place == PlaceOnTop) {
		stack_window_place_on_top(e.window);
	} else {
		stack_window_place_on_bottom(e.window);
	}

	if(has_key(window_data, e.window)) {
		repair_area_region(window_data[e.window]->position());
	}

}

void compositor_t::process_event(XDamageNotifyEvent const & e) {

//	printf("Damage area %dx%d+%d+%d\n", e.area.width, e.area.height, e.area.x,
//			e.area.y);
//	printf("Damage geometry %dx%d+%d+%d\n", e.geometry.width, e.geometry.height,
//			e.geometry.x, e.geometry.y);

	++damage_count;

	region r = read_damaged_region(e.damage);

	if (e.drawable == composite_overlay
			|| e.drawable == DefaultRootWindow(_cnx->dpy()))
		return;

	map<Window, composite_window_t *>::iterator x = window_data.find(
			e.drawable);
	if (x == window_data.end())
		return;

	//x->second->add_damaged_region(r);

	r.translate(e.geometry.x, e.geometry.y);
	repair_area_region(r);

}

region compositor_t::read_damaged_region(Damage d) {

	region result;

	/* create an empty region */
	XserverRegion region = XFixesCreateRegion(_cnx->dpy(), 0, 0);

	if (!region)
		throw std::runtime_error("could not create region");

	/* get damaged region and remove them from damaged status */
	XDamageSubtract(_cnx->dpy(), d, None, region);

	XRectangle * rects;
	int nb_rects;

	/* get all rectangles for the damaged region */
	rects = XFixesFetchRegion(_cnx->dpy(), region, &nb_rects);

	if (rects) {
		for (int i = 0; i < nb_rects; ++i) {
			//printf("rect %dx%d+%d+%d\n", rects[i].width, rects[i].height, rects[i].x, rects[i].y);
			result += rectangle(rects[i]);
		}
		XFree(rects);
	}

	XFixesDestroyRegion(_cnx->dpy(), region);

	return result;
}

void compositor_t::scan() {

	unsigned int num = 0;
	Window d1, d2, *wins = 0;


	XGrabServer(_cnx->dpy());
	cout << "XGrabServer(" << _cnx->dpy() << ")" << endl;

	window_stack.clear();
	window_data.clear();

	/** read all current root window **/
	if (XQueryTree(_cnx->dpy(), _cnx->root(), &d1, &d2, &wins, &num)
			!= 0) {

		if (num > 0)
			window_stack.insert(window_stack.end(), &wins[0], &wins[num]);

		for (unsigned i = 0; i < num; ++i) {
			if(wins[i] == composite_overlay)
				continue;
			XWindowAttributes wa;
			if (XGetWindowAttributes(_cnx->dpy(), wins[i], &wa)) {
				if(wa.c_class == InputOutput) {
					if (wa.map_state != IsUnmapped) {

						create_damage(wins[i], wa);
						_pending_damage += rectangle(wa.x, wa.y, wa.width,
								wa.height);

						p_composite_surface_t x = get_composite_surface(
								wins[i], wa);
						if (has_key(window_data, wins[i])) {
							delete window_data[wins[i]];
							window_data.erase(wins[i]);
						}
						composite_window_t * w = new composite_window_t(_cnx,
								wins[i], &wa, x);
						window_data[wins[i]] = w;
						w->fade_start.get_time();
						w->fade_mode = composite_window_t::FADE_IN;
						repair_area_region(w->get_region());

						auto k = window_to_composite_surface.find(wins[i]);
						if (k != window_to_composite_surface.end()) {
							k->second->onmap();
						}
					}
				}
			}
		}

		XFree(wins);
	}

	XUngrabServer(_cnx->dpy());
	cout << "XUngrabServer(" << _cnx->dpy() << ")" << endl;
	xflush();

}

void compositor_t::process_event(XEvent const & e) {

//	if (e.xany.type > 0 && e.xany.type < LASTEvent) {
//		printf("#%lu type: %s, send_event: %u, window: %lu\n", e.xany.serial,
//				x_event_name[e.xany.type], e.xany.send_event, e.xany.window);
//	} else {
//		printf("#%lu type: %u, send_event: %u, window: %lu\n", e.xany.serial,
//				e.xany.type, e.xany.send_event, e.xany.window);
//	}


	if (e.type == CirculateNotify) {
		process_event(e.xcirculate);
	} else if (e.type == ConfigureNotify) {
		process_event(e.xconfigure);
	} else if (e.type == CreateNotify) {
		process_event(e.xcreatewindow);
	} else if (e.type == DestroyNotify) {
		process_event(e.xdestroywindow);
	} else if (e.type == MapNotify) {
		process_event(e.xmap);
	} else if (e.type == ReparentNotify) {
		process_event(e.xreparent);
	} else if (e.type == UnmapNotify) {
		process_event(e.xunmap);
	} else if (e.type == damage_event + XDamageNotify) {
		process_event(reinterpret_cast<XDamageNotifyEvent const &>(e));
	} else if (e.type == xshape_event + ShapeNotify) {
		XShapeEvent const & sev = reinterpret_cast<XShapeEvent const &>(e);
		if(has_key(window_data, sev.window)) {
			window_data[sev.window]->update_shape();
			window_data[sev.window]->update_opaque_region();
		}
	} else if (e.type == xrandr_event + RRNotify) {
		//printf("RRNotify\n");
		XRRNotifyEvent const & ev = reinterpret_cast<XRRNotifyEvent const &>(e);
		if (ev.subtype == RRNotify_CrtcChange) {
			/* re-read root window attribute, in particular the size **/
			update_layout();
			//rebuild_cairo_context();
		}
	} else {
//		printf("Unhandled event\n");
//		if (e.xany.type > 0 && e.xany.type < LASTEvent) {
//			printf("#%lu type: %s, send_event: %u, window: %lu\n",
//					e.xany.serial, x_event_name[e.xany.type], e.xany.send_event,
//					e.xany.window);
//		} else {
//			printf("#%lu type: %u, send_event: %u, window: %lu\n",
//					e.xany.serial, e.xany.type, e.xany.send_event,
//					e.xany.window);
//		}
	}

}

void compositor_t::update_layout() {

	XGetWindowAttributes(_cnx->dpy(), DefaultRootWindow(_cnx->dpy()), &root_attributes);

	if(composite_back_buffer != None)
		XdbeDeallocateBackBufferName(_cnx->dpy(), composite_back_buffer);

	_desktop_region.clear();

	XRRScreenResources * resources = XRRGetScreenResourcesCurrent(_cnx->dpy(),
			DefaultRootWindow(_cnx->dpy()));

	for (int i = 0; i < resources->ncrtc; ++i) {
		XRRCrtcInfo * info = XRRGetCrtcInfo(_cnx->dpy(), resources,
				resources->crtcs[i]);

		/** if the CRTC has at less one output bound **/
		if(info->noutput > 0) {
			rectangle area(info->x, info->y, info->width, info->height);
			_desktop_region = _desktop_region + area;
		}
		XRRFreeCrtcInfo(info);
	}
	XRRFreeScreenResources(resources);

	if(_desktop_region.empty()) {
		/** fall back to one screen **/
		_desktop_region = rectangle(root_attributes.x, root_attributes.y, root_attributes.width, root_attributes.height);

	}

	printf("layout = %s\n", _desktop_region.to_string().c_str());

	composite_back_buffer = XdbeAllocateBackBufferName(_cnx->dpy(), composite_overlay, None);

	cleanup_internal_data();
	scan();

	/**
	 * Add the composite window to the stack as invisible window to track a
	 * valid stack. scan() currently does not report this window, I add remove
	 * line, in case of scan will report it.
	 **/
	stack_window_place_on_top(composite_overlay);

}


void compositor_t::process_events() {
	XEvent ev;
	while(XPending(_cnx->dpy())) {
		XNextEvent(_cnx->dpy(), &ev);
		process_event(ev);
	}
}

void compositor_t::stack_window_update(Window window, Window above) {
	if (above == None) {
		window_stack.remove(window);
		window_stack.push_front(window);
	} else {

		auto x = find(window_stack.begin(), window_stack.end(), above);

		if (x == window_stack.end()) {
			printf("Window not found : %lu for window %lu\n", above, window);
		} else {
			window_stack.remove(window);
			window_stack.insert(++x, window);
		}
	}
}

void compositor_t::stack_window_place_on_top(Window w) {
	window_stack.remove(w);
	window_stack.push_back(w);
}

void compositor_t::stack_window_place_on_bottom(Window w) {
	window_stack.remove(w);
	window_stack.push_front(w);
}

void compositor_t::stack_window_remove(Window w) {
	window_stack.remove(w);
}

//void compositor_t::save_state() {
//
//	compositor_window_state.clear();
//
//	for (map<Window, composite_window_t *>::iterator i = window_data.begin();
//			i != window_data.end(); ++i) {
//		compositor_window_state[i->first] = i->second->get_region();
//	}
//
//
//}

/**
 * Compute damaged area from last render_flush
 **/
//void compositor_t::compute_pending_damage() {
//
//	for (map<Window, composite_window_t *>::iterator i = window_data.begin();
//			i != window_data.end(); ++i) {
//
//		composite_window_t * cw = i->second;
//
//		if (cw->c_class() != InputOutput) {
//			continue;
//		}
//
//		if (cw->old_map_state() == IsUnmapped) {
//			if(cw->map_state() == IsUnmapped) {
//				continue;
//			} else {
//				/** was mapped **/
//				pending_damage += cw->get_region();
//			}
//		} else {
//			if(cw->map_state() == IsUnmapped) {
//				/** was unmapped **/
//				pending_damage += cw->old_region();
//			} else {
//				if (cw->has_moved()) {
//					/**
//					 * if moved or re-stacked, add previous location and current
//					 * location as damaged.
//					 **/
//					pending_damage += cw->old_region();
//					pending_damage += cw->get_region();
//				} else {
//					/**
//					 * What ever is happen, add damaged area
//					 **/
//					pending_damage += cw->get_damaged_region();
//				}
//			}
//		}
//
//		/**
//		 * reset damaged, and moved flags
//		 **/
//		i->second->clear_state();
//
//	}
//
//}


/**
 * Check event without blocking.
 **/
bool compositor_t::process_check_event() {
	XEvent e;

	int x;
	/** Passive check of events in queue, never flush any thing **/
	if ((x = XEventsQueued(_cnx->dpy(), QueuedAlready)) > 0) {
		/** should not block or flush **/
		XNextEvent(_cnx->dpy(), &e);
		process_event(e);
		return true;
	} else {
		return false;
	}
}

void compositor_t::cleanup_internal_data() {
	/** cleanup_internal_data composite window list **/
	for(auto & i: window_data) {
		delete i.second;
	}

	window_data.clear();

	/** cleanup_internal_data composite window surfaces **/
	window_to_composite_surface.clear();

}


}

