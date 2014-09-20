/*
 * window_icon_handler.cxx
 *
 * copyright (2010-2014) Benoit Gschwind
 *
 * This code is licensed under the GPLv3. see COPYING file for more details.
 *
 */

#include <stdint.h>

#include <limits>
#include <cairo.h>
#include <cairo-xlib.h>


#include "window_icon_handler.hxx"

namespace page {

window_icon_handler_t::window_icon_handler_t(client_base_t const * c, unsigned int xsize, unsigned int ysize) {

	_c = c;
	icon_surf = 0;


	/* if window have icon properties */
	if (c->net_wm_icon() != nullptr) {
		vector<long> const & icon_data = *(c->net_wm_icon());

		uint32_t * icon_data32 = 0;

		icon_t selected;
		std::list<struct icon_t> icons;
		bool has_icon = false;

		/**
		 * copy long to 32 bits int, this is needed for 64bits arch (reminder:
		 * long in 64 bits arch are 64 bits)
		 **/
		icon_data32 = new uint32_t[icon_data.size()];
		for (unsigned int i = 0; i < icon_data.size(); ++i)
			icon_data32[i] = icon_data[i];

		/* find all icons and set up an handler */
		unsigned int offset = 0;
		while (offset < icon_data.size()) {
			icon_t tmp;
			tmp.width = icon_data[offset + 0];
			tmp.height = icon_data[offset + 1];
			tmp.data = (unsigned char *) &icon_data32[offset + 2];
			offset += 2 + tmp.width * tmp.height;
			icons.push_back(tmp);
		}


		icon_t ic;
		int x = std::numeric_limits<int>::max();
		int y = std::numeric_limits<int>::max();

		/* find the smallest icon that is greater than desired size */
		std::list<icon_t>::iterator i = icons.begin();
		while (i != icons.end()) {
			if ((*i).width >= (int) xsize and (*i).height >= (int) ysize
					and x > (*i).width and y > (*i).height) {
				x = (*i).width;
				y = (*i).height;
				ic = (*i);
				has_icon = true;
			}
			++i;
		}

		if (has_icon) {
			selected = ic;
		} else {
			/**
			 * if no usable icon are found, find the bigest one
			 **/

			x = 0, y = 0;
			std::list<icon_t>::iterator i = icons.begin();
			while (i != icons.end()) {
				if ((x * y) < ((*i).width * (*i).height)) {
					x = (*i).width, y = (*i).height;
					ic = (*i);
					has_icon = true;
				}
				++i;
			}

			if(has_icon) {
				selected = ic;
			}

		}

		if (has_icon) {

			//printf("selected icon : %dx%d\n", selected.width, selected.height);

			XVisualInfo vinfo;
			if (XMatchVisualInfo(c->cnx()->dpy(), c->cnx()->screen(), 32, TrueColor, &vinfo)
					== 0) {
				throw std::runtime_error(
						"Unable to find valid visual for background windows");
			}

			icon_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, xsize,
					ysize);

			int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
					selected.width);
			cairo_surface_t * tmp = cairo_image_surface_create_for_data(
					selected.data, CAIRO_FORMAT_ARGB32,
					selected.width, selected.height, stride);

			double x_ratio = xsize / (double)selected.width;
			double y_ratio = ysize / (double)selected.height;

			cairo_t * cr = cairo_create(icon_surf);

			cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
			cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
			cairo_paint(cr);

			cairo_scale(cr, x_ratio, y_ratio);
			cairo_set_source_surface(cr, tmp, 0.0, 0.0);
			cairo_rectangle(cr, 0, 0, selected.width, selected.height);
			cairo_pattern_set_filter(cairo_get_source(cr),
					CAIRO_FILTER_NEAREST);
			cairo_fill(cr);

			warn(cairo_get_reference_count(cr) == 1);
			cairo_destroy(cr);
			warn(cairo_surface_get_reference_count(tmp) == 1);
			cairo_surface_destroy(tmp);

		} else {
			icon_surf = 0;
		}

		delete[] icon_data32;

	} else {
		icon_surf = 0;
	}

}

window_icon_handler_t::~window_icon_handler_t() {
	if (icon_surf != nullptr) {
		warn(cairo_surface_get_reference_count(icon_surf) == 1);
		cairo_surface_destroy(icon_surf);
		icon_surf = nullptr;
	}
}

cairo_surface_t * window_icon_handler_t::get_cairo_surface() {
	return icon_surf;
}

}
