/*
 * simple_theme.cxx
 *
 * copyright (2010-2014) Benoit Gschwind
 *
 * This code is licensed under the GPLv3. see COPYING file for more details.
 *
 */


#include "simple2_theme.hxx"
#include "box.hxx"
#include "color.hxx"
#include <string>
#include <algorithm>
#include <iostream>

using namespace std;

namespace page {

inline void print_cairo_status(cairo_t * cr, char const * file, int line) {
	cairo_status_t s = cairo_status(cr);
	if (s != CAIRO_STATUS_SUCCESS) {
		printf("Cairo status %s:%d = %s\n", file, line,
				cairo_status_to_string(s));
	}
}


#define CHECK_CAIRO(x) do { \
	x;\
	print_cairo_status(cr, __FILE__, __LINE__); \
} while(false)


inline void cairo_set_source_rgba(cairo_t * cr, color_t const & color) {
	CHECK_CAIRO(::cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a));
}

inline void cairo_set_source_rgb(cairo_t * cr, color_t const & color) {
	CHECK_CAIRO(::cairo_set_source_rgba(cr, color.r, color.g, color.b, 1.0));
}

rectangle simple2_theme_t::compute_notebook_bookmark_position(
		rectangle const & allocation) const {
	return rectangle(allocation.x + allocation.w - 4 * 17 - 2, allocation.y + 2,
			16, 16).floor();
}

rectangle simple2_theme_t::compute_notebook_vsplit_position(
		rectangle const & allocation) const {

	return rectangle(allocation.x + allocation.w - 3 * 17 - 2, allocation.y + 2,
			16, 16).floor();
}

rectangle simple2_theme_t::compute_notebook_hsplit_position(
		rectangle const & allocation) const {

	return rectangle(allocation.x + allocation.w - 2 * 17 - 2, allocation.y + 2,
			16, 16).floor();

}

rectangle simple2_theme_t::compute_notebook_close_position(
		rectangle const & allocation) const {

	return rectangle(allocation.x + allocation.w - 1 * 17 - 2, allocation.y + 2,
			16, 16).floor();
}

rectangle simple2_theme_t::compute_floating_close_position(rectangle const & allocation) const {
	return rectangle(allocation.w - 1 * 17 - floating_margin.right, floating_margin.bottom, 16, 16).floor();
}

rectangle simple2_theme_t::compute_floating_bind_position(
		rectangle const & allocation) const {
	return rectangle(allocation.w - 2 * 17 - floating_margin.right, floating_margin.bottom, 16, 16).floor();
}


inline string get_value_string(GKeyFile * conf, string const & group, string const & key) {
	string ret;
	gchar * value = g_key_file_get_string(conf, group.c_str(), key.c_str(), 0);
	if(value != 0) {
		ret = value;
		g_free(value);
		return ret;
	} else {
		throw runtime_error("config key missing");
	}
}

simple2_theme_t::simple2_theme_t(xconnection_t * cnx, config_handler_t & conf) {

	notebook_margin.top = 19;
	notebook_margin.bottom = 2;
	notebook_margin.left = 2;
	notebook_margin.right = 2;

	split_margin.top = 0;
	split_margin.bottom = 0;
	split_margin.left = 0;
	split_margin.right = 0;

	floating_margin.top = 32;
	floating_margin.bottom = 12;
	floating_margin.left = 12;
	floating_margin.right = 12;

	split_width = 10;

	_cnx = cnx;

	string conf_img_dir = conf.get_string("default", "theme_dir");

	default_background_color.set(
			conf.get_string("simple_theme", "default_background_color"));

	popup_text_color.set(conf.get_string("simple_theme", "popup_text_color"));
	popup_outline_color.set(
			conf.get_string("simple_theme", "popup_outline_color"));
	popup_background_color.set(
			conf.get_string("simple_theme", "popup_background_color"));

	notebook_active_text_color.set(
			conf.get_string("simple_theme", "notebook_active_text_color"));
	notebook_active_outline_color.set(
			conf.get_string("simple_theme", "notebook_active_outline_color"));
	notebook_active_border_color.set(
			conf.get_string("simple_theme", "notebook_active_border_color"));
	notebook_active_background_color.set(
			conf.get_string("simple_theme",
					"notebook_active_background_color"));

	notebook_selected_text_color.set(
			conf.get_string("simple_theme", "notebook_selected_text_color"));
	notebook_selected_outline_color.set(
			conf.get_string("simple_theme", "notebook_selected_outline_color"));
	notebook_selected_border_color.set(
			conf.get_string("simple_theme", "notebook_selected_border_color"));
	notebook_selected_background_color.set(
			conf.get_string("simple_theme",
					"notebook_selected_background_color"));

	notebook_normal_text_color.set(
			conf.get_string("simple_theme", "notebook_normal_text_color"));
	notebook_normal_outline_color.set(
			conf.get_string("simple_theme", "notebook_normal_outline_color"));
	notebook_normal_border_color.set(
			conf.get_string("simple_theme", "notebook_normal_border_color"));
	notebook_normal_background_color.set(
			conf.get_string("simple_theme",
					"notebook_normal_background_color"));

	floating_active_text_color.set(
			conf.get_string("simple_theme", "floating_active_text_color"));
	floating_active_outline_color.set(
			conf.get_string("simple_theme", "floating_active_outline_color"));
	floating_active_border_color.set(
			conf.get_string("simple_theme", "floating_active_border_color"));
	floating_active_background_color.set(
			conf.get_string("simple_theme",
					"floating_active_background_color"));

	floating_normal_text_color.set(
			conf.get_string("simple_theme", "floating_normal_text_color"));
	floating_normal_outline_color.set(
			conf.get_string("simple_theme", "floating_normal_outline_color"));
	floating_normal_border_color.set(
			conf.get_string("simple_theme", "floating_normal_border_color"));
	floating_normal_background_color.set(
			conf.get_string("simple_theme",
					"floating_normal_background_color"));

	vsplit_button_s = 0;
	hsplit_button_s = 0;
	close_button_s = 0;
	pop_button_s = 0;
	pops_button_s = 0;
	unbind_button_s = 0;
	bind_button_s = 0;

	has_background = conf.has_key("simple_theme", "background_png");
	if(has_background)
		background_file = conf.get_string("simple_theme", "background_png");
	scale_mode = "center";

	if(conf.has_key("simple_theme", "scale_mode"))
		scale_mode = conf.get_string("simple_theme", "scale_mode");

	create_background_img();

	/* open icons */
	if (hsplit_button_s == 0) {
		std::string filename = conf_img_dir + "/hsplit_button.png";
		printf("Load: %s\n", filename.c_str());
		hsplit_button_s = cairo_image_surface_create_from_png(filename.c_str());
		if (hsplit_button_s == 0)
			throw std::runtime_error("file not found!");
	}

	if (vsplit_button_s == 0) {
		std::string filename = conf_img_dir + "/vsplit_button.png";
		printf("Load: %s\n", filename.c_str());
		vsplit_button_s = cairo_image_surface_create_from_png(filename.c_str());
		if (vsplit_button_s == 0)
			throw std::runtime_error("file not found!");
	}

	if (close_button_s == 0) {
		std::string filename = conf_img_dir + "/window-close-1.png";
		printf("Load: %s\n", filename.c_str());
		close_button_s = cairo_image_surface_create_from_png(filename.c_str());
		if (close_button_s == 0)
			throw std::runtime_error("file not found!");
	}

	if (pop_button_s == 0) {
		std::string filename = conf_img_dir + "/pop.png";
		printf("Load: %s\n", filename.c_str());
		pop_button_s = cairo_image_surface_create_from_png(filename.c_str());
		if (pop_button_s == 0)
			throw std::runtime_error("file not found!");
	}

	if (pops_button_s == 0) {
		std::string filename = conf_img_dir + "/pop_selected.png";
		printf("Load: %s\n", filename.c_str());
		pops_button_s = cairo_image_surface_create_from_png(filename.c_str());
		if (pop_button_s == 0)
			throw std::runtime_error("file not found!");
	}

	if (unbind_button_s == 0) {
		std::string filename = conf_img_dir + "/media-eject.png";
		printf("Load: %s\n", filename.c_str());
		unbind_button_s = cairo_image_surface_create_from_png(filename.c_str());
		if (unbind_button_s == 0)
			throw std::runtime_error("file not found!");
	}

	if (bind_button_s == 0) {
		std::string filename = conf_img_dir + "/view-restore.png";
		printf("Load: %s\n", filename.c_str());
		bind_button_s = cairo_image_surface_create_from_png(filename.c_str());
		if (bind_button_s == 0)
			throw std::runtime_error("file not found!");
	}

	notebook_active_font = pango_font_description_from_string(conf.get_string("simple_theme", "notebook_active_font").c_str());
	notebook_selected_font = pango_font_description_from_string(conf.get_string("simple_theme", "notebook_selected_font").c_str());
	notebook_normal_font = pango_font_description_from_string(conf.get_string("simple_theme", "notebook_normal_font").c_str());

	floating_active_font = pango_font_description_from_string(conf.get_string("simple_theme", "floating_active_font").c_str());
	floating_normal_font = pango_font_description_from_string(conf.get_string("simple_theme", "floating_normal_font").c_str());

	pango_popup_font = pango_font_description_from_string(conf.get_string("simple_theme", "pango_popup_font").c_str());


}

