/*
 * main.cxx
 *
 *  Created on: Feb 25, 2011
 *      Author: gschwind
 */

#include "execinfo.h"
#include "stdint.h"
#include "ftrace_function.hxx"
#include "page.hxx"




int main(int argc, char * * argv) {

	signal(SIGSEGV, sig_handler);
	signal(SIGABRT, sig_handler);

	page::page_t * m = new page::page_t(argc, argv);
	m->run();
	delete m;
	return 0;
}
