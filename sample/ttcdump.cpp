// ttcdump: print the structure of a TrueType Collection, and check that every face can be rebuilt.
//
//   ttcdump <input.ttc> [...]
//
// For every face the table directory is printed, each stored table checksum is compared with the
// payload, and the rebuilt face is checked against the whole file checksum `0xB1B0AFBA` - the value
// every valid sfnt has to sum to. The exit code is the number of problems found, so the program can
// be used as a validator.

#include "ttc_tools/ttc_tools.h"
#include <cstdio>
#include <string>
#include <vector>

namespace
{
	int dump_file(const std::string& path)
	{
		std::vector<std::uint8_t> data;
		ttc_tools::error_info error;
		if (!ttc_tools::read_file(path, data, &error))
		{
			std::fprintf(stderr, "ttcdump: %s: %s\n", path.c_str(), error.message.c_str());
			return 1;
		}

		const std::uint8_t* bytes = data.empty() ? nullptr : &data[0];
		ttc_tools::Collection collection;
		if (!collection.parse(bytes, data.size(), &error))
		{
			std::fprintf(stderr, "ttcdump: %s: %s (%s)\n", path.c_str(),
				error.message.c_str(), ttc_tools::error_name(error.code));
			return 1;
		}

		std::printf("%s\n", path.c_str());
		std::printf("  collection version %.1f, %u face(s), %u bytes\n",
			collection.version() == 0x00020000u ? 2.0 : 1.0,
			(unsigned int)collection.face_count(),
			(unsigned int)data.size());

		int problems = 0;
		std::vector<std::uint8_t> rebuilt;
		for (std::size_t i = 0; i < collection.face_count(); ++i)
		{
			ttc_tools::Sfnt sfnt;
			ttc_tools::Face face;
			if (!collection.sfnt_of(i, sfnt, &error) || !collection.describe(i, face, &error))
			{
				std::fprintf(stderr, "  face %u: %s\n", (unsigned int)i, error.message.c_str());
				++problems;
				continue;
			}

			std::printf("  face %u: '%s' '%s', %u table(s), %u glyph(s)\n",
				(unsigned int)i,
				face.family.empty() ? "(no name)" : face.family.c_str(),
				face.subfamily.c_str(),
				(unsigned int)sfnt.table_count(),
				(unsigned int)face.num_glyphs);

			for (std::size_t t = 0; t < sfnt.table_count(); ++t)
			{
				const ttc_tools::Table_Record& table = sfnt.table(t);
				// The head table is stored with a zero adjustment, exactly what build() writes back.
				std::vector<std::uint8_t> payload(
					bytes + table.offset, bytes + table.offset + table.length);
				if (table.tag == ttc_tools::Sfnt::tag_head && payload.size() >= 12)
				{
					payload[8] = payload[9] = payload[10] = payload[11] = 0;
				}
				const std::uint32_t sum = ttc_tools::Sfnt::checksum(
					payload.empty() ? nullptr : &payload[0], payload.size());
				const bool ok = sum == table.check_sum;
				if (!ok)
					++problems;
				std::printf("    %s  off=0x%08X len=%7u sum=0x%08X %s\n",
					ttc_tools::Sfnt::tag_string(table.tag).c_str(),
					(unsigned int)table.offset,
					(unsigned int)table.length,
					(unsigned int)sum,
					ok ? "ok" : "MISMATCH");
			}

			if (!collection.extract(i, rebuilt, nullptr, &error))
			{
				std::fprintf(stderr, "    rebuild failed: %s\n", error.message.c_str());
				++problems;
				continue;
			}
			const std::uint32_t whole = ttc_tools::Sfnt::checksum(&rebuilt[0], rebuilt.size());
			const bool ok = whole == 0xB1B0AFBAu;
			if (!ok)
				++problems;
			std::printf("    rebuilt %u bytes, whole checksum 0x%08X %s\n",
				(unsigned int)rebuilt.size(), (unsigned int)whole, ok ? "ok" : "MISMATCH");
		}
		return problems;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::fprintf(stderr,
			"ttcdump %s - print the structure of a TrueType Collection\n"
			"\n"
			"usage: ttcdump <input.ttc> [...]\n"
			"\n"
			"Exit code is the number of problems found (bad checksums, unreadable faces).\n",
			TTC_TOOLS_VERSION_STRING);
		return 2;
	}

	int problems = 0;
	for (int i = 1; i < argc; ++i)
		problems += dump_file(argv[i]);
	return problems > 255 ? 255 : problems;
}