simple2_theme_t::~simple2_theme_t() {

	if(background_s != 0) {
		cairo_surface_destroy(background_s);
		background_s = 0;
	}

	cairo_surface_destroy(hsplit_button_s);
	cairo_surface_destroy(vsplit_button_s);
	cairo_surface_destroy(close_button_s);
	cairo_surface_destroy(pop_button_s);
	cairo_surface_destroy(pops_button_s);
	cairo_surface_destroy(unbind_button_s);
	cairo_surface_destroy(bind_button_s);

	pango_font_description_free(notebook_active_font);
	pango_font_description_free(notebook_selected_font);
	pango_font_description_free(notebook_normal_font);
	pango_font_description_free(floating_active_font);
	pango_font_description_free(floating_normal_font);
	pango_font_description_free(pango_popup_font);

}

void simple2_theme_t::rounded_rectangle(cairo_t * cr, double x, double y,
		double w, double h, double r) {
	CHECK_CAIRO(cairo_save(cr));

	CHECK_CAIRO(cairo_new_path(cr));
	CHECK_CAIRO(cairo_move_to(cr, x, y + h));
	CHECK_CAIRO(cairo_line_to(cr, x, y + r));
	CHECK_CAIRO(cairo_arc(cr, x + r, y + r, r, 2.0 * (M_PI_2), 3.0 * (M_PI_2)));
	CHECK_CAIRO(cairo_move_to(cr, x + r, y));
	CHECK_CAIRO(cairo_line_to(cr, x + w - r, y));
	CHECK_CAIRO(cairo_arc(cr, x + w - r, y + r, r, 3.0 * (M_PI_2), 4.0 * (M_PI_2)));
	CHECK_CAIRO(cairo_move_to(cr, x + w, y + h));
	CHECK_CAIRO(cairo_line_to(cr, x + w, y + r));
	CHECK_CAIRO(cairo_stroke(cr));

	CHECK_CAIRO(cairo_restore(cr));
}

void simple2_theme_t::compute_areas_for_notebook(notebook_base_t const * n,
		vector<page_event_t> * l) const {

	{
		page_event_t nc(PAGE_EVENT_NOTEBOOK_CLOSE);
		nc.position = compute_notebook_close_position(n->allocation());
		nc.nbk = n;
		l->push_back(nc);

		page_event_t nhs(PAGE_EVENT_NOTEBOOK_HSPLIT);
		nhs.position = compute_notebook_hsplit_position(n->allocation());
		nhs.nbk = n;
		l->push_back(nhs);

		page_event_t nvs(PAGE_EVENT_NOTEBOOK_VSPLIT);
		nvs.position = compute_notebook_vsplit_position(n->allocation());
		nvs.nbk = n;
		l->push_back(nvs);

		page_event_t nm(PAGE_EVENT_NOTEBOOK_MARK);
		nm.position = compute_notebook_bookmark_position(n->allocation());
		nm.nbk = n;
		l->push_back(nm);
	}

	list<managed_window_base_t const *> clist = n->clients();

	if(clist.size() != 0) {
		double box_width = ((n->allocation().w - 17.0 * 5.0)
				/ (clist.size() + 1.0));
		double offset = n->allocation().x;

		rectangle b;

		for(list<managed_window_base_t const *>::iterator i = clist.begin(); i != clist.end(); ++i) {
			if ((*i) == n->selected()) {
				b = rectangle(floor(offset), n->allocation().y,
						floor(offset + 2.0 * box_width) - floor(offset),
						notebook_margin.top - 1);

				page_event_t ncclose(PAGE_EVENT_NOTEBOOK_CLIENT_CLOSE);
				ncclose.position.x = b.x + b.w - 1 * 17 - 3;
				ncclose.position.y = b.y + 4;
				ncclose.position.w = 16;
				ncclose.position.h = 16;
				ncclose.nbk = n;
				ncclose.clt = *i;
				l->push_back(ncclose);

				page_event_t ncub(PAGE_EVENT_NOTEBOOK_CLIENT_UNBIND);
				ncub.position.x = b.x + b.w - 2 * 17 - 3;
				ncub.position.y = b.y + 4;
				ncub.position.w = 16;
				ncub.position.h = 16;
				ncub.nbk = n;
				ncub.clt = *i;
				l->push_back(ncub);

				page_event_t nc(PAGE_EVENT_NOTEBOOK_CLIENT);
				nc.position = b;
				nc.nbk = n;
				nc.clt = *i;
				l->push_back(nc);

				offset += box_width * 2;

			} else {
				b = rectangle(floor(offset), n->allocation().y,
						floor(offset + box_width) - floor(offset),
						notebook_margin.top - 1);

				page_event_t nc(PAGE_EVENT_NOTEBOOK_CLIENT);
				nc.position = b;
				nc.nbk = n;
				nc.clt = *i;
				l->push_back(nc);

				offset += box_width;
			}

		}

	}
}

