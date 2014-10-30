/*
 * notebook_base.hxx
 *
 *  Created on: 10 mai 2014
 *      Author: gschwind
 */

#ifndef THEME_NOTEBOOK_HXX_
#define THEME_NOTEBOOK_HXX_

namespace page {

struct theme_notebook_t {
	i_rect allocation;
	i_rect client_position;
	std::vector<theme_tab_t> clients_tab;
	bool is_default;
};

}


#endif /* NOTEBOOK_BASE_HXX_ */
