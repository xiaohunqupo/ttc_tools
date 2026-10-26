#ifndef TTC_TOOLS_IPP
#define TTC_TOOLS_IPP

#include <cerrno>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#	include <direct.h>
#else
#	include <sys/stat.h>
#	include <sys/types.h>
#endif

namespace ttc_tools
{
	namespace details
	{
		/* ------------------------------------------------------------------ bytes */

		inline std::uint16_t rd_u16(const std::uint8_t* p)
		{
			return (std::uint16_t)(((std::uint16_t)p[0] << 8) | (std::uint16_t)p[1]);
		}

		inline std::uint32_t rd_u32(const std::uint8_t* p)
		{
			return ((std::uint32_t)p[0] << 24) | ((std::uint32_t)p[1] << 16) |
				((std::uint32_t)p[2] << 8) | (std::uint32_t)p[3];
		}

		inline void wr_u16(std::uint8_t* p, std::uint16_t value)
		{
			p[0] = (std::uint8_t)((value >> 8) & 0xFF);
			p[1] = (std::uint8_t)(value & 0xFF);
		}

		inline void wr_u32(std::uint8_t* p, std::uint32_t value)
		{
			p[0] = (std::uint8_t)((value >> 24) & 0xFF);
			p[1] = (std::uint8_t)((value >> 16) & 0xFF);
			p[2] = (std::uint8_t)((value >> 8) & 0xFF);
			p[3] = (std::uint8_t)(value & 0xFF);
		}

		inline std::size_t align4(std::size_t value)
		{
			return (value + 3u) & ~(std::size_t)3u;
		}

		/*!
		 * \brief Valid pointer for an empty buffer, so that "no bytes" is reported
		 *        as a truncated file and not as "the caller passed nothing".
		 */
		inline const std::uint8_t* empty_pointer()
		{
			static const std::uint8_t zero = 0;
			return &zero;
		}

		/*!
		 * \brief Sum of the big endian 32 bit words of a range, the last word
		 *        zero padded - the unit every sfnt checksum is built from.
		 */
		inline std::uint32_t checksum(const std::uint8_t* data, std::size_t size)
		{
			std::uint32_t sum = 0;
			std::size_t i = 0;
			for (; i + 4 <= size; i += 4)
				sum += rd_u32(data + i);
			if (i < size)
			{
				std::uint32_t last = 0;
				for (std::size_t k = 0; k < 4; ++k)
				{
					last <<= 8;
					if (i + k < size)
						last |= data[i + k];
				}
				sum += last;
			}
			return sum;
		}

		inline std::string hex32(std::uint32_t value)
		{
			char buffer[16];
			std::snprintf(buffer, sizeof(buffer), "0x%08X", (unsigned int)value);
			return std::string(buffer);
		}

		/* -------------------------------------------------------------- constants */

		const std::uint32_t kTagTTCF = 0x74746366u;			//!< 'ttcf'
		const std::uint32_t kSfntTrueType = 0x00010000u;	//!< TrueType outlines
		const std::uint32_t kSfntTrue = 0x74727565u;		//!< 'true', old Apple sfnt
		const std::uint32_t kSfntOTTO = 0x4F54544Fu;		//!< 'OTTO', CFF outlines
		const std::uint32_t kTagHead = 0x68656164u;			//!< 'head'
		const std::uint32_t kTagName = 0x6E616D65u;			//!< 'name'
		const std::uint32_t kTagMaxp = 0x6D617870u;			//!< 'maxp'
		const std::uint32_t kCheckSumMagic = 0xB1B0AFBAu;	//!< whole file checksum of a valid sfnt

		const std::size_t kHeaderBytes = 12;				//!< 'ttcf' + version + face count
		const std::size_t kSfntHeaderBytes = 12;			//!< sfnt version + table count + search fields
		const std::size_t kTableRecordBytes = 16;			//!< tag + checksum + offset + length

		const std::uint32_t kMaxTables = 4096u;				//!< rejected above this
		const std::uint32_t kMaxFaces = 1024u;				//!< rejected above this

		/* ----------------------------------------------------------------- errors */