void simple2_theme_t::render_notebook(cairo_t * cr, notebook_base_t const * n,
		rectangle const & area) const {

	CHECK_CAIRO(cairo_save(cr));

	CHECK_CAIRO(cairo_reset_clip(cr));
	CHECK_CAIRO(cairo_identity_matrix(cr));
	CHECK_CAIRO(cairo_set_line_width(cr, 1.0));
	CHECK_CAIRO(cairo_new_path(cr));

	/** hope to optimize things **/
	CHECK_CAIRO(cairo_rectangle(cr, area.x, area.y, area.w, area.h));
	CHECK_CAIRO(cairo_clip(cr));

	/** draw background **/
	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
	CHECK_CAIRO(cairo_rectangle(cr, n->allocation().x, n->allocation().y, n->allocation().w,
			n->allocation().h));
	if (background_s != 0) {
		CHECK_CAIRO(cairo_set_source_surface(cr, background_s, 0.0, 0.0));
	} else {
		CHECK_CAIRO(cairo_set_source_rgba(cr, default_background_color));
	}
	CHECK_CAIRO(cairo_fill(cr));

	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_OVER));

	list<managed_window_base_t const *> clients = n->clients();
	vector<page_event_t> * tabs = new vector<page_event_t>();
	compute_areas_for_notebook(n, tabs);

	for (vector<page_event_t>::iterator i = tabs->begin(); i != tabs->end();
			++i) {

		/** skip useless boxes **/
		if ((*i).type == PAGE_EVENT_NOTEBOOK_CLIENT_CLOSE) {
			continue;
		} else if ((*i).type == PAGE_EVENT_NOTEBOOK_CLIENT_UNBIND) {
			continue;
		} else if (not ((*i).type == PAGE_EVENT_NOTEBOOK_CLIENT)) {
			continue;
		}

		if ((*i).clt == n->selected()) {

			if ((*i).clt->is_focused()) {
				render_notebook_selected(cr, (*i),
						notebook_active_font,
						notebook_active_text_color,
						notebook_active_outline_color,
						notebook_active_border_color,
						notebook_active_background_color, 1.0);
			} else {
				render_notebook_selected(cr, (*i),
						notebook_selected_font,
						notebook_selected_text_color,
						notebook_selected_outline_color,
						notebook_selected_border_color,
						notebook_selected_background_color, 1.0);
			}

		} else {
			render_notebook_normal(cr, (*i),
					notebook_normal_font,
					notebook_normal_text_color,
					notebook_normal_outline_color,
					notebook_normal_border_color,
					notebook_normal_background_color);
		}

	}

	delete tabs;

	CHECK_CAIRO(cairo_reset_clip(cr));
	CHECK_CAIRO(cairo_identity_matrix(cr));
	CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));

	{
		rectangle b = compute_notebook_bookmark_position(n->allocation());

		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(cairo_set_source_rgb(cr, notebook_normal_background_color));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
		//CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		if (n->is_default()) {
			CHECK_CAIRO(cairo_set_source_surface(cr, pops_button_s, b.x, b.y));
			CHECK_CAIRO(cairo_mask_surface(cr, pops_button_s, b.x, b.y));
		} else {
			CHECK_CAIRO(cairo_set_source_surface(cr, pop_button_s, b.x, b.y));
			CHECK_CAIRO(cairo_mask_surface(cr, pop_button_s, b.x, b.y));
		}

	}

	{
		rectangle b = compute_notebook_vsplit_position(n->allocation());

		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(cairo_set_source_rgb(cr, notebook_normal_background_color));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
		//CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(cairo_set_source_surface(cr, vsplit_button_s, b.x, b.y));
		CHECK_CAIRO(cairo_mask_surface(cr, vsplit_button_s, b.x, b.y));
	}

	{
		rectangle b = compute_notebook_hsplit_position(n->allocation());

		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(cairo_set_source_rgb(cr, notebook_normal_background_color));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
		//CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(cairo_set_source_surface(cr, hsplit_button_s, b.x, b.y));
		CHECK_CAIRO(cairo_mask_surface(cr, hsplit_button_s, b.x, b.y));
	}

	{
		rectangle b = compute_notebook_close_position(n->allocation());

		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(cairo_set_source_rgb(cr, notebook_normal_background_color));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
		//CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(cairo_set_source_surface(cr, close_button_s, b.x, b.y));
		CHECK_CAIRO(cairo_mask_surface(cr, close_button_s, b.x, b.y));
	}

	CHECK_CAIRO(cairo_restore(cr));

}


static void draw_notebook_border(cairo_t * cr, rectangle const & alloc,
		rectangle const & tab_area, color_t const & color,
		double border_width) {

	double half = border_width / 2.0;

	CHECK_CAIRO(simple2_theme_t::cairo_rounded_tab(cr, tab_area.x + 0.5, tab_area.y + 0.5, tab_area.w - 1.0, tab_area.h - 1.0, 7.0));

//	CHECK_CAIRO(cairo_new_path(cr));
//
//	CHECK_CAIRO(cairo_move_to(cr, tab_area.x + half, tab_area.y + tab_area.h + half));
//	/** tab left **/
//	CHECK_CAIRO(cairo_line_to(cr, tab_area.x + half, tab_area.y + half));
//	/** tab top **/
//	CHECK_CAIRO(cairo_line_to(cr, tab_area.x + tab_area.w - half, tab_area.y + half));
//	/** tab right **/
//	CHECK_CAIRO(cairo_line_to(cr, tab_area.x + tab_area.w - half,)
//			tab_area.y + tab_area.h + half);
//	/** tab bottom **/
//	CHECK_CAIRO(cairo_line_to(cr, tab_area.x + half, tab_area.y + tab_area.h + half));

	CHECK_CAIRO(cairo_set_line_width(cr, border_width));
	CHECK_CAIRO(cairo_set_source_rgba(cr, color));
	CHECK_CAIRO(cairo_stroke(cr));
}


void simple2_theme_t::render_notebook_selected(
		cairo_t * cr,
		page_event_t const & data,
		PangoFontDescription * pango_font,
		color_t const & text_color,
		color_t const & outline_color,
		color_t const & border_color,
		color_t const & background_color,
		double border_width
) const {

	notebook_base_t const * n = data.nbk;
	managed_window_base_t const * c = data.clt;
	rectangle tab_area = data.position;

	tab_area.x += 2;
	tab_area.w -= 4;
	tab_area.y += 1;
	tab_area.h -= 1;

	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_OVER));

	/** draw the tab background **/
	rectangle b = tab_area;
	CHECK_CAIRO(cairo_set_source_rgba(cr, background_color));
	CHECK_CAIRO(cairo_rounded_tab(cr, b.x + 0.5, b.y + 0.5, b.w - 1.0, b.h - 1.0, 7.0));
	//CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
	CHECK_CAIRO(cairo_fill(cr));

	/** draw application icon **/
	rectangle bicon = tab_area;
	bicon.h = 16;
	bicon.w = 16;
	bicon.x += 7;
	bicon.y += 2;

	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_OVER));
	if ((c)->icon() != 0) {
		if ((c)->icon()->get_cairo_surface() != 0) {
			CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
			CHECK_CAIRO(cairo_set_source_surface(cr, (c)->icon()->get_cairo_surface(),
					bicon.x, bicon.y));
			CHECK_CAIRO(cairo_mask_surface(cr, (c)->icon()->get_cairo_surface(),
					bicon.x, bicon.y));
		}
	}

	/** draw application title **/
	rectangle btext = tab_area;
	btext.h -= 0;
	btext.w -= 3 * 16 + 12 + 4;
	btext.x += 7 + 16 + 2;
	btext.y += 2;

	/** draw title **/
	CHECK_CAIRO(cairo_translate(cr, btext.x + 2, btext.y));

	CHECK_CAIRO(cairo_new_path(cr));
	{

		PangoLayout * pango_layout = pango_cairo_create_layout(cr);
		pango_layout_set_font_description(pango_layout, pango_font);
		pango_layout_set_wrap(pango_layout, PANGO_WRAP_CHAR);
		pango_layout_set_ellipsize(pango_layout, PANGO_ELLIPSIZE_END);
		pango_layout_set_width(pango_layout, btext.w * PANGO_SCALE);
		pango_layout_set_text(pango_layout, (c)->title(), -1);
		pango_cairo_update_layout(cr, pango_layout);
		pango_cairo_layout_path(cr, pango_layout);

		CHECK_CAIRO(cairo_set_line_width(cr, 3.0));
		CHECK_CAIRO(cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND));
		CHECK_CAIRO(cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL));
		CHECK_CAIRO(cairo_set_source_rgba(cr, outline_color));
		CHECK_CAIRO(cairo_stroke(cr));

		CHECK_CAIRO(cairo_set_line_width(cr, 1.0));
		CHECK_CAIRO(cairo_set_source_rgba(cr, text_color));
		pango_cairo_layout_path(cr, pango_layout);
		CHECK_CAIRO(cairo_fill(cr));
		g_object_unref(pango_layout);
	}

	CHECK_CAIRO(cairo_identity_matrix(cr));

	/** draw close button **/

	rectangle ncclose;
	ncclose.x = tab_area.x + tab_area.w - 1 * 17 - 4;
	ncclose.y = tab_area.y + 2;
	ncclose.w = 16;
	ncclose.h = 16;

	CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
	//CHECK_CAIRO(cairo_rectangle(cr, ncclose.x, ncclose.y, ncclose.w, ncclose.h));
	CHECK_CAIRO(cairo_set_source_surface(cr, close_button_s, ncclose.x, ncclose.y));
	CHECK_CAIRO(cairo_mask_surface(cr, close_button_s, ncclose.x, ncclose.y));

	/** draw unbind button **/
	rectangle ncub;
	ncub.x = tab_area.x + tab_area.w - 2 * 17 - 4;
	ncub.y = tab_area.y + 2;
	ncub.w = 16;
	ncub.h = 16;

	CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
	//CHECK_CAIRO(cairo_rectangle(cr, ncub.x, ncub.y, ncub.w, ncub.h));
	CHECK_CAIRO(cairo_set_source_surface(cr, unbind_button_s, ncub.x, ncub.y));
	CHECK_CAIRO(cairo_mask_surface(cr, unbind_button_s, ncub.x, ncub.y));

	rectangle area = n->allocation();
	area.y += notebook_margin.top - 4;
	area.h -= notebook_margin.top - 4;

	rectangle bclient = n->selected()->base_position();

