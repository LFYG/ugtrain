/* parser.cpp:    parsing functions to read in the config file
 *
 * Copyright (c) 2012, by:      Sebastian Riemer
 *    All rights reserved.      Ernst-Reinke-Str. 23
 *                              10369 Berlin, Germany
 *                             <sebastian.riemer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <stdlib.h>
#include <cstring>
#include "parser.h"
using namespace std;


#define CFG_DIR "."

enum {
	NAME_REGULAR,
	NAME_CHECK,
	NAME_DYNMEM_START,
	NAME_DYNMEM_END
};

static inline void proc_name_err (string *line, u32 lidx)
{
	cerr << "First line doesn't contain a valid process name!" << endl;
	cerr << string(*line, 0, lidx) << "<--" << endl;
	exit(-1);
}

static inline void cfg_parse_err (string *line, u32 lnr, u32 lidx)
{
	cerr << "Error while parsing config (line " << ++lnr << ")!" << endl;
	cerr << string(*line, 0, lidx) << "<--" << endl;
	exit(-1);
}

static void read_config_vect (string *path, vector<string> *lines)
{
	ifstream cfg_file;
	string line;

	cout << "Loading config file \"" << *path << "\"." << endl;
	cfg_file.open(path->c_str());
	if (!cfg_file.is_open()) {
		cerr << "File \"" << *path << "\" doesn't exist!" << endl;
		exit(-1);
	}
	while (cfg_file.good()) {
		getline(cfg_file, line);
		lines->push_back(line);
	}
	cfg_file.close();
}

static string parse_proc_name (string *line, u32 *start)
{
	u32 lidx;

	if (line->length() == 0)
		proc_name_err(line, 0);

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (!isalnum(line->at(lidx)) &&
		    line->at(lidx) != '.' &&
		    line->at(lidx) != '-' &&
		    line->at(lidx) != '_')
			proc_name_err(line, lidx);
	}
	*start = lidx;
	return *line;
}

static string parse_value_name (string *line, u32 lnr, u32 *start,
				i32 *name_type)
{
	u32 lidx;
	string ret;

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			break;
		} else if (!isalnum(line->at(lidx)) &&
		    line->at(lidx) != '-' &&
		    line->at(lidx) != '_') {
			cfg_parse_err(line, lnr, lidx);
		}
	}

	ret = string(*line, *start, lidx - *start);
	*start = lidx + 1;
	if (ret == "check")
		*name_type = NAME_CHECK;
	else if (ret == "dynmemstart")
		*name_type = NAME_DYNMEM_START;
	else if (ret == "dynmemend")
		*name_type = NAME_DYNMEM_END;
	else
		*name_type = NAME_REGULAR;

	return ret;
}

static void *parse_address (string *line, u32 lnr, u32 *start)
{
	u32 lidx;
	void *ret = NULL;

	lidx = *start;
	if (lidx + 2 > line->length() || line->at(lidx) != '0' ||
	    line->at(lidx + 1) != 'x')
		cfg_parse_err(line, lnr, --lidx);
	*start = lidx + 2;
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (lidx == line->length() - 1) {
			ret = (void *) strtol(string(*line, *start,
				lidx + 1 - *start).c_str(), NULL, 16);
			break;
		} else if (line->at(lidx) == ' ') {
			ret = (void *) strtol(string(*line, *start,
				lidx - *start).c_str(), NULL, 16);
			break;
		} else if (!isxdigit(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	*start = lidx + 1;
	return ret;
}

static i32 parse_data_type (string *line, u32 lnr, u32 *start,
			    bool *is_signed, bool *is_float)
{
	u32 lidx;
	i32 ret = 32;

	lidx = *start;
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	switch (line->at(lidx)) {
	case 'u':
		*is_signed = false;
		*is_float = false;
		break;
	case 'i':
		*is_signed = true;
		*is_float = false;
		break;
	case 'f':
		*is_signed = true;
		*is_float = true;
		break;
	default:
		cfg_parse_err(line, lnr, --lidx);
		break;
	}

	*start = ++lidx;
	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			ret = atoi(string(*line,
			  *start, lidx - *start).c_str());
			if (*is_float && (ret != 32 && ret != 64))
				cfg_parse_err(line, lnr, lidx);
			break;
		} else if (!isdigit(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	*start = ++lidx;
	return ret;
}

/*
 * This function parses a signed/unsigned integer or a float/double value
 * from the config and also determines if a greater/less than check is wanted.
 *
 * Attention: Hacky floats. A float has 4 bytes, a double has 8 bytes
 *            and i64 has 8 bytes. Why not copy the float/double into the i64?!
 *            We always parse floats as doubles here.
 */