		inline bool fail(error_info* error, int code, const std::string& message)
		{
			if (error)
			{
				error->code = code;
				error->message = message;
			}
			return false;
		}

		inline bool pass(error_info* error)
		{
			if (error)
			{
				error->code = err_none;
				error->message.clear();
			}
			return true;
		}

		/* -------------------------------------------------------------- utf8/utf16 */

		inline void append_utf8(std::string& out, std::uint32_t cp)
		{
			if (cp < 0x80u)
			{
				out.push_back((char)cp);
			}
			else if (cp < 0x800u)
			{
				out.push_back((char)(0xC0u | (cp >> 6)));
				out.push_back((char)(0x80u | (cp & 0x3Fu)));
			}
			else if (cp < 0x10000u)
			{
				out.push_back((char)(0xE0u | (cp >> 12)));
				out.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
				out.push_back((char)(0x80u | (cp & 0x3Fu)));
			}
			else
			{
				out.push_back((char)(0xF0u | (cp >> 18)));
				out.push_back((char)(0x80u | ((cp >> 12) & 0x3Fu)));
				out.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
				out.push_back((char)(0x80u | (cp & 0x3Fu)));
			}
		}

		/*!
		 * \brief Decode big endian UTF-16, the encoding of the Windows `name` strings.
		 */
		inline std::string utf16be_to_utf8(const std::uint8_t* data, std::size_t bytes)
		{
			std::string out;
			out.reserve(bytes / 2);
			std::size_t i = 0;
			while (i + 1 < bytes)
			{
				std::uint32_t cp = rd_u16(data + i);
				i += 2;
				if (cp >= 0xD800u && cp <= 0xDBFFu)
				{
					if (i + 1 < bytes)
					{
						const std::uint32_t low = rd_u16(data + i);
						if (low >= 0xDC00u && low <= 0xDFFFu)
						{
							i += 2;
							cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
						}
						else
							cp = 0xFFFDu;
					}
					else
						cp = 0xFFFDu;
				}
				else if (cp >= 0xDC00u && cp <= 0xDFFFu)
					cp = 0xFFFDu;
				append_utf8(out, cp);
			}
			return out;
		}

		inline std::string latin1_to_utf8(const std::uint8_t* data, std::size_t bytes)
		{
			std::string out;
			out.reserve(bytes);
			for (std::size_t i = 0; i < bytes; ++i)
				append_utf8(out, data[i]);
			return out;
		}

		/* ----------------------------------------------------------- file handling */

		inline bool file_size(FILE* file, std::uint64_t& size)
		{
			size = 0;
#if defined(_MSC_VER)
			if (_fseeki64(file, 0, SEEK_END) != 0)
				return false;
			const __int64 end = _ftelli64(file);
			if (end < 0 || _fseeki64(file, 0, SEEK_SET) != 0)
				return false;
			size = (std::uint64_t)end;
			return true;
#elif defined(__GNUC__) || defined(__clang__) || defined(__APPLE__)
			if (fseeko(file, 0, SEEK_END) != 0)
				return false;
			const off_t end = ftello(file);
			if (end < 0 || fseeko(file, 0, SEEK_SET) != 0)
				return false;
			size = (std::uint64_t)end;
			return true;
#else
			if (std::fseek(file, 0, SEEK_END) != 0)
				return false;
			const long end = std::ftell(file);
			if (end < 0 || std::fseek(file, 0, SEEK_SET) != 0)
				return false;
			size = (std::uint64_t)end;
			return true;
#endif
		}

#if defined(_WIN32)
		/*!
		 * \brief UTF-8 to UTF-16, so that non ASCII font names become valid paths.
		 */
		inline std::wstring utf8_to_wide(const std::string& text)
		{
			std::wstring out;
			out.reserve(text.size());
			std::size_t i = 0;
			while (i < text.size())
			{
				const unsigned char lead = (unsigned char)text[i];
				std::uint32_t cp = 0xFFFDu;
				int extra = 0;
				if (lead < 0x80u)
					cp = lead;
				else if ((lead & 0xE0u) == 0xC0u)
				{
					cp = lead & 0x1Fu;
					extra = 1;
				}
				else if ((lead & 0xF0u) == 0xE0u)
				{
					cp = lead & 0x0Fu;
					extra = 2;
				}
				else if ((lead & 0xF8u) == 0xF0u)
				{
					cp = lead & 0x07u;
					extra = 3;
				}
				++i;
				for (int k = 0; k < extra && i < text.size(); ++k, ++i)
					cp = (cp << 6) | ((unsigned char)text[i] & 0x3Fu);
				if (cp >= 0x10000u)
				{
					cp -= 0x10000u;
					out.push_back((wchar_t)(0xD800u + (cp >> 10)));
					out.push_back((wchar_t)(0xDC00u + (cp & 0x3FFu)));
				}
				else
					out.push_back((wchar_t)cp);
			}
			return out;
		}
#endif