//	/* left */
//	box_int_t bleft(area.x, area.y, bclient.x - area.x, area.h);
//	CHECK_CAIRO(cairo_set_source_rgba(cr, background_color));
//	CHECK_CAIRO(cairo_rectangle(cr, bleft.x, bleft.y, bleft.w, bleft.h));
//	CHECK_CAIRO(cairo_fill(cr));
//
//	/* right */
//
//	box_int_t bright(bclient.x + bclient.w, area.y,
//			area.x + area.w - bclient.x - bclient.w, area.h);
//	CHECK_CAIRO(cairo_set_source_rgba(cr, background_color));
//	CHECK_CAIRO(cairo_rectangle(cr, bright.x, bright.y, bright.w, bright.h));
//	CHECK_CAIRO(cairo_fill(cr));
//
//	/* top */
//
//	box_int_t btop(bclient.x, area.y, bclient.w, bclient.y - area.y);
//	CHECK_CAIRO(cairo_set_source_rgba(cr, background_color));
//	CHECK_CAIRO(cairo_rectangle(cr, btop.x, btop.y, btop.w, btop.h));
//	CHECK_CAIRO(cairo_fill(cr));
//
//	/* bottom */
//	box_int_t bbottom(bclient.x, bclient.y + bclient.h, bclient.w,
//			area.y + area.h - bclient.y - bclient.h);
//	CHECK_CAIRO(cairo_set_source_rgba(cr, background_color));
//	CHECK_CAIRO(cairo_rectangle(cr, bbottom.x, bbottom.y, bbottom.w, bbottom.h));
//	CHECK_CAIRO(cairo_fill(cr));

	/** draw border **/
	draw_notebook_border(cr, n->allocation(), tab_area, border_color, border_width);


	/** draw gradient under tab **/

	CHECK_CAIRO(cairo_save(cr));
	CHECK_CAIRO(cairo_translate(cr, tab_area.x, tab_area.y + tab_area.h));
	CHECK_CAIRO(cairo_rectangle(cr, 0.0, 0.0, tab_area.w, 100.0));
	CHECK_CAIRO(cairo_clip(cr));
	cairo_pattern_t * gradient;
	gradient = cairo_pattern_create_linear(0.0, 0.0, 0.0, 20.0);
	CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(gradient, 0.0, 1.0, 1.0, 1.0, 1.0));
	CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(gradient, 1.0, 1.0, 1.0, 1.0, 0.0));
	CHECK_CAIRO(cairo_set_source_rgba(cr, border_color));
	CHECK_CAIRO(cairo_mask(cr, gradient));
	CHECK_CAIRO(cairo_fill(cr));
	CHECK_CAIRO(cairo_pattern_destroy(gradient));
	CHECK_CAIRO(cairo_restore(cr));

}



void simple2_theme_t::render_notebook_normal(
		cairo_t * cr,
		page_event_t const & data,
		PangoFontDescription * pango_font,
		color_t const & text_color,
		color_t const & outline_color,
		color_t const & border_color,
		color_t const & background_color
) const {

	rectangle tab_area = data.position;
	tab_area.x += 2;
	tab_area.w -= 4;
	tab_area.y += 1;
	tab_area.h -= 1;


	managed_window_base_t const * c = data.clt;



	CHECK_CAIRO(cairo_save(cr));

	PangoLayout * pango_layout = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(pango_layout, pango_font);
	pango_layout_set_wrap(pango_layout, PANGO_WRAP_CHAR);
	pango_layout_set_ellipsize(pango_layout, PANGO_ELLIPSIZE_END);

	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_OVER));

	CHECK_CAIRO(cairo_set_line_width(cr, 1.0));

	/** draw the tab background **/
	rectangle b = tab_area;
	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_OVER));
	CHECK_CAIRO(cairo_set_source_rgba(cr, background_color));
	//CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
	CHECK_CAIRO(cairo_rounded_tab(cr, b.x + 0.5, b.y + 0.5, b.w - 1.0, b.h - 1.0, 7.0));

	CHECK_CAIRO(cairo_fill(cr));

	rectangle bicon = tab_area;
	bicon.h = 16;
	bicon.w = 16;
	bicon.x += 3;
	bicon.y += 2;

	CHECK_CAIRO(cairo_save(cr));
	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
	if ((c)->icon() != 0) {
		if ((c)->icon()->get_cairo_surface() != 0) {
			CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
			CHECK_CAIRO(cairo_set_source_surface(cr, (c)->icon()->get_cairo_surface(),
					bicon.x, bicon.y));
			CHECK_CAIRO(cairo_mask_surface(cr, (c)->icon()->get_cairo_surface(),
					bicon.x, bicon.y));
		}
	}
	CHECK_CAIRO(cairo_restore(cr));

	rectangle btext = tab_area;
	btext.h -= 0;
	btext.w -= 1 * 16 + 8;
	btext.x += 3 + 16 + 2;
	btext.y += 2;

	CHECK_CAIRO(cairo_save(cr));

	/* draw title */
	CHECK_CAIRO(cairo_set_source_rgba(cr, outline_color));

	CHECK_CAIRO(cairo_translate(cr, btext.x + 2, btext.y));

	CHECK_CAIRO(cairo_new_path(cr));
	pango_layout_set_width(pango_layout, btext.w * PANGO_SCALE);
	pango_layout_set_text(pango_layout, (c)->title(), -1);
	pango_cairo_update_layout(cr, pango_layout);
	pango_cairo_layout_path(cr, pango_layout);

	CHECK_CAIRO(cairo_set_line_width(cr, 3.0));
	CHECK_CAIRO(cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND));
	CHECK_CAIRO(cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL));

	CHECK_CAIRO(cairo_stroke(cr));

	CHECK_CAIRO(cairo_set_line_width(cr, 1.0));
	CHECK_CAIRO(cairo_set_source_rgba(cr, text_color));
	pango_cairo_layout_path(cr, pango_layout);
	CHECK_CAIRO(cairo_fill(cr));

	CHECK_CAIRO(cairo_restore(cr));

	{
		rectangle b = tab_area;

//		CHECK_CAIRO(cairo_new_path(cr));
//		CHECK_CAIRO(cairo_move_to(cr, b.x + 0.5, b.y + b.h + 0.5));
//		CHECK_CAIRO(cairo_line_to(cr, b.x + 0.5, b.y + 0.5));
//		CHECK_CAIRO(cairo_line_to(cr, b.x + b.w - 0.5, b.y + 0.5));
//		CHECK_CAIRO(cairo_line_to(cr, b.x + b.w - 0.5, b.y + b.h + 0.5));
//		CHECK_CAIRO(cairo_line_to(cr, b.x + 0.5, b.y + b.h + 0.5));
//
		CHECK_CAIRO(cairo_rounded_tab(cr, b.x + 0.5, b.y + 0.5, b.w - 1.0, b.h - 1.0, 7.0));
		CHECK_CAIRO(cairo_set_source_rgba(cr, border_color));
		CHECK_CAIRO(cairo_stroke(cr));
	}


	/** draw gradient under tabs **/
	CHECK_CAIRO(cairo_save(cr));
	CHECK_CAIRO(cairo_translate(cr, tab_area.x, tab_area.y + tab_area.h));
	CHECK_CAIRO(cairo_rectangle(cr, 0.0, 0.0, tab_area.w, 100.0));
	CHECK_CAIRO(cairo_clip(cr));
	cairo_pattern_t * gradient;
	gradient = cairo_pattern_create_linear(0.0, 0.0, 0.0, 20.0);
	CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(gradient, 0.0, 1.0, 1.0, 1.0, 1.0));
	CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(gradient, 1.0, 0.0, 0.0, 0.0, 0.0));
	CHECK_CAIRO(cairo_set_source_rgba(cr, border_color));
	CHECK_CAIRO(cairo_mask(cr, gradient));
	CHECK_CAIRO(cairo_fill(cr));
	CHECK_CAIRO(cairo_pattern_destroy(gradient));
	CHECK_CAIRO(cairo_restore(cr));

	g_object_unref(pango_layout);
	CHECK_CAIRO(cairo_restore(cr));



}

