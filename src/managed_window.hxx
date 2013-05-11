/*
 * managed_window.hxx
 *
 *  Created on: 16 mars 2013
 *      Author: gschwind
 */

#ifndef MANAGED_WINDOW_HXX_
#define MANAGED_WINDOW_HXX_

#include "xconnection.hxx"
#include "window.hxx"
#include "icon.hxx"
#include "window_icon_handler.hxx"
#include "theme_layout.hxx"

namespace page {

enum managed_window_type_e {
	MANAGED_FLOATING,
	MANAGED_NOTEBOOK,
	MANAGED_FULLSCREEN
};

class managed_window_t {
private:

	theme_layout_t const * theme;

	managed_window_type_e _type;

	unsigned _margin_top;
	unsigned _margin_bottom;
	unsigned _margin_left;
	unsigned _margin_right;

	box_int_t _floating_wished_position;

	box_int_t _wished_position;
	box_int_t _orig_position;
	box_int_t _base_position;

	cairo_t * _cr;
	cairo_surface_t * _surf;

	window_icon_handler_t * icon;

	/* avoid copy */
	managed_window_t(managed_window_t const &);
	managed_window_t & operator=(managed_window_t const &);

	void init_managed_type(managed_window_type_e type);

public:

	window_t * const orig;
	window_t * const base;

	managed_window_t(managed_window_type_e initial_type, window_t * w, window_t * border, theme_layout_t const * theme);
	virtual ~managed_window_t();

	void normalize();
	void iconify();

	void reconfigure();
	void fake_configure();

	void set_wished_position(box_int_t const & position);
	box_int_t const & get_wished_position() const;

	void delete_window(Time t);

	bool check_orig_position(box_int_t const & position);
	bool check_base_position(box_int_t const & position);


	box_int_t get_base_position() const;

	void set_managed_type(managed_window_type_e type);

	string get_title();

	cairo_t * get_cairo_context();

	void focus();

	managed_window_type_e get_type();

	window_icon_handler_t * get_icon();
	void update_icon();

	void set_theme(theme_layout_t const * theme);

	cairo_t * get_cairo();

	bool is(managed_window_type_e type);

};

}


#endif /* MANAGED_WINDOW_HXX_ */