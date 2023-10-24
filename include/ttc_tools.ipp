namespace ttc_tools
{
	namespace details
	{
		inline int MakeInt(char c1, char c2, char c3, char c4)
		{
			return (c1 << 24) | (c2 << 16) | (c3 << 8) | c4;
		}
		inline void WriteChar(FILE* f, char c)
		{
			putc(c, f);
		}

		inline void WriteChar(TTC::TTF& ttf, char c, int pos = -1)
		{
			if (pos == -1)
				ttf.emplace_back(c);
			else
				ttf[pos] = c;
		}

		inline void WriteShort(FILE* f, int val)
		{
			WriteChar(f, (val >> 8) & 0xff);
			WriteChar(f, val & 0xff);
		}

		inline void WriteShort(TTC::TTF& ttf, int val, int pos = -1)
		{
			if (pos == -1)
			{
				ttf.emplace_back((val >> 8) & 0xff);
				ttf.emplace_back(val & 0xff);
			}
			else
			{
				ttf[pos] = (val >> 8) & 0xff;
				ttf[pos+1] =val & 0xff;
			}
			
		}

		inline void WriteLong(FILE* f, int val)
		{
			WriteChar(f, (val >> 24) & 0xff);
			WriteChar(f, (val >> 16) & 0xff);
			WriteChar(f, (val >> 8) & 0xff);
			WriteChar(f, val & 0xff);
		}

		inline void WriteLong(TTC::TTF& ttf, int val, int pos = -1)
		{
			if (pos == -1)
			{
				ttf.emplace_back((val >> 24) & 0xff);
				ttf.emplace_back((val >> 16) & 0xff);
				ttf.emplace_back((val >> 8) & 0xff);
				ttf.emplace_back(val & 0xff);
			}
			else
			{
				ttf[pos] = (val >> 24) & 0xff;
				ttf[pos + 1] = (val >> 16) & 0xff;
				ttf[pos + 2] = (val >> 8) & 0xff;
				ttf[pos + 3] = val & 0xff;
			}
		}

		inline int ReadChar(FILE* f)
		{
			return getc(f);
		}

		inline int ReadChar(const TTC::TTF& ttf, size_t& pos)
		{
			int c = ttf[pos];
			++pos;
			return c;
		}

		inline int ReadShort(FILE* f)
		{
			int ch1 = ReadChar(f);
			int ch2 = ReadChar(f);
			if (ch2 == EOF)
				return(EOF);
			return (ch1 << 8) | ch2;
		}

		inline int ReadUShort(const TTC::TTF& ttf, size_t& pos)
		{
			int ch1 = ttf[pos];
			int ch2 = ttf[pos+1];
			pos += 2;
			if (ch2 == EOF)
				return(EOF);
			return (ch1 << 8) | ch2;
		}

		inline int ReadLong(FILE* ttf)
		{
			int ch1 = getc(ttf);
			int ch2 = getc(ttf);
			int ch3 = getc(ttf);
			int ch4 = getc(ttf);
			if (ch4 == EOF)
				return(EOF);
			return (ch1 << 24) | (ch2 << 16) | (ch3 << 8) | ch4;
		}

		inline int ReadLong(const TTC::TTF& ttf, size_t& pos)
		{
			int ch1 = ttf[pos];
			int ch2 = ttf[pos + 1];
			int ch3 = ttf[pos + 2];
			int ch4 = ttf[pos + 3];
			pos += 4;
			if (ch4 == EOF)
				return(EOF);
			return (ch1 << 24) | (ch2 << 16) | (ch3 << 8) | ch4;
		}

		inline unsigned int CheckSum(FILE* f)
		{
			unsigned int sum = 0;
			rewind(f);
			while (true)
			{
				unsigned int chuck = ReadLong(f);
				if (feof(f) || ferror(f))
					break;
				sum += chuck;
			}
			return sum;
		}

		inline unsigned int CheckSum(TTC::TTF& ttf)
		{
			unsigned int sum = 0;
			size_t pos = 0;
			while (true)
			{
				unsigned int chuck = ReadLong(ttf, pos);
				if (pos >= ttf.size())
					break;
				sum += chuck;
			}
			return sum;
		}

		inline void CopyTableDataTo(FILE* ttc, FILE* ttf, int offset, int length)
		{
			fseek(ttc, offset, SEEK_SET);
			for (int i = 0; i < length; ++i)
			{
				int c = ReadChar(ttc);
				if (c == EOF)
				{
					// Data ended before table
					break;
				}
				WriteChar(ttf, c);
			}
			if (length & 1)
			{
				WriteChar(ttf, '\0');
				++length;
			}
			if (length & 2)
			{
				WriteShort(ttf, 0);
			}
		}

		inline void CopyTableDataTo(FILE* ttc, TTC::TTF& ttf, int offset, int length)
		{
			fseek(ttc, offset, SEEK_SET);
			for (int i = 0; i < length; ++i)
			{
				int c = ReadChar(ttc);
				if (c == EOF)
				{
					// Data ended before table
					break;
				}
				WriteChar(ttf, c);
			}
			if (length & 1)
			{
				WriteChar(ttf, '\0');
				++length;
			}
			if (length & 2)
			{
				WriteShort(ttf, 0);
			}
		}

		inline void WriteTTF(FILE* ttc, FILE* ttf, int ttfOffset)
		{
			// Move to ttf begin.
			fseek(ttc, ttfOffset, SEEK_SET);

			// Copy File Info
			int ttfVersion = ReadLong(ttc);
			WriteLong(ttf, ttfVersion);

			int tableCount = ReadShort(ttc);
			WriteShort(ttf, tableCount);

			int other1 = ReadShort(ttc);
			WriteShort(ttf, other1);

			int other2 = ReadShort(ttc);
			WriteShort(ttf, other2);

			int other3 = ReadShort(ttc);
			WriteShort(ttf, other3);

			// Copy Table infos.
			std::vector<int> ttfOffsets(tableCount);
			std::vector<int> ttfLengths(tableCount);
			int headIndex = -1;
			for (size_t i = 0; i < tableCount; i++)
			{
				int tag = ReadLong(ttc);
				WriteLong(ttf, tag);
				if (tag == MakeInt('h', 'e', 'a', 'd'))
					headIndex = i;

				int checksum = ReadLong(ttc);
				WriteLong(ttf, checksum);

				int offset = ReadLong(ttc);
				WriteLong(ttf, offset);
				ttfOffsets[i] = offset;

				int length = ReadLong(ttc);
				WriteLong(ttf, length);
				ttfLengths[i] = length;
			}

			// Copy Tables
			int headPos = -1;
			for (int i = 0; i < tableCount; ++i)
			{
				int pos = ftell(ttf);

				// copy table data.
				CopyTableDataTo(ttc, ttf, ttfOffsets[i], ttfLengths[i]);

				// Write head table special info
				if (i == headIndex)
				{
					fseek(ttf, pos + 8, SEEK_SET);
					WriteLong(ttf, 0);
					headPos = pos;
				}

				// Write table end.
				fseek(ttf, 12 + i * 16 + 8, SEEK_SET);
				WriteLong(ttf, pos);
				fseek(ttf, 0, SEEK_END);
			}
			if (headPos != -1)
			{
				unsigned int checksum = CheckSum(ttf);
				checksum = 0xb1b0afba - checksum;
				fseek(ttf, headPos + 2 * sizeof(int), SEEK_SET);
				WriteLong(ttf, checksum);
			}
		}

		inline void WriteTTF(FILE* ttc, TTC::TTF& ttf, int ttfOffset)
		{
			// Move to ttf begin.
			fseek(ttc, ttfOffset, SEEK_SET);

			// Copy File Info
			int ttfVersion = ReadLong(ttc);
			WriteLong(ttf, ttfVersion);

			int tableCount = ReadShort(ttc);
			WriteShort(ttf, tableCount);

			int other1 = ReadShort(ttc);
			WriteShort(ttf, other1);

			int other2 = ReadShort(ttc);
			WriteShort(ttf, other2);

			int other3 = ReadShort(ttc);
			WriteShort(ttf, other3);

			// Copy Table infos.
			std::vector<int> ttfOffsets(tableCount);
			std::vector<int> ttfLengths(tableCount);
			int headIndex = -1;
			for (size_t i = 0; i < tableCount; i++)
			{
				int tag = ReadLong(ttc);
				WriteLong(ttf, tag);
				if (tag == MakeInt('h', 'e', 'a', 'd'))
					headIndex = i;

				int checksum = ReadLong(ttc);
				WriteLong(ttf, checksum);

				int offset = ReadLong(ttc);
				WriteLong(ttf, offset);
				ttfOffsets[i] = offset;

				int length = ReadLong(ttc);
				WriteLong(ttf, length);
				ttfLengths[i] = length;
			}

			// Copy Tables
			int headPos = -1;
			for (int i = 0; i < tableCount; ++i)
			{
				int pos = ttf.size();

				// copy table data.
				CopyTableDataTo(ttc, ttf, ttfOffsets[i], ttfLengths[i]);

				// Write head table special info
				if (i == headIndex)
				{
					WriteLong(ttf, 0, pos + 8);
					headPos = pos;
				}

				// Write table end.
				WriteLong(ttf, pos, 12 + i * 16 + 8);
			}
			if (headPos != -1)
			{
				unsigned int checksum = CheckSum(ttf);
				checksum = 0xb1b0afba - checksum;
				WriteLong(ttf, checksum, headPos + 2 * sizeof(int));
			}
		}
	}

	inline bool UnzipTTc(const std::string& filename, const std::string& dir, const std::string& noNameReplacePrefix)
	{
		FILE* ttc = nullptr;
		if (fopen_s(&ttc, filename.c_str(), "rb") != 0 || !ttc)
		{
			// Can't open.
			return false;
		}
		
		int fileType = details::ReadLong(ttc);
		if (fileType != details::MakeInt('t','t','c','f'))
		{
			// It's not ttc File;
			fclose(ttc);
			return false;
		}

		int version = details::ReadLong(ttc);
		if (version != 0x10000 && version != 0x20000)
		{
			// Bad version.
			fclose(ttc);
			return false;
		}

		int ttfCount = details::ReadLong(ttc);
		if (ttfCount <= 0)
		{
			// Empty File
			fclose(ttc);
			return false;
		}

		std::vector<int> offsets(ttfCount);
		for (int i = 0; i < ttfCount; ++i)
		{
			int offset = details::ReadLong(ttc);
			offsets[i] = offset;
		}

		for (int i = 0 ; i < ttfCount; ++i)
		{
			int offset = offsets[i];
			char ttfName[_MAX_PATH];
			sprintf_s(ttfName, _MAX_PATH, "%s\\%s_%02d.ttf", dir.c_str(), noNameReplacePrefix.c_str(), i);
			
			FILE* ttf = nullptr;
			if (fopen_s(&ttf, ttfName, "w+b") != 0 || !ttf)
				continue;

			details::WriteTTF(ttc, ttf, offset);
			fclose(ttf);
		}

		fclose(ttc);
	}

	inline bool unzipTTC_Memery(const std::string& filename, TTC& ttcData)
	{
		FILE* ttc = nullptr;
		if (fopen_s(&ttc, filename.c_str(), "rb") != 0 || !ttc)
		{
			// Can't open.
			return false;
		}

		int fileType = details::ReadLong(ttc);
		if (fileType != details::MakeInt('t', 't', 'c', 'f'))
		{
			// It's not ttc File;
			fclose(ttc);
			return false;
		}

		int version = details::ReadLong(ttc);
		if (version != 0x10000 && version != 0x20000)
		{
			// Bad version.
			fclose(ttc);
			return false;
		}

		int ttfCount = details::ReadLong(ttc);
		if (ttfCount <= 0)
		{
			// Empty File
			fclose(ttc);
			return false;
		}

		std::vector<int> offsets(ttfCount);
		for (int i = 0; i < ttfCount; ++i)
		{
			int offset = details::ReadLong(ttc);
			offsets[i] = offset;
		}

		TTC tempData;
		tempData.ttfs.resize(ttfCount);
		for (int i = 0; i < ttfCount; ++i)
		{
			int offset = offsets[i];
			details::WriteTTF(ttc, tempData.ttfs[i], offset);
		}

		fclose(ttc);
	}
}