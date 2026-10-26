#pragma once
#ifndef _TTC_TOOLS_H_
#define _TTC_TOOLS_H_

/*!
 * \file ttc_tools.h
 * \brief Umbrella header: pulls in the collection parser, the face rebuild and the extraction
 *        helpers.
 *
 * `ttc_tools` splits a TrueType Collection (`.ttc`) into standalone fonts (`.ttf` / `.otf`). A
 * collection stores several faces in one file and lets them share table data; most font consumers -
 * `ttf2mesh` among them - accept a single face only, so the shared tables have to be copied out and
 * the checksums recomputed before they can be used. See `docs/ttc_format.md` for the byte layout and
 * `README.md` for the usage.
 */

/*!
 * \name Version
 * Major / minor / patch number of the library, currently 0.3.1. The numbers are maintained here by
 * hand - the build system only reads them - and `CHANGELOG.md` records what changed in each version.
 * Bump them together with a new `CHANGELOG.md` entry.
 */
///@{
#define TTC_TOOLS_VERSION_MAJOR 0 //!< Incompatible API changes.
#define TTC_TOOLS_VERSION_MINOR 3 //!< New features, backwards compatible.
#define TTC_TOOLS_VERSION_PATCH 1 //!< Fixes and internal changes.
#define TTC_TOOLS_VERSION_STRING "0.3.1" //!< The three numbers as one string, for logs and `--version`.
///@}

#include "ttc_tools/config.h"
#include "ttc_tools/ttc_error.h"
#include "ttc_tools/ttc_file.h"
#include "ttc_tools/ttc_sfnt.h"
#include "ttc_tools/ttc_name.h"
#include "ttc_tools/ttc_collection.h"
#include "ttc_tools/ttc_extract.h"

#endif // !_TTC_TOOLS_H_