static i64 parse_value (string *line, u32 lnr, u32 *start, bool is_signed,
		        bool is_float, i32 *check)
{
	u32 lidx;
	i64 ret = 0;
	double tmp_dval;

	lidx = *start;
	if (lidx + 2 > line->length())
		cfg_parse_err(line, lnr, --lidx);
	if (line->at(lidx) == 'l' && line->at(lidx + 1) == ' ') {
		*check = DO_LT;
		*start += 2;
	} else if (line->at(lidx) == 'g' && line->at(lidx + 1) == ' ') {
		*check = DO_GT;
		*start += 2;
	} else {
		*check = DO_UNCHECKED;
	}

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ' ') {
			break;
		} else if (!isdigit(line->at(lidx)) && line->at(*start) != '-' &&
		    !(is_float && line->at(lidx)) == '.') {
			cfg_parse_err(line, lnr, lidx);
		}
	}
	if (is_float) {
		tmp_dval = strtod(string(*line, *start,
			lidx - *start).c_str(), NULL);
		memcpy(&ret, &tmp_dval, sizeof(i64));  // hacky double to hex
	} else if (is_signed) {
		ret = strtoll(string(*line,
			*start, lidx - *start).c_str(), NULL, 10);
	} else {
		ret = strtoull(string(*line,
			*start, lidx - *start).c_str(), NULL, 10);
	}
	*start = ++lidx;
	return ret;
}

static void parse_key_bindings (string *line, u32 lnr, u32 *start,
				list<CfgEntry> *cfg,
				list<CfgEntry*> **cfgp_map)
{
	u32 lidx;
	char key;

	for (lidx = *start; lidx < line->length(); lidx++) {
		if (line->at(lidx) == ',' || line->at(lidx) == ' ') {
			if (lidx == *start + 1) {
				key = line->at(*start);
				*start = lidx + 1;
				if (!cfgp_map[(i32)key])
					cfgp_map[(i32)key] = new list<CfgEntry*>();
				cfgp_map[(i32)key]->push_back(&cfg->back());
			} else {
				cfg_parse_err(line, lnr, lidx);
			}
			if (line->at(lidx) == ' ')
				break;
		} else if (!isalnum(line->at(lidx))) {
			cfg_parse_err(line, lnr, lidx);
		}
	}
}

list<CfgEntry*> *read_config (char *cfg_name,
			      string *proc_name,
			      list<CfgEntry> *cfg,
			      list<CfgEntry*> **cfgp_map)
{
	CfgEntry cfg_en;
	CfgEntry *cfg_enp;
	list<CfgEntry*> *cfg_act = new list<CfgEntry*>();
	CheckEntry chk_en;
	list<CheckEntry> *chk_lp;
	DynMemEntry *dynmem_enp = NULL;
	u32 lnr, start = 0;
	i32 name_type, tmp;
	bool in_dynmem = false;
	string line;
	vector<string> lines;
	string path(string(CFG_DIR) + string("/")
		+ string(cfg_name) + string(".conf"));

	// read config into string vector
	read_config_vect(&path, &lines);

	// parse config
	*proc_name = parse_proc_name(&lines.at(0), &start);

	for (lnr = 1; lnr < lines.size(); lnr++) {
		line = lines.at(lnr);
		start = 0;
		if (line.length() <= 0 || line[0] == '#')
			continue;

		cfg_en.name = parse_value_name(&line, lnr, &start, &name_type);
		switch (name_type) {
		case NAME_CHECK:
			cfg_enp = &cfg->back();
			if (!cfg_enp->checks)
				cfg_enp->checks = new list<CheckEntry>();

			chk_lp = cfg_enp->checks;
			chk_en.addr = parse_address(&line, lnr, &start);
			chk_en.size = parse_data_type(&line, lnr, &start,
				&chk_en.is_signed, &chk_en.is_float);
			chk_en.value = parse_value(&line, lnr, &start,
				chk_en.is_signed, chk_en.is_float, &chk_en.check);

			chk_lp->push_back(chk_en);
			break;

		case NAME_DYNMEM_START:
			in_dynmem = true;
			dynmem_enp = new DynMemEntry();
			dynmem_enp->name = parse_value_name(&line, lnr,
				&start, &name_type);
			dynmem_enp->mem_size = parse_value(&line, lnr,
				&start, false, false, &tmp);
			dynmem_enp->code_addr = parse_address(&line, lnr, &start);
			dynmem_enp->stack_offs = parse_address(&line, lnr, &start);
			dynmem_enp->mem_addr = NULL;
			break;

		case NAME_DYNMEM_END:
			if (in_dynmem) {
				in_dynmem = false;
				dynmem_enp = NULL;
			} else {
				cfg_parse_err(&line, lnr, start);
			}
			break;

		default:
			cfg_en.checks = NULL;
			cfg_en.addr = parse_address(&line, lnr, &start);
			cfg_en.size = parse_data_type(&line, lnr, &start,
				&cfg_en.is_signed, &cfg_en.is_float);
			cfg_en.value = parse_value(&line, lnr, &start,
				cfg_en.is_signed, cfg_en.is_float, &cfg_en.check);
			if (in_dynmem) {
				cfg_en.dynmem = dynmem_enp;
			} else {
				cfg_en.dynmem = NULL;
			}

			cfg->push_back(cfg_en);

			parse_key_bindings(&line, lnr, &start, cfg, cfgp_map);

			// get activation state
			if (start > line.length())
				cfg_parse_err(&line, lnr, --start);
			else if (line.at(start) == 'a')
				cfg_act->push_back(&cfg->back());
			break;
		}
	}

	return cfg_act;
}
