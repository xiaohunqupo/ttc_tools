// ttc_tools.cpp : command line front end for the .ttc splitter.
//
//   ttc_tools <input.ttc> [output_dir]     write every face as <family>.ttf / .otf
//   ttc_tools -l <input.ttc>               list the faces
//   ttc_tools -x <input.ttc> <index> <out> extract a single face
//   ttc_tools -v                           print the version
//   ttc_tools -h                           print this help

#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "ttc_tools.h"

namespace
{
	void print_usage(std::FILE* out)
	{
		std::fprintf(out,
			"ttc_tools %s - split a TrueType Collection into standalone fonts\n"
			"\n"
			"usage:\n"
			"  ttc_tools <input.ttc> [output_dir]       extract every face\n"
			"  ttc_tools -l <input.ttc>                 list the faces\n"
			"  ttc_tools -x <input.ttc> <index> <out>   extract one face\n"
			"  ttc_tools -v                             print the version\n"
			"  ttc_tools -h                             print this help\n"
			"\n"
			"Faces are written as <family>.ttf, or .otf for CFF outlines. The output\n"
			"directory defaults to the directory of the input file and is created when\n"
			"missing. Results go to stdout, problems to stderr.\n",
			TTC_TOOLS_VERSION_STRING);
	}

	void print_error(const std::string& what, const ttc_tools::error_info& error)
	{
		std::fprintf(stderr, "ttc_tools: %s: %s (%s)\n",
			what.c_str(), error.message.c_str(), ttc_tools::error_name(error.code));
	}

	std::string parent_directory(const std::string& path)
	{
		const std::size_t slash = path.find_last_of("/\\");
		if (slash == std::string::npos)
			return std::string(".");
		if (slash == 0)
			return path.substr(0, 1);
		return path.substr(0, slash);
	}

	const char* sfnt_name(std::uint32_t version)
	{
		switch (version)
		{
		case 0x00010000u: return "TrueType";
		case 0x74727565u: return "true";
		case 0x4F54544Fu: return "CFF";
		default: return "unknown";
		}
	}

	int list_faces(const std::string& input)
	{
		std::vector<ttc_tools::Face> faces;
		ttc_tools::error_info error;
		if (!ttc_tools::ListTTC(input, faces, &error))
		{
			print_error(input, error);
			return 1;
		}
		std::printf("%s: %u face(s)\n", input.c_str(), (unsigned int)faces.size());
		for (std::size_t i = 0; i < faces.size(); ++i)
		{
			std::printf("  [%u] %-34s %-16s %6u glyphs  %2u tables  %s\n",
				(unsigned int)i,
				faces[i].family.empty() ? "(no name)" : faces[i].family.c_str(),
				faces[i].subfamily.c_str(),
				(unsigned int)faces[i].num_glyphs,
				(unsigned int)faces[i].table_count,
				sfnt_name(faces[i].sfnt_version));
		}
		return 0;
	}

	int extract_all(const std::string& input, const std::string& output_dir)
	{
		const std::string dir = output_dir.empty() ? parent_directory(input) : output_dir;
		std::vector<std::string> written;
		ttc_tools::error_info error;
		if (!ttc_tools::UnzipTTCToDir(input, dir, "Face", &written, &error))
		{
			print_error(input, error);
			return 1;
		}
		for (std::size_t i = 0; i < written.size(); ++i)
			std::printf("%s\n", written[i].c_str());
		std::printf("%u face(s) written to %s\n", (unsigned int)written.size(), dir.c_str());
		return 0;
	}

	int extract_one(const std::string& input, std::size_t index, const std::string& output)
	{
		ttc_tools::Face info;
		ttc_tools::error_info error;
		if (!ttc_tools::UnzipTTCFaceToFile(input, index, output, &info, &error))
		{
			print_error(input, error);
			return 1;
		}
		std::printf("%s: %s %s, %u glyphs\n", output.c_str(),
			info.family.empty() ? "(no name)" : info.family.c_str(),
			info.subfamily.c_str(), (unsigned int)info.num_glyphs);
		return 0;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		print_usage(stderr);
		return 2;
	}

	const std::string first = argv[1];
	if (first == "-h" || first == "--help")
	{
		print_usage(stdout);
		return 0;
	}
	if (first == "-v" || first == "--version")
	{
		std::printf("ttc_tools %s\n", TTC_TOOLS_VERSION_STRING);
		return 0;
	}
	if (first == "-l" || first == "--list")
	{
		if (argc < 3)
		{
			print_usage(stderr);
			return 2;
		}
		return list_faces(argv[2]);
	}
	if (first == "-x" || first == "--extract")
	{
		if (argc < 5)
		{
			print_usage(stderr);
			return 2;
		}
		char* end = nullptr;
		const long index = std::strtol(argv[3], &end, 10);
		if (!end || *end != '\0' || index < 0)
		{
			std::fprintf(stderr, "ttc_tools: '%s' is not a face index\n", argv[3]);
			return 2;
		}
		return extract_one(argv[2], (std::size_t)index, argv[4]);
	}
	if (!first.empty() && first[0] == '-')
	{
		std::fprintf(stderr, "ttc_tools: unknown option '%s'\n", first.c_str());
		print_usage(stderr);
		return 2;
	}

	return extract_all(first, argc > 2 ? std::string(argv[2]) : std::string());
}
