#pragma once
#ifndef _TTC_TOOLS_H
#define _TTC_TOOLS_H

#include <string>
#include <iostream>
#include <vector>

namespace ttc_tools
{
	struct TTC
	{
		typedef std::vector<uint8_t> TTF;
		std::vector<TTF> ttfs;
	};

	bool UnzipTTc(const std::string& filename, const std::string& dir, const std::string& noNameReplacePrefix = "File");

	bool unzipTTC_Memery(const std::string& filename, TTC& ttc);
}

#include "ttc_tools.ipp"
#endif // !_TTC_TOOLS_H