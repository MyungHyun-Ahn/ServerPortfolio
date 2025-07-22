#pragma once

#pragma warning(disable : 4244) // xutility

#include <windows.h>
#include <cstdio>
#include <string>
#include <unordered_map>

namespace MHLib::utils
{
	// �̱� �����忡���� ���ٵ� ��
	class CFileLoader
	{
	public:
		// �⺻������ std::wstring ���� ����
		VOID Parse(const WCHAR *fileName) noexcept
		{
			size_t retVal;

			FILE *pFile = _wfsopen(fileName, L"r, ccs = UTF-16LE", _SH_DENYWR);
			if (pFile == nullptr)
			{
				wprintf(L"file open fail, errorCode = %d\n", GetLastError());
				return;
			}

			size_t fileBytes;
			fseek(pFile, 0, SEEK_END);
			fileBytes = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);

			WCHAR *fileBuffer = new WCHAR[fileBytes / 2]; // wchar�ϱ�
			retVal = fread_s(fileBuffer, fileBytes, sizeof(char), fileBytes, pFile);
			if (retVal == 0)
			{
				// error
				return;
			}

			fclose(pFile);

			fileBuffer[retVal / sizeof(WCHAR)] = L'\0';

			std::wstring wStr(fileBuffer);

			// wstring ���� ������ٸ� ���� ���� ����
			delete[] fileBuffer;

			// \t\r ������� ����
			wStr.erase(remove(wStr.begin(), wStr.end(), L'\t'), wStr.end());
			wStr.erase(remove(wStr.begin(), wStr.end(), L' '), wStr.end());

			// �ּ� ���� "//"
			{
				size_t xPos = wStr.find(L"//");
				while (xPos != std::wstring::npos)
				{
					// nPos ����
					size_t yPos;
					for (yPos = xPos; yPos < wStr.size(); yPos++)
					{
						if (wStr[yPos] == L'\n')
							break;
					}

					// \n ���� 1�� �߰� + 1
					wStr.erase(xPos, yPos - xPos + 1);

					// ���⵵ �������� ok
					xPos = wStr.find(L"//");
				}
			}
			// ���� �ּ��� �������� \n ���� ������ ��
			wStr.erase(remove(wStr.begin(), wStr.end(), L'\n'), wStr.end());

			// /* ~ */ ����
			{
				size_t xPos = wStr.find(L"/*");
				size_t yPos = wStr.find(L"*/");
				while (xPos != std::wstring::npos)
				{
					// */�� ���� �ʾҴٸ� ���� ��� ���� ��������
					if (yPos == std::wstring::npos)
						yPos = wStr.size() - 1;

					// */ ���� 2���ϱ� + 2
					wStr.erase(xPos, yPos - xPos + 2);

					// ���� �������ϱ� ok
					xPos = wStr.find(L"/*", xPos);
					yPos = wStr.find(L"*/", yPos);
				}
			}

			// ��� Key Value�� std::wstring���� ����
			//	* Get �迭 �Լ����� ���ϴ� Ÿ������ ��ȯ

			// ClassName ������ :���� ����
			{
				size_t keyStart = wStr.find(L"-"); // ù -
				size_t valueEnd = 0;
				size_t xPos = 1; // wStr�� �� �� ��ŵ
				size_t yPos = 0;
				size_t nextYPos = wStr.find(L":");
				while (true)
				{
					yPos = nextYPos;
					if (yPos == std::wstring::npos)
						break;

					// Ŭ���� �̸�
					std::wstring className = wStr.substr(xPos, yPos - xPos);

					// �� �ڿ� ���� ���� - Key = Value;
					// -�� ã��
					nextYPos = wStr.find(L":", nextYPos + 1);
					while (keyStart != std::wstring::npos)
					{
						// ���� className ������ keyStart�� ������ ���
						if (nextYPos < keyStart)
							break;

						size_t keyEnd = wStr.find(L"=", keyStart);
						keyStart++;
						std::wstring keyStr = wStr.substr(keyStart, keyEnd - keyStart);

						size_t valueStart = keyEnd + 1;
						valueEnd = wStr.find(L";", valueEnd + 1);
						std::wstring valueStr = wStr.substr(valueStart, valueEnd - valueStart);

						m_parsedDatas[className].insert(std::make_pair(keyStr, valueStr));

						keyStart = wStr.find(L"-", keyStart);
					}

					xPos = valueEnd + 1;
				}
			}

			return;
		}

		// �ʿ��ϸ� �� ������ ��
		BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, USHORT *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(classStr);
			if (classIt == m_parsedDatas.end())
			{
				// class ����
				return FALSE;
			}

			auto keyIt = classIt->second.find(keyStr);
			if (keyIt == classIt->second.end())
			{
				// value ����
				return FALSE;
			}

			INT out = _wtoi(keyIt->second.c_str());
			*outValue = (USHORT)out;
			return TRUE;
		}
		BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, INT *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(classStr);
			if (classIt == m_parsedDatas.end())
			{
				// class ����
				return FALSE;
			}

			auto keyIt = classIt->second.find(keyStr);
			if (keyIt == classIt->second.end())
			{
				// value ����
				return FALSE;
			}

			INT out = _wtoi(keyIt->second.c_str());
			*outValue = out;
			return TRUE;
		}
		BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, unsigned int *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(classStr);
			if (classIt == m_parsedDatas.end())
			{
				// class ����
				return FALSE;
			}

			auto keyIt = classIt->second.find(keyStr);
			if (keyIt == classIt->second.end())
			{
				// value ����
				return FALSE;
			}

			unsigned int out = _wtoi(keyIt->second.c_str());
			*outValue = out;
			return TRUE;
		}
		BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, BYTE *outValue) noexcept
		{
			auto classIt = m_parsedDatas.find(classStr);
			if (classIt == m_parsedDatas.end())
			{
				// class ����
				return FALSE;
			}

			auto keyIt = classIt->second.find(keyStr);
			if (keyIt == classIt->second.end())
			{
				// value ����
				return FALSE;
			}

			BYTE out = _wtoi(keyIt->second.c_str());
			*outValue = out;
			return TRUE;
		}
		BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::string *str) noexcept
		{
			auto classIt = m_parsedDatas.find(classStr);
			if (classIt == m_parsedDatas.end())
			{
				// class ����
				return FALSE;
			}

			auto keyIt = classIt->second.find(keyStr);
			if (keyIt == classIt->second.end())
			{
				// value ����
				return FALSE;
			}

			str->assign(keyIt->second.begin(), keyIt->second.end());
			return TRUE;
		}
		BOOL		Load(const WCHAR *classStr, const WCHAR *keyStr, std::wstring *wstr) noexcept
		{
			auto classIt = m_parsedDatas.find(classStr);
			if (classIt == m_parsedDatas.end())
			{
				// class ����
				return FALSE;
			}

			auto keyIt = classIt->second.find(keyStr);
			if (keyIt == classIt->second.end())
			{
				// value ����
				return FALSE;
			}
			wstr->assign(keyIt->second.begin(), keyIt->second.end());
			return TRUE;
		}

	private:
		// �Ľ��ؼ� �����͸� ������ ���� �ڷᱸ��
		std::unordered_map<std::wstring, std::unordered_map<std::wstring, std::wstring>> m_parsedDatas;
	};
}