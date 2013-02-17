/*
 * window_icon_handler.hxx
 *
 *  Created on: 15 févr. 2013
 *      Author: gschwind
 */

#ifndef WINDOW_ICON_HANDLER_HXX_
#define WINDOW_ICON_HANDLER_HXX_

#include <cairo.h>
#include "icon.hxx"
#include "window.hxx"

namespace page {

class window_icon_handler_t {
	/* icon data */
	icon_t icon;
	/* icon surface */
	cairo_surface_t * icon_surf;
public:
	window_icon_handler_t(window_t * w);
	~window_icon_handler_t();
	cairo_surface_t * get_cairo_surface();

};

}



#endif /* WINDOW_ICON_HANDLER_HXX_ */