		inline FILE* open_read(const std::string& path)
		{
#if defined(_MSC_VER)
			FILE* file = nullptr;
			if (_wfopen_s(&file, utf8_to_wide(path).c_str(), L"rb") != 0)
				return nullptr;
			return file;
#elif defined(_WIN32)
			return _wfopen(utf8_to_wide(path).c_str(), L"rb");
#else
			return std::fopen(path.c_str(), "rb");
#endif
		}

		inline FILE* open_write(const std::string& path)
		{
#if defined(_MSC_VER)
			FILE* file = nullptr;
			if (_wfopen_s(&file, utf8_to_wide(path).c_str(), L"wb") != 0)
				return nullptr;
			return file;
#elif defined(_WIN32)
			return _wfopen(utf8_to_wide(path).c_str(), L"wb");
#else
			return std::fopen(path.c_str(), "wb");
#endif
		}

		/*!
		 * \brief Read a whole file, one allocation, no per byte calls.
		 */
		inline bool read_file(const std::string& path, std::vector<std::uint8_t>& out, error_info* error)
		{
			out.clear();
			FILE* file = open_read(path);
			if (!file)
				return fail(error, err_open_failed, "cannot open '" + path + "'");

			std::uint64_t size = 0;
			bool ok = file_size(file, size) && size <= (std::uint64_t)(std::size_t)-1;
			if (ok)
			{
				out.resize((std::size_t)size);
				if (!out.empty() && std::fread(&out[0], 1, out.size(), file) != out.size())
					ok = false;
			}
			ok = ok && std::ferror(file) == 0;
			std::fclose(file);
			if (!ok)
			{
				out.clear();
				return fail(error, err_read_failed, "cannot read '" + path + "'");
			}
			return pass(error);
		}

		inline bool write_file(const std::string& path, const std::uint8_t* data, std::size_t size, error_info* error)
		{
			FILE* file = open_write(path);
			if (!file)
				return fail(error, err_write_failed, "cannot create '" + path + "'");
			bool ok = size == 0 || std::fwrite(data, 1, size, file) == size;
			ok = (std::fclose(file) == 0) && ok;
			if (!ok)
				return fail(error, err_write_failed, "cannot write '" + path + "'");
			return pass(error);
		}

		/* ------------------------------------------------------------- directories */

		inline bool make_dir(const std::string& path)
		{
#if defined(_WIN32)
			if (::_wmkdir(utf8_to_wide(path).c_str()) == 0)
				return true;
#else
			if (::mkdir(path.c_str(), 0777) == 0)
				return true;
#endif
			return errno == EEXIST;
		}

		/*!
		 * \brief Create `path` and every missing parent, one level at a time.
		 */
		inline bool ensure_dir(const std::string& path, error_info* error)
		{
			if (path.empty())
				return pass(error);

			std::string partial;
			std::size_t i = 0;
			if (path.size() >= 2 && path[1] == ':')
			{
				partial = path.substr(0, 2);	// "C:"
				i = 2;
			}
			else if (path[0] == '/' || path[0] == '\\')
			{
				partial = path.substr(0, 1);	// "/"
				i = 1;
			}

			while (i < path.size())
			{
				while (i < path.size() && (path[i] == '/' || path[i] == '\\'))
					++i;
				const std::size_t start = i;
				while (i < path.size() && path[i] != '/' && path[i] != '\\')
					++i;
				if (start == i)
					break;
				if (!partial.empty() && partial[partial.size() - 1] != '/' && partial[partial.size() - 1] != '\\')
					partial.push_back('/');
				partial.append(path, start, i - start);
				if (!make_dir(partial))
					return fail(error, err_mkdir_failed, "cannot create directory '" + partial + "'");
			}
			return pass(error);
		}