void simple2_theme_t::render_split(cairo_t * cr, split_base_t const * s,
		rectangle const & area) const {
	CHECK_CAIRO(cairo_save(cr));
	CHECK_CAIRO(cairo_reset_clip(cr));
	CHECK_CAIRO(cairo_identity_matrix(cr));
	CHECK_CAIRO(cairo_set_line_width(cr, 1.0));

	CHECK_CAIRO(cairo_rectangle(cr, area.x, area.y, area.w, area.h));
	CHECK_CAIRO(cairo_clip(cr));

	rectangle sarea = compute_split_bar_location(s);
	if (background_s != 0) {
		CHECK_CAIRO(cairo_set_source_surface(cr, background_s, 0.0, 0.0));
	} else {
		CHECK_CAIRO(cairo_set_source_rgba(cr, default_background_color));
	}
	CHECK_CAIRO(cairo_rectangle(cr, sarea.x, sarea.y, sarea.w, sarea.h));
	CHECK_CAIRO(cairo_fill(cr));
	CHECK_CAIRO(cairo_restore(cr));
}



void simple2_theme_t::render_floating(managed_window_base_t * mw) const {

	if (mw->is_focused()) {
		render_floating_base(mw,
				floating_active_font,
				floating_active_text_color,
				floating_active_outline_color, floating_active_border_color,
				floating_active_background_color, 1.0);
	} else {
		render_floating_base(mw,
				floating_normal_font,
				floating_normal_text_color,
				floating_normal_outline_color, floating_normal_border_color,
				floating_normal_background_color, 1.0);
	}
}


void simple2_theme_t::render_floating_base(
		managed_window_base_t * mw,
		PangoFontDescription * pango_font,
		color_t const & text_color,
		color_t const & outline_color,
		color_t const & border_color,
		color_t const & background_color,
		double border_width
) const {

	rectangle allocation = mw->base_position();

	{
		cairo_t * cr = mw->cairo_top();
		PangoLayout * pango_layout = pango_cairo_create_layout(cr);
		pango_layout_set_font_description(pango_layout, pango_font);

		rectangle b(0, 0, allocation.w, floating_margin.top);

		CHECK_CAIRO(cairo_reset_clip(cr));
		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * grad0 = cairo_pattern_create_linear(b.x, 0.0, b.x + floating_margin.left, 0.0);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad0, 0.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad0, 1.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y + floating_margin.bottom, floating_margin.left, floating_margin.top - floating_margin.bottom));
		CHECK_CAIRO(cairo_set_source(cr, grad0));
		CHECK_CAIRO(cairo_fill(cr));
		CHECK_CAIRO(cairo_pattern_destroy(grad0));

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * grad1 = cairo_pattern_create_linear(b.x + b.w - floating_margin.right, 0.0, b.x + b.w, 0.0);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad1, 0.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad1, 1.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_rectangle(cr, b.x + b.w - floating_margin.right, b.y + floating_margin.bottom, floating_margin.right, floating_margin.top - floating_margin.bottom));
		CHECK_CAIRO(cairo_set_source(cr, grad1));
		CHECK_CAIRO(cairo_fill(cr));
		CHECK_CAIRO(cairo_pattern_destroy(grad1));


		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * grad2 = cairo_pattern_create_linear(0.0, b.y, 0.0, b.y + floating_margin.bottom);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad2, 0.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad2, 1.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_rectangle(cr, b.x + floating_margin.left, b.y, b.w - floating_margin.right - floating_margin.left, floating_margin.bottom));
		CHECK_CAIRO(cairo_set_source(cr, grad2));
		CHECK_CAIRO(cairo_fill(cr));
		CHECK_CAIRO(cairo_pattern_destroy(grad2));

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * r0grad = cairo_pattern_create_radial(b.x + floating_margin.left, b.y + floating_margin.bottom, 0.0, b.x + floating_margin.left, b.y + floating_margin.bottom, floating_margin.left);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(r0grad, 0.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(r0grad, 1.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_set_source(cr, r0grad));
		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, floating_margin.left, floating_margin.bottom));
		CHECK_CAIRO(cairo_fill(cr));
		CHECK_CAIRO(cairo_pattern_destroy(r0grad));

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * r1grad = cairo_pattern_create_radial(b.x + b.w - floating_margin.right, b.y + floating_margin.bottom, 0.0, b.x + b.w - floating_margin.right, b.y + floating_margin.bottom, floating_margin.right);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(r1grad, 0.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(r1grad, 1.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_set_source(cr, r1grad));
		CHECK_CAIRO(cairo_rectangle(cr, b.x + b.w - floating_margin.right, b.y, floating_margin.right, floating_margin.bottom));
		CHECK_CAIRO(cairo_fill(cr));
		CHECK_CAIRO(cairo_pattern_destroy(r1grad));

		rectangle b1(b.x + floating_margin.left, b.y + floating_margin.bottom, b.w - floating_margin.left - floating_margin.right, b.h - floating_margin.bottom);

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		CHECK_CAIRO(cairo_rectangle(cr, b1.x, b1.y, b1.w, b1.h));
		//CHECK_CAIRO(cairo_rounded_tab(cr, b.x, b.y, b.w, b.h, 7.0));
		CHECK_CAIRO(cairo_set_source_rgba(cr, background_color));
		CHECK_CAIRO(cairo_fill(cr));

		rectangle bicon = b1;
		bicon.h = 16;
		bicon.w = 16;
		bicon.x += 2;
		bicon.y += 2;

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_OVER));

		if (mw->icon() != 0) {
			if (mw->icon()->get_cairo_surface() != 0) {
				CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
				CHECK_CAIRO(cairo_set_source_surface(cr,
						mw->icon()->get_cairo_surface(), bicon.x, bicon.y));
				CHECK_CAIRO(cairo_mask_surface(cr,
						mw->icon()->get_cairo_surface(), bicon.x, bicon.y));
			}
		}

		rectangle bbind = compute_floating_bind_position(b);
		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
		//CHECK_CAIRO(cairo_rectangle(cr, bbind.x, bbind.y, bbind.w, bbind.h));
		CHECK_CAIRO(cairo_set_source_surface(cr, bind_button_s, bbind.x, bbind.y));
		CHECK_CAIRO(cairo_mask_surface(cr, bind_button_s, bbind.x, bbind.y));

		rectangle bclose = compute_floating_close_position(b);
		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
		//CHECK_CAIRO(cairo_rectangle(cr, bclose.x, bclose.y, bclose.w, bclose.h));
		CHECK_CAIRO(cairo_set_source_surface(cr, close_button_s, bclose.x, bclose.y));
		CHECK_CAIRO(cairo_mask_surface(cr, close_button_s, bclose.x, bclose.y));

		rectangle btext = b1;
		btext.h -= 4;
		btext.w -= 3 * 16 - 8;
		btext.x += 8 + 16 + 2;
		btext.y += 2;

		CHECK_CAIRO(cairo_reset_clip(cr));

		CHECK_CAIRO(cairo_save(cr));
		//CHECK_CAIRO(cairo_new_path(cr));

		CHECK_CAIRO(cairo_set_font_size(cr, 13));

		/* draw selectected title */
		CHECK_CAIRO(cairo_translate(cr, btext.x + 2, btext.y));

		CHECK_CAIRO(cairo_new_path(cr));
		pango_layout_set_text(pango_layout, mw->title(), -1);
		pango_cairo_update_layout(cr, pango_layout);
		pango_cairo_layout_path(cr, pango_layout);

		CHECK_CAIRO(cairo_set_line_width(cr, 3.0));
		CHECK_CAIRO(cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND));
		CHECK_CAIRO(cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL));
		CHECK_CAIRO(cairo_set_source_rgba(cr, outline_color));
		CHECK_CAIRO(cairo_stroke(cr));

		CHECK_CAIRO(cairo_set_line_width(cr, 1.0));
		CHECK_CAIRO(cairo_set_source_rgba(cr, text_color));
		pango_cairo_layout_path(cr, pango_layout);
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(cairo_restore(cr));
		CHECK_CAIRO(cairo_reset_clip(cr));

