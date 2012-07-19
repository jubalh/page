/*
 * client.cxx
 *
 *  Created on: Feb 25, 2011
 *      Author: gschwind
 */

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <cairo.h>
#include <cairo-xlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <cstring>
#include "client.hxx"

namespace page {

client_t::client_t(window_t & x) : w(x) {
	w.add_to_save_set();

	icon_surf = 0;
	init_icon();
}

client_t::~client_t() {

	w.write_wm_state(WithdrawnState);

	if (icon_surf != 0) {
		cairo_surface_destroy(icon_surf);
		icon_surf = 0;
	}

	if (icon.data != 0) {
		free(icon.data);
		icon.data = 0;
	}

	w.remove_from_save_set();
}

void client_t::update_size(int w, int h) {

	//if (is_fullscreen()) {
	//	height = cnx.root_size.h;
	//	width = cnx.root_size.w;
	//}

	/* default size if no size_hints is provided */
	width = w;
	height = h;

	this->w.read_wm_normal_hints();
	XSizeHints const * size_hints = this->w.get_size_hints();

	if (size_hints->flags & PMaxSize) {
		if (w > size_hints->max_width)
			w = size_hints->max_width;
		if (h > size_hints->max_height)
			h = size_hints->max_height;
	}

	if (size_hints->flags & PBaseSize) {
		if (w < size_hints->base_width)
			w = size_hints->base_width;
		if (h < size_hints->base_height)
			h = size_hints->base_height;
	} else if (size_hints->flags & PMinSize) {
		if (w < size_hints->min_width)
			w = size_hints->min_width;
		if (h < size_hints->min_height)
			h = size_hints->min_height;
	}

	if (size_hints->flags & PAspect) {
		if (size_hints->flags & PBaseSize) {
			/* ICCCM say if base is set substract base before aspect checking ref : ICCCM*/
			if ((w - size_hints->base_width) * size_hints->min_aspect.y
					< (h - size_hints->base_height) * size_hints->min_aspect.x) {
				/* reduce h */
				h = size_hints->base_height
						+ ((w - size_hints->base_width) * size_hints->min_aspect.y)
								/ size_hints->min_aspect.x;

			} else if ((w - size_hints->base_width) * size_hints->max_aspect.y
					> (h - size_hints->base_height) * size_hints->max_aspect.x) {
				/* reduce w */
				w = size_hints->base_width
						+ ((h - size_hints->base_height) * size_hints->max_aspect.x)
								/ size_hints->max_aspect.y;
			}
		} else {
			if (w * size_hints->min_aspect.y < h * size_hints->min_aspect.x) {
				/* reduce h */
				h = (w * size_hints->min_aspect.y) / size_hints->min_aspect.x;

			} else if (w * size_hints->max_aspect.y > h * size_hints->max_aspect.x) {
				/* reduce w */
				w = (h * size_hints->max_aspect.x) / size_hints->max_aspect.y;
			}
		}

	}

	if (size_hints->flags & PResizeInc) {
		w -= ((w - size_hints->base_width) % size_hints->width_inc);
		h -= ((h - size_hints->base_height) % size_hints->height_inc);
	}

	width = w;
	height = h;

	printf("Update #%lu window size %dx%d\n", this->w.get_xwin(), width,
			height);
}

void client_t::init_icon() {

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
	icon_data = w.get_icon_data(&n);
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
		/* find a icon */
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

void client_t::set_fullscreen() {
	w.set_fullscreen();
}

void client_t::unset_fullscreen() {
	w.unset_fullscreen();
}

void client_t::withdraw_to_X() {
//	printf("Manage #%lu\n", w.get_xwin());
	XWMHints const * hints = w.read_wm_hints();
	if (hints) {
		if (hints->initial_state == IconicState) {
			w.write_wm_state(IconicState);
		} else {
			w.write_wm_state(NormalState);
		}
	} else {
		w.write_wm_state(NormalState);
	}

//	if (w.is_dock()) {
//		printf("IsDock !\n");
//		unsigned int n;
//		long const * partial_struct = w.read_partial_struct();
//		if (partial_struct) {
//			w.set_dock_action();
//			w.map();
//			return;
//		} /* if has not partial struct threat it as normal window */
//	}

	w.set_default_action();

}

}