		inline std::string join_path(const std::string& dir, const std::string& name)
		{
			if (dir.empty())
				return name;
			const char last = dir[dir.size() - 1];
			if (last == '/' || last == '\\')
				return dir + name;
			return dir + "/" + name;
		}

		/*!
		 * \brief Directory part of a path, "." when there is none.
		 */
		inline std::string parent_of(const std::string& path)
		{
			const std::size_t slash = path.find_last_of("/\\");
			if (slash == std::string::npos)
				return std::string(".");
			if (slash == 0)
				return path.substr(0, 1);
			return path.substr(0, slash);
		}

		/* -------------------------------------------------------------- file names */

		inline int ascii_lower(int c)
		{
			return (c >= 'A' && c <= 'Z') ? c + 32 : c;
		}

		inline bool same_name(const std::string& a, const std::string& b)
		{
			if (a.size() != b.size())
				return false;
			for (std::size_t i = 0; i < a.size(); ++i)
			{
				if (ascii_lower((unsigned char)a[i]) != ascii_lower((unsigned char)b[i]))
					return false;
			}
			return true;
		}

		inline bool name_taken(const std::string& name, const std::vector<std::string>& used)
		{
			for (std::size_t i = 0; i < used.size(); ++i)
			{
				if (same_name(name, used[i]))
					return true;
			}
			return false;
		}

		/*!
		 * \brief Strip what a file name cannot carry, trim, and cap the byte length
		 *        without cutting a UTF-8 sequence in half.
		 */
		inline std::string sanitize_name(const std::string& name)
		{
			const std::size_t kMaxBytes = 120;

			std::string out;
			out.reserve(name.size());
			for (std::size_t i = 0; i < name.size(); ++i)
			{
				const unsigned char c = (unsigned char)name[i];
				if (c < 0x20u || c == 0x7Fu)
					continue;
				if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' ||
					c == '|' || c == '?' || c == '*')
					out.push_back('_');
				else
					out.push_back((char)c);
			}

			std::size_t begin = 0;
			std::size_t end = out.size();
			while (begin < end && (out[begin] == ' ' || out[begin] == '.'))
				++begin;
			while (end > begin && (out[end - 1] == ' ' || out[end - 1] == '.'))
				--end;
			out = out.substr(begin, end - begin);

			if (out.size() > kMaxBytes)
			{
				std::size_t cut = kMaxBytes;
				while (cut > 0 && ((unsigned char)out[cut] & 0xC0u) == 0x80u)
					--cut;
				out.resize(cut);
				while (!out.empty() && (out[out.size() - 1] == ' ' || out[out.size() - 1] == '.'))
					out.erase(out.size() - 1);
			}
			return out;
		}

		inline std::string number_suffix(std::size_t index)
		{
			std::string digits = std::to_string((unsigned long long)index);
			if (digits.size() < 2)
				digits.insert(digits.begin(), '0');
			return digits;
		}

		/* ------------------------------------------------------------------- sfnt */

		struct table_record
		{
			std::uint32_t tag;
			std::uint32_t checksum;
			std::uint32_t offset;
			std::uint32_t length;
		};

		struct face_layout
		{
			std::uint32_t offset;			//!< start of the face header inside the collection
			std::uint32_t sfnt_version;
			std::uint32_t num_tables;
			int head_index;					//!< -1 when the face has no `head`
			int name_index;
			int maxp_index;
			std::vector<table_record> tables;

			face_layout()
				: offset(0), sfnt_version(0), num_tables(0), head_index(-1), name_index(-1), maxp_index(-1)
			{
			}
		};

		inline int find_table(const face_layout& layout, std::uint32_t tag)
		{
			for (std::size_t i = 0; i < layout.tables.size(); ++i)
			{
				if (layout.tables[i].tag == tag)
					return (int)i;
			}
			return -1;
		}

		inline bool known_sfnt_version(std::uint32_t version)
		{
			return version == kSfntTrueType || version == kSfntTrue || version == kSfntOTTO;
		}

