#ifndef TTC_TOOLS_H
#define TTC_TOOLS_H

/*!
 * \file ttc_tools.h
 * \brief Split a TrueType Collection (`.ttc`) into standalone fonts (`.ttf` / `.otf`).
 *
 * A `.ttc` stores several faces in one file and lets them share table data. Most
 * font consumers - ttf2mesh (fetisov) among them - accept a single face only, so
 * the collection has to be turned back into one sfnt per face before use.
 *
 * C++11, header only, no third party dependencies. See README.md for the format
 * description and docs/ttc_format.md for the byte level layout.
 */

#define TTC_TOOLS_VERSION_MAJOR 0
#define TTC_TOOLS_VERSION_MINOR 3
#define TTC_TOOLS_VERSION_PATCH 0
#define TTC_TOOLS_VERSION_STRING "0.3.0"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ttc_tools
{
	/*!
	 * \brief Reason a call failed, see `error_info::code`.
	 */
	enum error_code
	{
		err_none = 0,					//!< success
		err_open_failed = 1,			//!< input file could not be opened
		err_read_failed = 2,			//!< input file could not be read to the end
		err_write_failed = 3,			//!< a face could not be written
		err_mkdir_failed = 4,			//!< the output directory could not be created
		err_not_a_collection = 5,		//!< signature is not `ttcf`
		err_unsupported_version = 6,	//!< collection version is not 1.0 or 2.0
		err_empty_collection = 7,		//!< the header declares zero faces
		err_bad_face_count = 8,			//!< face count exceeds the supported limit
		err_truncated = 9,				//!< header, directory or table data runs past the end of the file
		err_bad_face = 10,				//!< a face is not a known sfnt (TrueType / `true` / `OTTO`)
		err_bad_table_count = 11,		//!< a face declares zero or too many tables
		err_out_of_range = 12			//!< face index is outside the collection
	};

	/*!
	 * \brief Short name of an error code, for logs. Never returns `nullptr`.
	 */
	inline const char* error_name(int code)
	{
		switch (code)
		{
		case err_none: return "err_none";
		case err_open_failed: return "err_open_failed";
		case err_read_failed: return "err_read_failed";
		case err_write_failed: return "err_write_failed";
		case err_mkdir_failed: return "err_mkdir_failed";
		case err_not_a_collection: return "err_not_a_collection";
		case err_unsupported_version: return "err_unsupported_version";
		case err_empty_collection: return "err_empty_collection";
		case err_bad_face_count: return "err_bad_face_count";
		case err_truncated: return "err_truncated";
		case err_bad_face: return "err_bad_face";
		case err_bad_table_count: return "err_bad_table_count";
		case err_out_of_range: return "err_out_of_range";
		default: return "err_unknown";
		}
	}

	/*!
	 * \brief Diagnostic for the last call. Optional everywhere: pass `nullptr`
	 *        when only the boolean result matters.
	 */
	struct error_info
	{
		int code = err_none;
		std::string message;

		bool ok() const { return code == err_none; }
		void clear() { code = err_none; message.clear(); }
	};

	/*!
	 * \brief Metadata of one face, read from the `name` and `maxp` tables.
	 */
	struct Face
	{
		std::uint32_t sfnt_version = 0;	//!< 0x00010000 / `true` / `OTTO`
		std::uint32_t table_count = 0;	//!< number of tables in the face
		std::uint16_t num_glyphs = 0;	//!< `maxp.numGlyphs`, 0 when the table is missing
		std::string family;				//!< `name` id 1, UTF-8
		std::string subfamily;			//!< `name` id 2, UTF-8
	};

	/*!
	 * \brief A collection unpacked in memory.
	 */
	struct TTC
	{
		typedef std::vector<std::uint8_t> TTF;

		std::uint32_t version = 0;		//!< collection version from the header
		std::vector<TTF> ttfs;			//!< one complete sfnt per face, in file order
		std::vector<Face> faces;		//!< metadata, always parallel to `ttfs`
	};

	/* ---------------- list ---------------- */

	/*!
	 * \brief Read face metadata without rebuilding the faces.
	 * \param data Collection bytes.
	 * \param size Byte count.
	 * \param [out] faces One entry per face, in file order.
	 * \param [out] error Optional diagnostic.
	 */
	bool ListTTC(const void* data, std::size_t size, std::vector<Face>& faces, error_info* error = nullptr);

	/*!
	 * \brief Read face metadata straight from a file.
	 */
	bool ListTTC(const std::string& filename, std::vector<Face>& faces, error_info* error = nullptr);

	/* ---------------- extract to memory ---------------- */

	/*!
	 * \brief Rebuild every face as a standalone sfnt in memory.
	 *
	 * Cost: roughly `num_faces x file_size` bytes, because the table data the
	 * faces share has to be copied into each of them. Use `UnzipTTCFace()` to
	 * hold one face at a time.
	 */
	bool UnzipTTC(const void* data, std::size_t size, TTC& ttc, error_info* error = nullptr);

	/*!
	 * \brief Rebuild every face of a file as standalone sfnts in memory.
	 */
	bool UnzipTTC(const std::string& filename, TTC& ttc, error_info* error = nullptr);

	/* ---------------- extract one face ---------------- */

	/*!
	 * \brief Rebuild one face as a standalone sfnt in memory.
	 * \param index Face index, `0 .. size-1` of the collection.
	 * \param [out] ttf Receives the complete sfnt bytes.
	 * \param [out] face Optional metadata.
	 */
	bool UnzipTTCFace(const void* data, std::size_t size, std::size_t index,
		TTC::TTF& ttf, Face* face = nullptr, error_info* error = nullptr);

	/*!
	 * \brief Rebuild one face of a file.
	 */
	bool UnzipTTCFace(const std::string& filename, std::size_t index,
		TTC::TTF& ttf, Face* face = nullptr, error_info* error = nullptr);

	/* ---------------- extract to disk ---------------- */

	/*!
	 * \brief Write every face next to each other as `<family>.ttf` / `<family>.otf`.
	 *
	 * The output directory is created when missing. A face without a usable
	 * `name` table falls back to `<no_name_prefix>_<index>`, and a name that is
	 * already taken gets `_<subfamily>` and then `_2`, `_3` ... appended.
	 *
	 * \param [out] written Optional list of the files that were written.
	 */
	bool UnzipTTCToDir(const std::string& filename, const std::string& dir,
		const std::string& no_name_prefix = "File",
		std::vector<std::string>* written = nullptr, error_info* error = nullptr);

	/*!
	 * \brief Write every face of an in-memory collection.
	 */
	bool UnzipTTCToDir(const void* data, std::size_t size, const std::string& dir,
		const std::string& no_name_prefix = "File",
		std::vector<std::string>* written = nullptr, error_info* error = nullptr);

	/*!
	 * \brief Write one face to an explicit path, creating the parent directory when missing.
	 */
	bool UnzipTTCFaceToFile(const std::string& filename, std::size_t index,
		const std::string& path, Face* face = nullptr, error_info* error = nullptr);

	/* ---------------- 0.2.0 compatibility ---------------- */

	/*!
	 * \brief Same as `UnzipTTCToDir()`, spelling kept from 0.2.0.
	 */
	inline bool UnzipTTc(const std::string& filename, const std::string& dir,
		const std::string& noNameReplacePrefix = "File")
	{
		return UnzipTTCToDir(filename, dir, noNameReplacePrefix, nullptr, nullptr);
	}

	/*!
	 * \brief Same as `UnzipTTC()`, spelling kept from 0.2.0.
	 *
	 * Until 0.2.0 this filled a local copy and handed nothing back; it does now.
	 */
	inline bool unzipTTC_Memery(const std::string& filename, TTC& ttcData)
	{
		return UnzipTTC(filename, ttcData, nullptr);
	}

	/*!
	 * \brief Same as `unzipTTC_Memery()`, spelled correctly.
	 */
	inline bool unzipTTC_Memory(const std::string& filename, TTC& ttcData)
	{
		return UnzipTTC(filename, ttcData, nullptr);
	}
}

#include "ttc_tools.ipp"

#endif // TTC_TOOLS_H