//		CHECK_CAIRO(cairo_new_path(cr));
//		CHECK_CAIRO(cairo_move_to(cr, b.x + 1.0, b.y + b.h));
//		CHECK_CAIRO(cairo_line_to(cr, b.x + 1.0, b.y + 1.0));
//		CHECK_CAIRO(cairo_line_to(cr, b.w - 1.0, b.y + 1.0));
//		CHECK_CAIRO(cairo_line_to(cr, b.w - 1.0, b.y + b.h));
//		CHECK_CAIRO(cairo_set_line_width(cr, 2.0));
//		CHECK_CAIRO(cairo_set_source_rgba(cr, border_color));
//		CHECK_CAIRO(cairo_stroke(cr));

		g_object_unref(pango_layout);
		cairo_destroy(cr);
	}

	{
		cairo_t * cr = mw->cairo_bottom();

		rectangle b(0, 0, allocation.w, floating_margin.bottom);

		/** clear **/
		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_fill(cr));


		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * grad = cairo_pattern_create_linear(0.0, 0.0, 0.0, floating_margin.bottom);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad, 0.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad, 1.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_set_source(cr, grad));
		CHECK_CAIRO(cairo_rectangle(cr, b.x + floating_margin.left, b.y, b.w - floating_margin.left - floating_margin.right, b.h));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(cairo_pattern_destroy(grad));

		CHECK_CAIRO(cairo_reset_clip(cr));

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * r0grad = cairo_pattern_create_radial(b.x + floating_margin.left, b.y, 0.0, b.x + floating_margin.left, b.y, floating_margin.left);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(r0grad, 0.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(r0grad, 1.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_set_source(cr, r0grad));
		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, floating_margin.left, floating_margin.bottom));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(cairo_pattern_destroy(r0grad));

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * r1grad = cairo_pattern_create_radial(b.x + b.w - floating_margin.right, b.y, 0.0, b.x + b.w - floating_margin.right, b.y, floating_margin.right);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(r1grad, 0.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(r1grad, 1.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_set_source(cr, r1grad));
		CHECK_CAIRO(cairo_rectangle(cr, b.x + b.w - floating_margin.right, b.y, floating_margin.left, floating_margin.bottom));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(cairo_pattern_destroy(r1grad));

		cairo_destroy(cr);

	}

	{
		cairo_t * cr = mw->cairo_right();

		rectangle b(0, 0, floating_margin.right,
				allocation.h - floating_margin.top
						- floating_margin.bottom);

		/** clear **/
		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_fill(cr));


		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * grad = cairo_pattern_create_linear(0.0, 0.0, floating_margin.right, 0.0);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad, 0.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad, 1.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(cairo_set_source(cr, grad));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(cairo_pattern_destroy(grad));

		cairo_destroy(cr);
	}

	{
		cairo_t * cr = mw->cairo_left();

		rectangle b(0, 0, floating_margin.left,
				allocation.h - floating_margin.top
						- floating_margin.bottom);

		/** clear **/
		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		cairo_pattern_t * grad = cairo_pattern_create_linear(0.0, 0.0, floating_margin.left, 0.0);
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad, 0.0, 0.0, 0.0, 0.0, 0.0));
		CHECK_CAIRO(cairo_pattern_add_color_stop_rgba(grad, 1.0, 0.0, 0.0, 0.0, 0.5));
		CHECK_CAIRO(cairo_rectangle(cr, b.x, b.y, b.w, b.h));
		CHECK_CAIRO(cairo_set_source(cr, grad));
		CHECK_CAIRO(cairo_fill(cr));

		CHECK_CAIRO(cairo_pattern_destroy(grad));


		cairo_destroy(cr);

	}

}

void simple2_theme_t::render_popup_notebook0(cairo_t * cr, window_icon_handler_t * icon, unsigned int width,
		unsigned int height, string const & title) {

	PangoLayout * pango_layout = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(pango_layout, pango_popup_font);

	/** clear popup **/
	CHECK_CAIRO(cairo_reset_clip(cr));
	CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0));
	CHECK_CAIRO(cairo_paint(cr));

	CHECK_CAIRO(cairo_set_source_rgba(cr, popup_background_color));
	//CHECK_CAIRO(cairo_rectangle(cr, 1, 1, width - 2, height - 2));
	//CHECK_CAIRO(cairo_stroke(cr));


	draw_hatched_rectangle(cr, 40, 1, 1, width - 2, height - 2);


	if (icon != 0) {
		double x_offset = (width - 64) / 2.0;
		double y_offset = (height - 64) / 2.0;

		CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
		CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0));
		CHECK_CAIRO(cairo_set_source_surface(cr, icon->get_cairo_surface(), x_offset, y_offset));
		CHECK_CAIRO(cairo_mask_surface(cr, icon->get_cairo_surface(), x_offset, y_offset));


		/* draw tittle */
		CHECK_CAIRO(cairo_translate(cr, 0.0, y_offset + 68.0));

		CHECK_CAIRO(cairo_new_path(cr));
		pango_cairo_update_layout(cr, pango_layout);
		pango_layout_set_text(pango_layout, title.c_str(), -1);
		pango_layout_set_width(pango_layout, width * PANGO_SCALE);
		pango_layout_set_alignment(pango_layout, PANGO_ALIGN_CENTER);
		pango_layout_set_wrap(pango_layout, PANGO_WRAP_CHAR);
		pango_cairo_layout_path(cr, pango_layout);

		CHECK_CAIRO(cairo_set_line_width(cr, 5.0));
		CHECK_CAIRO(cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND));
		CHECK_CAIRO(cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL));

		CHECK_CAIRO(cairo_set_source_rgba(cr, popup_outline_color));

		CHECK_CAIRO(cairo_stroke_preserve(cr));

		CHECK_CAIRO(cairo_set_line_width(cr, 1.0));
		CHECK_CAIRO(cairo_set_source_rgba(cr, popup_text_color));
		CHECK_CAIRO(cairo_fill(cr));


	}

	g_object_unref(pango_layout);

}