		/*!
		 * \brief Parse and validate the collection header.
		 */
		inline bool parse_header(const std::uint8_t* data, std::size_t size,
			std::uint32_t& version, std::vector<std::uint32_t>& offsets, error_info* error)
		{
			version = 0;
			offsets.clear();

			if (!data || size < kHeaderBytes)
				return fail(error, err_truncated, "file is smaller than the 12 byte collection header");
			if (rd_u32(data) != kTagTTCF)
				return fail(error, err_not_a_collection, "signature is not 'ttcf'");

			version = rd_u32(data + 4);
			if (version != 0x00010000u && version != 0x00020000u)
				return fail(error, err_unsupported_version, "unsupported collection version " + hex32(version));

			const std::uint32_t count = rd_u32(data + 8);
			if (count == 0)
				return fail(error, err_empty_collection, "collection declares zero faces");
			if (count > kMaxFaces)
				return fail(error, err_bad_face_count, "collection declares " +
					std::to_string((unsigned long long)count) + " faces, limit is " +
					std::to_string((unsigned long long)kMaxFaces));
			if ((std::size_t)count > (size - kHeaderBytes) / 4)
				return fail(error, err_truncated, "face offset array runs past the end of the file");

			offsets.resize(count);
			for (std::uint32_t i = 0; i < count; ++i)
				offsets[i] = rd_u32(data + kHeaderBytes + (std::size_t)i * 4);
			return pass(error);
		}

		/*!
		 * \brief Parse and validate one face directory.
		 */
		inline bool parse_face(const std::uint8_t* data, std::size_t size, std::size_t index,
			std::uint32_t offset, face_layout& layout, error_info* error)
		{
			layout = face_layout();
			layout.offset = offset;

			const std::string where = "face " + std::to_string((unsigned long long)index);
			if ((std::size_t)offset + kSfntHeaderBytes > size)
				return fail(error, err_truncated, where + " starts at " + hex32(offset) + ", past the end of the file");

			const std::uint8_t* face = data + offset;
			layout.sfnt_version = rd_u32(face);
			if (!known_sfnt_version(layout.sfnt_version))
				return fail(error, err_bad_face, where + " has an unknown sfnt version " + hex32(layout.sfnt_version));

			layout.num_tables = rd_u16(face + 4);
			if (layout.num_tables == 0 || layout.num_tables > kMaxTables)
				return fail(error, err_bad_table_count, where + " declares " +
					std::to_string((unsigned long long)layout.num_tables) + " tables, limit is " +
					std::to_string((unsigned long long)kMaxTables));

			const std::size_t directory_bytes = kSfntHeaderBytes + (std::size_t)layout.num_tables * kTableRecordBytes;
			if ((std::size_t)offset + directory_bytes > size)
				return fail(error, err_truncated, where + " table directory runs past the end of the file");

			layout.tables.resize(layout.num_tables);
			for (std::uint32_t i = 0; i < layout.num_tables; ++i)
			{
				const std::uint8_t* record = face + kSfntHeaderBytes + (std::size_t)i * kTableRecordBytes;
				table_record& table = layout.tables[i];
				table.tag = rd_u32(record);
				table.checksum = rd_u32(record + 4);
				table.offset = rd_u32(record + 8);
				table.length = rd_u32(record + 12);
				if ((std::size_t)table.offset + table.length > size)
					return fail(error, err_truncated, where + " table " + hex32(table.tag) + " runs past the end of the file");
			}

			layout.head_index = find_table(layout, kTagHead);
			layout.name_index = find_table(layout, kTagName);
			layout.maxp_index = find_table(layout, kTagMaxp);
			return pass(error);
		}

		/*!
		 * \brief Rebuild one face as a standalone sfnt.
		 *
		 * Table data is copied verbatim, the directory is rewritten with new
		 * offsets, every table checksum is recomputed, and `head.checkSumAdjustment`
		 * is set so that the whole file sums to 0xB1B0AFBA.
		 */
		inline bool build_face(const std::uint8_t* data, const face_layout& layout,
			std::vector<std::uint8_t>& out, error_info* error)
		{
			const std::size_t count = layout.tables.size();
			std::vector<std::uint32_t> offsets(count);

			std::size_t total = kSfntHeaderBytes + count * kTableRecordBytes;
			for (std::size_t i = 0; i < count; ++i)
			{
				const std::size_t start = align4(total);
				if (start + layout.tables[i].length > 0xFFFFFFFFu)
					return fail(error, err_bad_face, "face does not fit in 4 GiB");
				offsets[i] = (std::uint32_t)start;
				total = start + layout.tables[i].length;
			}
			total = align4(total);
			if (total > 0xFFFFFFFFu)
				return fail(error, err_bad_face, "face does not fit in 4 GiB");

			out.assign(total, 0);	// also zeroes the padding between tables

			std::uint16_t power = 1;
			std::uint16_t log = 0;
			while ((std::uint32_t)power * 2u <= (std::uint32_t)count)
			{
				power = (std::uint16_t)(power * 2u);
				++log;
			}

			wr_u32(&out[0], layout.sfnt_version);
			wr_u16(&out[4], (std::uint16_t)count);
			wr_u16(&out[6], (std::uint16_t)((std::uint32_t)power * 16u));
			wr_u16(&out[8], log);
			wr_u16(&out[10], (std::uint16_t)((std::uint32_t)count * 16u - (std::uint32_t)power * 16u));

			for (std::size_t i = 0; i < count; ++i)
			{
				const table_record& table = layout.tables[i];
				std::uint8_t* record = &out[kSfntHeaderBytes + i * kTableRecordBytes];
				wr_u32(record, table.tag);
				wr_u32(record + 8, offsets[i]);
				wr_u32(record + 12, table.length);
				if (table.length != 0)
					std::memcpy(&out[offsets[i]], data + table.offset, table.length);
			}

			if (layout.head_index >= 0 && layout.tables[layout.head_index].length >= 12)
				wr_u32(&out[offsets[layout.head_index] + 8], 0);

			for (std::size_t i = 0; i < count; ++i)
			{
				std::uint8_t* record = &out[kSfntHeaderBytes + i * kTableRecordBytes];
				wr_u32(record + 4, checksum(&out[offsets[i]], layout.tables[i].length));
			}

			if (layout.head_index >= 0 && layout.tables[layout.head_index].length >= 12)
			{
				const std::uint32_t adjustment = kCheckSumMagic - checksum(&out[0], out.size());
				wr_u32(&out[offsets[layout.head_index] + 8], adjustment);
			}
			return pass(error);
		}

		/*!
		 * \brief Read one `name` record (1 = family, 2 = subfamily) as UTF-8.
		 */
		inline bool read_name(const std::uint8_t* data, const face_layout& layout,
			std::uint16_t name_id, std::string& out)
		{
			if (layout.name_index < 0)
				return false;
			const table_record& table = layout.tables[layout.name_index];
			if (table.length < 6)
				return false;

			const std::uint8_t* name = data + table.offset;
			const std::uint16_t count = rd_u16(name + 2);
			const std::uint16_t storage = rd_u16(name + 4);
			if (6u + (std::size_t)count * 12u > table.length)
				return false;

			int best_score = 1000;
			std::string best;
			for (std::uint16_t i = 0; i < count; ++i)
			{
				const std::uint8_t* record = name + 6 + (std::size_t)i * 12;
				const std::uint16_t platform = rd_u16(record);
				const std::uint16_t encoding = rd_u16(record + 2);
				const std::uint16_t language = rd_u16(record + 4);
				const std::uint16_t id = rd_u16(record + 6);
				const std::uint16_t length = rd_u16(record + 8);
				const std::uint16_t offset = rd_u16(record + 10);
				if (id != name_id)
					continue;
				if ((std::size_t)storage + offset + length > table.length)
					continue;

				int score = 0;
				if (platform == 3)
					score = (encoding == 1 || encoding == 10) ? 0 : 1;
				else if (platform == 0)
					score = 1;
				else if (platform == 1)
					score = 3;
				else
					continue;
				if (platform == 3 && language != 0x0409u)
					++score;
				if (score >= best_score)
					continue;

				const std::uint8_t* text = name + storage + offset;
				best_score = score;
				best = (platform == 1) ? latin1_to_utf8(text, length) : utf16be_to_utf8(text, length);
			}

			if (best_score == 1000)
				return false;
			out = best;
			return true;
		}