void simple2_theme_t::render_popup_move_frame(cairo_t * cr, window_icon_handler_t * icon, unsigned int width,
		unsigned int height, string const & title) {
	render_popup_notebook0(cr, icon, width, height, title);
}

void simple2_theme_t::draw_hatched_rectangle(cairo_t * cr, int space, int x, int y, int w, int h) const {

	CHECK_CAIRO(cairo_save(cr));
	int left_bound = x;
	int right_bound = x + w;
	int top_bound = y;
	int bottom_bound = y + h;

	int count = (right_bound - left_bound) / space + (bottom_bound - top_bound) / space;

	CHECK_CAIRO(cairo_new_path(cr));

	for(int i = 0; i <= count; ++i) {

		/** clip left/right **/

		int x0 = left_bound;
		int y0 = top_bound + space * i;
		int x1 = right_bound;
		int y1 = y0 - (right_bound - left_bound);

		/** clip top **/
		if(y1 < top_bound) {
			y1 = top_bound;
			x1 = left_bound + (y0 - top_bound);
		}

		/** clip bottom **/
		if(y0 > bottom_bound) {
			x0 = left_bound + (y0 - (bottom_bound - top_bound));
			y0 = bottom_bound;
		}

		CHECK_CAIRO(cairo_move_to(cr, x0, y0));
		CHECK_CAIRO(cairo_line_to(cr, x1, y1));

	}

//	CHECK_CAIRO(cairo_set_source_rgb(cr, 1.0, 0.0, 0.0));
	CHECK_CAIRO(cairo_set_line_width(cr, 1.0));
	CHECK_CAIRO(cairo_stroke(cr));

	CHECK_CAIRO(cairo_rectangle(cr, left_bound, top_bound, right_bound - left_bound, bottom_bound - top_bound));
//	CHECK_CAIRO(cairo_set_source_rgb(cr, 1.0, 0.0, 0.0));
	CHECK_CAIRO(cairo_set_line_width(cr, 3.0));
	CHECK_CAIRO(cairo_stroke(cr));

	CHECK_CAIRO(cairo_restore(cr));

}

void simple2_theme_t::update() {

	if(background_s != 0) {
		cairo_surface_destroy(background_s);
		background_s = 0;
	}

	create_background_img();

}

void simple2_theme_t::create_background_img() {

	if (has_background) {

		cairo_surface_t * tmp = cairo_image_surface_create_from_png(
				background_file.c_str());

		XWindowAttributes wa;
		XGetWindowAttributes(_cnx->dpy, DefaultRootWindow(_cnx->dpy), &wa);

		background_s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, wa.width,
				wa.height);

		/**
		 * WARNING: transform order and set_source_surface have huge
		 * Consequence.
		 **/

		double src_width = cairo_image_surface_get_width(tmp);
		double src_height = cairo_image_surface_get_height(tmp);

		if (src_width > 0 and src_height > 0) {


			if (scale_mode == "stretch") {
				cairo_t * cr = cairo_create(background_s);

				CHECK_CAIRO(::cairo_set_source_rgb(cr, 0.5, 0.5, 0.5));
				CHECK_CAIRO(cairo_rectangle(cr, 0, 0, wa.width, wa.height));
				CHECK_CAIRO(cairo_fill(cr));

				double x_ratio = wa.width / src_width;
				double y_ratio = wa.height / src_height;
				CHECK_CAIRO(cairo_scale(cr, x_ratio, y_ratio));
				CHECK_CAIRO(cairo_set_source_surface(cr, tmp, 0, 0));
				CHECK_CAIRO(cairo_rectangle(cr, 0, 0, src_width, src_height));
				CHECK_CAIRO(cairo_fill(cr));

				CHECK_CAIRO(cairo_destroy(cr));

			} else if (scale_mode == "zoom") {
				cairo_t * cr = cairo_create(background_s);

				CHECK_CAIRO(::cairo_set_source_rgb(cr, 0.5, 0.5, 0.5));
				CHECK_CAIRO(cairo_rectangle(cr, 0, 0, wa.width, wa.height));
				CHECK_CAIRO(cairo_fill(cr));

				double x_ratio = wa.width / (double)src_width;
				double y_ratio = wa.height / (double)src_height;

				double x_offset;
				double y_offset;

				if (x_ratio > y_ratio) {

					double yp = wa.height / x_ratio;

					x_offset = 0;
					y_offset = (yp - src_height) / 2.0;

					CHECK_CAIRO(cairo_scale(cr, x_ratio, x_ratio));
					CHECK_CAIRO(cairo_set_source_surface(cr, tmp, x_offset, y_offset));
					CHECK_CAIRO(cairo_rectangle(cr, 0, 0, src_width, yp));
					CHECK_CAIRO(cairo_fill(cr));

				} else {

					double xp = wa.width / y_ratio;

					x_offset = (xp - src_width) / 2.0;
					y_offset = 0;

					CHECK_CAIRO(cairo_scale(cr, y_ratio, y_ratio));
					CHECK_CAIRO(cairo_set_source_surface(cr, tmp, x_offset, y_offset));
					CHECK_CAIRO(cairo_rectangle(cr, 0, 0, xp, src_height));
					CHECK_CAIRO(cairo_fill(cr));
				}

				cairo_destroy(cr);

			} else if (scale_mode == "center") {
				cairo_t * cr = cairo_create(background_s);

				CHECK_CAIRO(::cairo_set_source_rgb(cr, 0.5, 0.5, 0.5));
				CHECK_CAIRO(cairo_rectangle(cr, 0, 0, wa.width, wa.height));
				CHECK_CAIRO(cairo_fill(cr));

				double x_offset = (wa.width - src_width) / 2.0;
				double y_offset = (wa.height - src_height) / 2.0;

				CHECK_CAIRO(cairo_set_source_surface(cr, tmp, x_offset, y_offset));
				CHECK_CAIRO(cairo_rectangle(cr, max<double>(0.0, x_offset),
						max<double>(0.0, y_offset),
						min<double>(src_width, wa.width),
						min<double>(src_height, wa.height)));
				CHECK_CAIRO(cairo_fill(cr));

				CHECK_CAIRO(cairo_destroy(cr));

			} else if (scale_mode == "scale" || scale_mode == "span") {
				cairo_t * cr = cairo_create(background_s);

				CHECK_CAIRO(::cairo_set_source_rgb(cr, 0.5, 0.5, 0.5));
				CHECK_CAIRO(cairo_rectangle(cr, 0, 0, wa.width, wa.height));
				CHECK_CAIRO(cairo_fill(cr));

				double x_ratio = wa.width / src_width;
				double y_ratio = wa.height / src_height;

				double x_offset, y_offset;

				if (x_ratio < y_ratio) {

					double yp = wa.height / x_ratio;

					x_offset = 0;
					y_offset = (yp - src_height) / 2.0;

					CHECK_CAIRO(cairo_scale(cr, x_ratio, x_ratio));
					CHECK_CAIRO(cairo_set_source_surface(cr, tmp, x_offset, y_offset));
					CHECK_CAIRO(cairo_rectangle(cr, x_offset, y_offset, src_width, src_height));
					CHECK_CAIRO(cairo_fill(cr));

				} else {
					double xp = wa.width / y_ratio;

					y_offset = 0;
					x_offset = (xp - src_width) / 2.0;

					CHECK_CAIRO(cairo_scale(cr, y_ratio, y_ratio));
					CHECK_CAIRO(cairo_set_source_surface(cr, tmp, x_offset, y_offset));
					CHECK_CAIRO(cairo_rectangle(cr, x_offset, y_offset, src_width, src_height));
					CHECK_CAIRO(cairo_fill(cr));
				}

				CHECK_CAIRO(cairo_destroy(cr));

			} else if (scale_mode == "tile") {

				cairo_t * cr = cairo_create(background_s);
				CHECK_CAIRO(::cairo_set_source_rgb(cr, 0.5, 0.5, 0.5));
				CHECK_CAIRO(cairo_rectangle(cr, 0, 0, wa.width, wa.height));
				CHECK_CAIRO(cairo_fill(cr));

				for (double x = 0; x < wa.width; x += src_width) {
					for (double y = 0; y < wa.height; y += src_height) {
						CHECK_CAIRO(cairo_identity_matrix(cr));
						CHECK_CAIRO(cairo_translate(cr, x, y));
						CHECK_CAIRO(cairo_set_source_surface(cr, tmp, 0, 0));
						CHECK_CAIRO(cairo_rectangle(cr, 0, 0, wa.width, wa.height));
						CHECK_CAIRO(cairo_fill(cr));
					}
				}

				CHECK_CAIRO(cairo_destroy(cr));

			}

		}
		cairo_surface_destroy(tmp);

	} else {
		background_s = 0;
	}
}