		inline void describe_face(const std::uint8_t* data, const face_layout& layout, Face& face)
		{
			face.sfnt_version = layout.sfnt_version;
			face.table_count = layout.num_tables;
			face.num_glyphs = 0;
			face.family.clear();
			face.subfamily.clear();

			if (layout.maxp_index >= 0 && layout.tables[layout.maxp_index].length >= 6)
				face.num_glyphs = rd_u16(data + layout.tables[layout.maxp_index].offset + 4);
			read_name(data, layout, 1, face.family);
			read_name(data, layout, 2, face.subfamily);
		}

		/*!
		 * \brief `<family>`, or `_<subfamily>` / `_N` when that stem is taken.
		 */
		inline std::string make_file_stem(const Face& face, std::size_t index,
			const std::string& no_name_prefix, const std::vector<std::string>& used)
		{
			std::string base = sanitize_name(face.family);
			if (base.empty())
				base = sanitize_name(no_name_prefix) + "_" + number_suffix(index);
			if (base.empty())
				base = "File_" + number_suffix(index);

			std::string candidate = base;
			const std::string sub = sanitize_name(face.subfamily);
			if (name_taken(candidate, used) && !sub.empty())
				candidate = base + "_" + sub;
			for (unsigned int n = 2; name_taken(candidate, used) && n < 10000u; ++n)
				candidate = base + "_" + (sub.empty() ? std::string() : sub + "_") + std::to_string((unsigned long long)n);
			return candidate;
		}
	}

	/* -------------------------------------------------------------------- public */

	inline bool ListTTC(const void* data, std::size_t size, std::vector<Face>& faces, error_info* error)
	{
		faces.clear();
		if (!data)
			return details::fail(error, err_open_failed, "no data");

		const std::uint8_t* bytes = (const std::uint8_t*)data;
		std::uint32_t version = 0;
		std::vector<std::uint32_t> offsets;
		if (!details::parse_header(bytes, size, version, offsets, error))
			return false;

		faces.reserve(offsets.size());
		for (std::size_t i = 0; i < offsets.size(); ++i)
		{
			details::face_layout layout;
			if (!details::parse_face(bytes, size, i, offsets[i], layout, error))
				return false;
			Face face;
			details::describe_face(bytes, layout, face);
			faces.push_back(face);
		}
		return details::pass(error);
	}

	inline bool ListTTC(const std::string& filename, std::vector<Face>& faces, error_info* error)
	{
		std::vector<std::uint8_t> data;
		if (!details::read_file(filename, data, error))
			return false;
		return ListTTC(data.empty() ? details::empty_pointer() : &data[0], data.size(), faces, error);
	}

	inline bool UnzipTTC(const void* data, std::size_t size, TTC& ttc, error_info* error)
	{
		ttc.version = 0;
		ttc.ttfs.clear();
		ttc.faces.clear();
		if (!data)
			return details::fail(error, err_open_failed, "no data");

		const std::uint8_t* bytes = (const std::uint8_t*)data;
		std::uint32_t version = 0;
		std::vector<std::uint32_t> offsets;
		if (!details::parse_header(bytes, size, version, offsets, error))
			return false;

		ttc.version = version;
		ttc.ttfs.resize(offsets.size());
		ttc.faces.resize(offsets.size());
		for (std::size_t i = 0; i < offsets.size(); ++i)
		{
			details::face_layout layout;
			if (!details::parse_face(bytes, size, i, offsets[i], layout, error) ||
				!details::build_face(bytes, layout, ttc.ttfs[i], error))
			{
				ttc.version = 0;
				ttc.ttfs.clear();
				ttc.faces.clear();
				return false;
			}
			details::describe_face(bytes, layout, ttc.faces[i]);
		}
		return details::pass(error);
	}

	inline bool UnzipTTC(const std::string& filename, TTC& ttc, error_info* error)
	{
		std::vector<std::uint8_t> data;
		if (!details::read_file(filename, data, error))
		{
			ttc.version = 0;
			ttc.ttfs.clear();
			ttc.faces.clear();
			return false;
		}
		return UnzipTTC(data.empty() ? details::empty_pointer() : &data[0], data.size(), ttc, error);
	}

	inline bool UnzipTTCFace(const void* data, std::size_t size, std::size_t index,
		TTC::TTF& ttf, Face* face, error_info* error)
	{
		ttf.clear();
		if (face)
			*face = Face();
		if (!data)
			return details::fail(error, err_open_failed, "no data");

		const std::uint8_t* bytes = (const std::uint8_t*)data;
		std::uint32_t version = 0;
		std::vector<std::uint32_t> offsets;
		if (!details::parse_header(bytes, size, version, offsets, error))
			return false;
		if (index >= offsets.size())
			return details::fail(error, err_out_of_range, "face index " +
				std::to_string((unsigned long long)index) + " is outside a collection of " +
				std::to_string((unsigned long long)offsets.size()));

		details::face_layout layout;
		if (!details::parse_face(bytes, size, index, offsets[index], layout, error))
			return false;
		if (!details::build_face(bytes, layout, ttf, error))
			return false;
		if (face)
			details::describe_face(bytes, layout, *face);
		return details::pass(error);
	}

	inline bool UnzipTTCFace(const std::string& filename, std::size_t index,
		TTC::TTF& ttf, Face* face, error_info* error)
	{
		std::vector<std::uint8_t> data;
		if (!details::read_file(filename, data, error))
		{
			ttf.clear();
			if (face)
				*face = Face();
			return false;
		}
		return UnzipTTCFace(data.empty() ? details::empty_pointer() : &data[0], data.size(), index, ttf, face, error);
	}

	inline bool UnzipTTCToDir(const void* data, std::size_t size, const std::string& dir,
		const std::string& no_name_prefix, std::vector<std::string>* written, error_info* error)
	{
		if (written)
			written->clear();
		if (!data)
			return details::fail(error, err_open_failed, "no data");

		const std::uint8_t* bytes = (const std::uint8_t*)data;
		std::uint32_t version = 0;
		std::vector<std::uint32_t> offsets;
		if (!details::parse_header(bytes, size, version, offsets, error))
			return false;
		if (!details::ensure_dir(dir, error))
			return false;

		std::vector<std::string> used;
		std::vector<std::uint8_t> face_bytes;
		for (std::size_t i = 0; i < offsets.size(); ++i)
		{
			details::face_layout layout;
			if (!details::parse_face(bytes, size, i, offsets[i], layout, error))
				return false;
			if (!details::build_face(bytes, layout, face_bytes, error))
				return false;

			Face face;
			details::describe_face(bytes, layout, face);
			const std::string stem = details::make_file_stem(face, i, no_name_prefix, used);
			const std::string extension = layout.sfnt_version == details::kSfntOTTO ? ".otf" : ".ttf";
			const std::string path = details::join_path(dir, stem + extension);

			if (!details::write_file(path, face_bytes.empty() ? nullptr : &face_bytes[0], face_bytes.size(), error))
				return false;

			used.push_back(stem);
			if (written)
				written->push_back(path);
		}
		return details::pass(error);
	}

	inline bool UnzipTTCToDir(const std::string& filename, const std::string& dir,
		const std::string& no_name_prefix, std::vector<std::string>* written, error_info* error)
	{
		std::vector<std::uint8_t> data;
		if (!details::read_file(filename, data, error))
		{
			if (written)
				written->clear();
			return false;
		}
		return UnzipTTCToDir(data.empty() ? details::empty_pointer() : &data[0], data.size(), dir, no_name_prefix, written, error);
	}

	inline bool UnzipTTCFaceToFile(const std::string& filename, std::size_t index,
		const std::string& path, Face* face, error_info* error)
	{
		TTC::TTF bytes;
		if (!UnzipTTCFace(filename, index, bytes, face, error))
			return false;
		if (!details::ensure_dir(details::parent_of(path), error))
			return false;
		return details::write_file(path, bytes.empty() ? nullptr : &bytes[0], bytes.size(), error);
	}
}

#endif // TTC_TOOLS_IPP