vector<page_event_t> * simple2_theme_t::compute_page_areas(
		list<tree_t const *> const & page) const {

	vector<page_event_t> * ret = new vector<page_event_t>();

	for (list<tree_t const *>::const_iterator i = page.begin();
			i != page.end(); ++i) {

		if(dynamic_cast<split_base_t const *>(*i)) {
			split_base_t const * s = dynamic_cast<split_base_t const *>(*i);
			page_event_t bsplit(PAGE_EVENT_SPLIT);
			bsplit.position = compute_split_bar_location(s);

			if(s->type() == VERTICAL_SPLIT) {
				bsplit.position.w += notebook_margin.right + notebook_margin.left;
				bsplit.position.x -= notebook_margin.right;
			} else {
				bsplit.position.h += notebook_margin.bottom;
				bsplit.position.y -= notebook_margin.bottom;
			}

			bsplit.spt = s;
			ret->push_back(bsplit);
		} else if (dynamic_cast<notebook_base_t const *>(*i)) {
			notebook_base_t const * n = dynamic_cast<notebook_base_t const *>(*i);
			compute_areas_for_notebook(n, ret);
		} else if (dynamic_cast<viewport_base_t const *>(*i)) {
			/** nothing to do **/
		}
	}

	return ret;
}

vector<floating_event_t> * simple2_theme_t::compute_floating_areas(
		managed_window_base_t * mw) const {

	vector<floating_event_t> * ret = new vector<floating_event_t>();

	floating_event_t fc(FLOATING_EVENT_CLOSE);
	fc.position = compute_floating_close_position(mw->base_position());
	ret->push_back(fc);

	floating_event_t fb(FLOATING_EVENT_BIND);
	fb.position = compute_floating_bind_position(mw->base_position());
	ret->push_back(fb);

	int x0 = floating_margin.left;
	int x1 = mw->base_position().w - floating_margin.right;

	int y0 = floating_margin.bottom;
	int y1 = mw->base_position().h - floating_margin.bottom;

	int w0 = mw->base_position().w - floating_margin.left
			- floating_margin.right;
	int h0 = mw->base_position().h - floating_margin.bottom
			- floating_margin.bottom;

	floating_event_t ft(FLOATING_EVENT_TITLE);
	ft.position = rectangle(x0, y0, w0,
			floating_margin.top - floating_margin.bottom);
	ret->push_back(ft);

	floating_event_t fgt(FLOATING_EVENT_GRIP_TOP);
	fgt.position = rectangle(x0, 0, w0, floating_margin.bottom);
	ret->push_back(fgt);

	floating_event_t fgb(FLOATING_EVENT_GRIP_BOTTOM);
	fgb.position = rectangle(x0, y1, w0, floating_margin.bottom);
	ret->push_back(fgb);

	floating_event_t fgl(FLOATING_EVENT_GRIP_LEFT);
	fgl.position = rectangle(0, y0, floating_margin.left, h0);
	ret->push_back(fgl);

	floating_event_t fgr(FLOATING_EVENT_GRIP_RIGHT);
	fgr.position = rectangle(x1, y0, floating_margin.right, h0);
	ret->push_back(fgr);

	floating_event_t fgtl(FLOATING_EVENT_GRIP_TOP_LEFT);
	fgtl.position = rectangle(0, 0, floating_margin.left,
			floating_margin.bottom);
	ret->push_back(fgtl);

	floating_event_t fgtr(FLOATING_EVENT_GRIP_TOP_RIGHT);
	fgtr.position = rectangle(x1, 0, floating_margin.right,
			floating_margin.bottom);
	ret->push_back(fgtr);

	floating_event_t fgbl(FLOATING_EVENT_GRIP_BOTTOM_LEFT);
	fgbl.position = rectangle(0, y1, floating_margin.left,
			floating_margin.bottom);
	ret->push_back(fgbl);

	floating_event_t fgbr(FLOATING_EVENT_GRIP_BOTTOM_RIGHT);
	fgbr.position = rectangle(x1, y1, floating_margin.right,
			floating_margin.bottom);
	ret->push_back(fgbr);

	return ret;

}


void simple2_theme_t::render_popup_split(cairo_t * cr, unsigned int width, unsigned int height) {

	CHECK_CAIRO(cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE));
	CHECK_CAIRO(::cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 0.3));
	CHECK_CAIRO(cairo_rectangle(cr, 0, 0, width, height));
	CHECK_CAIRO(cairo_fill(cr));

	CHECK_CAIRO(::cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 1.0));
	CHECK_CAIRO(cairo_new_path(cr));

//	if(width > height) {
//		CHECK_CAIRO(cairo_move_to(cr, 0.0, height / 2));
//		CHECK_CAIRO(cairo_line_to(cr, width, height / 2));
//
//		CHECK_CAIRO(cairo_move_to(cr, 1.0, 1.0));
//		CHECK_CAIRO(cairo_line_to(cr, 1.0, height - 1.0));
//
//		CHECK_CAIRO(cairo_move_to(cr, width - 1.0, 1.0));
//		CHECK_CAIRO(cairo_line_to(cr, width - 1.0, height));
//
//	} else {
//		CHECK_CAIRO(cairo_move_to(cr, width / 2, 1.0));
//		CHECK_CAIRO(cairo_line_to(cr, width / 2, height - 1.0));
//
//		CHECK_CAIRO(cairo_move_to(cr, 1.0, 1.0));
//		CHECK_CAIRO(cairo_line_to(cr, width - 1.0, 1.0));
//
//		CHECK_CAIRO(cairo_move_to(cr, 1.0, height - 1.0));
//		CHECK_CAIRO(cairo_line_to(cr, width - 1.0, height - 1.0));
//
//	}

	CHECK_CAIRO(cairo_set_line_width(cr, 2.0));
	CHECK_CAIRO(cairo_stroke(cr));
}

void simple2_theme_t::cairo_rounded_tab(cairo_t * cr, double x, double y, double w, double h, double radius) {

	CHECK_CAIRO(cairo_new_path(cr));
	CHECK_CAIRO(cairo_move_to(cr, x, y + h));
	CHECK_CAIRO(cairo_line_to(cr, x, y + radius));
	CHECK_CAIRO(cairo_arc(cr, x + radius, y + radius, radius, 2.0 * M_PI_2, 3.0 * M_PI_2));
	CHECK_CAIRO(cairo_line_to(cr, x + w - radius, y));
	CHECK_CAIRO(cairo_arc(cr, x + w - radius, y + radius, radius, 3.0 * M_PI_2, 4.0 * M_PI_2));
	CHECK_CAIRO(cairo_line_to(cr, x + w, y + h));
	CHECK_CAIRO(cairo_close_path(cr));


}


}


//page::theme_t * get_theme(config_handler_t * conf) {
//	return new page::simple2_theme_t(*conf);
//}