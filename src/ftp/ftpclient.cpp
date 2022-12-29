#include "ftpclient.h"



FTPClient::FTPClient() :
	m_Insecure(false),
	m_NoSignal(false),
	m_ProgressCallbackSet(false),
	m_Port(0),
	m_FtpProtocol(FTP_PROTO::FTP),
	m_Session(nullptr),
	m_Timeout(0),
	m_Active(false)
{
	
}

FTPClient::~FTPClient()
{
	if (m_Session != nullptr) 
	{
		CleanupSession();
	}
}

void FTPClient::replaceString(std::string& StrToReplace, const char& Search, const std::string& ReplacementStr)
{
	if (StrToReplace.empty()) return;
	int searchCount = 0, wordLen = StrToReplace.size() - 1, replaceStrSize = ReplacementStr.size();
	for (auto i : StrToReplace)
	{
		if (i == Search)
		{
			searchCount++;
		}
	}

	int newSize = wordLen + searchCount * (replaceStrSize - 1) + 1;
	int newLength = newSize - 1;
	StrToReplace.resize(newSize);
	for (int i = wordLen; i >= 0; i--)
	{
		if (StrToReplace[i] == Search)
		{
			for (int k = replaceStrSize - 1; k >= 0; k--)
			{
				StrToReplace[newLength--] = ReplacementStr[k];
			}
		}
		else
		{
			StrToReplace[newLength] = StrToReplace[i];
			newLength--;
		}
	}

}

//possible need to use proxy
void FTPClient::SetProxy(const std::string& Proxy)
{
	if (Proxy.empty()) return;

	std::string proxy = Proxy;
	std::transform(proxy.begin(), proxy.end(), proxy.begin(), ::toupper);

	if (proxy.compare(0, 4, "HTTP") != 0)
		m_Proxy = "http://" + Proxy;
	else
		m_Proxy = Proxy;
}

void FTPClient::SetProxyUserPwd(const std::string& ProxyPwd)
{
	m_ProxyPwd = ProxyPwd;
}

bool FTPClient::InitSession(const std::string& Host, const int& Port, const std::string& Login, const std::string& Password,
	const FTP_PROTO& FtpProtocol)
{
	if (Host.empty()) return false;
	if (m_Session) return false; //already connected to one session
	m_Session = curl_easy_init();
	m_Server = Host;
	m_Port = Port;
	m_UserName = Login;
	m_Password = Password;
	m_FtpProtocol = FtpProtocol;
	if (m_Session != nullptr) return true;
	return false;
}

bool FTPClient::CleanupSession() 
{
	if (!m_Session) 
	{
		return false;
	}

	curl_easy_cleanup(m_Session);//clean up
	m_Session = nullptr;//reset

	return true;
}

std::string FTPClient::parseURL(const std::string& URL)
{
	std::string strURL = m_Server + "/" + URL;
	replaceString(strURL, '/', "//");
	replaceString(strURL, ' ', "%20"); //URL encoding
	if (strURL.compare(0, 3, "ftp") != 0 && strURL.compare(0,4,"sftp") != 0)//make sure there is no prefix already
	{
		switch (m_FtpProtocol)
		{
		case FTP_PROTO::FTP:
			strURL = "ftp://" + strURL;
			break;
		case FTP_PROTO::FTPS:
			strURL = "ftps://" + strURL;
			break;
		case FTP_PROTO::FTPES:
			strURL = "ftp://" + strURL;
			break;
		case FTP_PROTO::SFTP:
			strURL = "sftp://" + strURL;
			break;
		default:
			break;
		}
	}
	return strURL;
}



bool FTPClient::DownloadFile(const std::string& LocalFilePathName, const std::string& RemoteFilePathName)
{
	if (LocalFilePathName.empty() || RemoteFilePathName.empty()) return false;
	if (!m_Session)
	{
		return false;
	}
	bool res = false;
	curl_easy_reset(m_Session);
	std::string File = parseURL(RemoteFilePathName);
	std::ofstream ofsOutput;
	ofsOutput.open(
#ifdef __linux__
		LocalFilePathName,
#else
		Utf8ToUtf16(LocalFilePathName),
#endif
		std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
	if (ofsOutput)
	{
		curl_easy_setopt(m_Session, CURLOPT_URL, File.c_str());
		curl_easy_setopt(m_Session, CURLOPT_WRITEFUNCTION, WriteFileCallback);
		curl_easy_setopt(m_Session, CURLOPT_WRITEDATA, &ofsOutput);

		CURLcode result = Perform();

		if (result != CURLE_OK)
		{
		}
		else
			res = true;

		ofsOutput.close();
		if (!res) remove(LocalFilePathName.c_str());
	}
	return res;
}

bool FTPClient::UploadFile(FTPClient::CurlRead cRead , void* InfileStream, const std::string& RemoteFilePathName, curl_off_t FileSize, const bool& Creatdir)
{
	if (cRead == nullptr || RemoteFilePathName.empty())
		return false;

	if (!m_Session) 
	{
		return false;
	}

	//mandatory reset
	curl_easy_reset(m_Session);

	std::string strLocalRemoteFile = parseURL(RemoteFilePathName);

	//set target
	curl_easy_setopt(m_Session, CURLOPT_URL, strLocalRemoteFile.c_str());

	// set read function
	curl_easy_setopt(m_Session, CURLOPT_READFUNCTION, cRead);

	//set file to upload
	curl_easy_setopt(m_Session, CURLOPT_READDATA, InfileStream);

	//set size
	curl_easy_setopt(m_Session, CURLOPT_INFILESIZE_LARGE, FileSize);

	//enable upload
	curl_easy_setopt(m_Session, CURLOPT_UPLOAD, 1L);

	if (Creatdir) curl_easy_setopt(m_Session, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR);

	CURLcode res = Perform();

	bool result = false;

	if (res != CURLE_OK) 
	{
		//log it?
	}
	else
	{
		result = true; 
	}

	return result;
}

bool FTPClient::UploadFile(const std::string& LocalFilePathName, const std::string& RemoteFilePathName, const bool& Creatdir)
{
	if (LocalFilePathName.empty() || RemoteFilePathName.empty()) return false;

	std::ifstream iFile;

	struct stat file_info;
	bool result = false;
#ifdef __linux__
	if (stat(LocalFilePathName.c_str(), &file_info) == 0)
	{
		iFile.open(LocalFilePathName, std::ifstream::in | std::ifstream::binary);
#else
	std::wstring wstrLocalFile = Utf8ToUtf16(LocalFilePathName);
	if (_wstat64i32(wstrLocalFile.c_str(), reinterpret_cast<struct _stat64i32*>(&file_info)) == 0)
	{
		iFile.open(wstrLocalFile, std::ifstream::in | std::ifstream::binary);
#endif
		if (!iFile)
		{
			return result;
		}
		result = UploadFile(ReadStreamCallback, static_cast<void*>(&iFile), RemoteFilePathName, file_info.st_size, Creatdir);
	}
	iFile.close();

	return result;
}


bool FTPClient::DeleteFiles(const std::string& RemoteFile)
{
	if (RemoteFile.empty()) return false;
	std::string strRemoteFolder;
	std::string strRemoteFileName;
	std::string strBuf;
	bool result = false;
	if (!m_Session) 
	{
		return result;
	}
	curl_easy_reset(m_Session);

	struct curl_slist* headerlist = nullptr;
	if (m_FtpProtocol == FTP_PROTO::SFTP) 
	{
		strRemoteFolder = parseURL("");
		strRemoteFileName = RemoteFile;

		strBuf += "rm ";
	}
	else
	{
		std::size_t pos = RemoteFile.find_last_of("/");
		if (pos != std::string::npos)
		{
			strRemoteFolder = parseURL(RemoteFile.substr(0, pos)) + "//";
			strRemoteFileName = RemoteFile.substr(pos + 1);
		}
		else
		{
			strRemoteFolder = parseURL("");
			strRemoteFileName = RemoteFile;
		}
		strBuf += "DELE ";
	}

	curl_easy_setopt(m_Session, CURLOPT_URL, strRemoteFolder.c_str());
	strBuf += strRemoteFileName;
	headerlist = curl_slist_append(headerlist, strBuf.c_str());

	curl_easy_setopt(m_Session, CURLOPT_POSTQUOTE, headerlist);
	curl_easy_setopt(m_Session, CURLOPT_NOBODY, 1L);
	curl_easy_setopt(m_Session, CURLOPT_HEADER, 1L);

	CURLcode res = Perform();

	if (res != CURLE_OK)
	{
		//
	}
	else result = true;

	curl_slist_free_all(headerlist);

	return result;
}

bool FTPClient::CreateDir(const std::string& strDirName)
{
	if (strDirName.empty()) return false;

	if (!m_Session) return false;

	curl_easy_reset(m_Session);

	struct curl_slist* headerlist = nullptr;

	std::string strRemoteFolder;
	std::string strRemoteNewFolderName;
	std::string strBuf;
	bool result = false;

	if (m_FtpProtocol == FTP_PROTO::SFTP) 
	{
		strRemoteFolder = parseURL("");
		strRemoteNewFolderName = strDirName;

		strBuf += "mkdir ";
	}
	else 
	{
		std::size_t pos = strDirName.find_last_of("/");
		if (pos != std::string::npos) {
			strRemoteFolder = parseURL(strDirName.substr(0, pos)) + "//";
			strRemoteNewFolderName = strDirName.substr(pos + 1);
		}
		else
		{
			strRemoteFolder = parseURL("");
			strRemoteNewFolderName = strDirName;
		}
		strBuf += "MKD ";
	}

	curl_easy_setopt(m_Session, CURLOPT_URL, strRemoteFolder.c_str());

	strBuf += strRemoteNewFolderName;
	headerlist = curl_slist_append(headerlist, strBuf.c_str());

	curl_easy_setopt(m_Session, CURLOPT_POSTQUOTE, headerlist);
	curl_easy_setopt(m_Session, CURLOPT_NOBODY, 1L);
	curl_easy_setopt(m_Session, CURLOPT_HEADER, 1L);
	curl_easy_setopt(m_Session, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR);

	curl_easy_setopt(m_Session, CURLOPT_TCP_KEEPALIVE, 0L);

	CURLcode res = Perform();

	if (res != CURLE_OK) 
	{
	}
	else result = true;

	curl_slist_free_all(headerlist);

	return result;
}

bool FTPClient::PeekDir(const std::string& RemoteFilePath, std::string& ReturnList)
{
	if (RemoteFilePath.empty()) return false;

	bool result = false;
	if (!m_Session) 
	{
		//log
		return result;
	}

	curl_easy_reset(m_Session);

	std::string strRemoteFile = parseURL(RemoteFilePath);

	curl_easy_setopt(m_Session, CURLOPT_URL, strRemoteFile.c_str());

	curl_easy_setopt(m_Session, CURLOPT_DIRLISTONLY, 1L);

	curl_easy_setopt(m_Session, CURLOPT_WRITEFUNCTION, WriteStringCallback);
	curl_easy_setopt(m_Session, CURLOPT_WRITEDATA, &ReturnList);

	CURLcode res = Perform();

	if (res != CURLE_OK)
	{
		//log?
	}
	else result = true;

	return result;
}

bool FTPClient::IsFileExists(const std::string& RemoteFilePath, const std::string& FileToFind)
{
	std::string ReturnList;
	PeekDir(RemoteFilePath, ReturnList);
	if (ReturnList.find(FileToFind) != std::string::npos)
	{
		return true;
	}
	return false;
}

size_t FTPClient::WriteStringCallback(void* Buf, size_t Size, size_t Nmemb, void* Stream)
{
	std::string* strWriteHere = reinterpret_cast<std::string*>(Stream);
	if (strWriteHere != nullptr) 
	{
		strWriteHere->append(reinterpret_cast<char*>(Buf), Size* Nmemb);
		return Size * Nmemb;
	}
	return 0;
}

size_t FTPClient::WriteFileCallback(void* Buf, size_t Size, size_t Nmemb, void* Data)
{
	if ((Size == 0) || (Nmemb == 0) || ((Size * Nmemb) < 1) || (Data == nullptr)) return 0;

	std::ofstream* pFileStream = static_cast<std::ofstream*>(Data);
	if (pFileStream->is_open()) 
	{
		pFileStream->write(static_cast<char*>(Buf), Size* Nmemb);
	}

	return Size * Nmemb;
}

size_t FTPClient::ReadStreamCallback(void* Buf, size_t Size, size_t Nmemb, void* Stream)
{
	auto* pFileStream = static_cast<std::istream*>(Stream);
	if (!pFileStream->fail()) 
	{
		pFileStream->read(static_cast<char*>(Buf), Size* Nmemb);
		return pFileStream->gcount();
	}
	return 0;
}


CURLcode FTPClient::Perform()
{
	CURLcode result = CURLE_OK;

	curl_easy_setopt(m_Session, CURLOPT_PORT, m_Port);
	curl_easy_setopt(m_Session, CURLOPT_USERPWD, (m_UserName + ":" + m_Password).c_str());

	if (m_Active) curl_easy_setopt(m_Session, CURLOPT_FTPPORT, "-");

	if (m_Timeout > 0) 
	{
		curl_easy_setopt(m_Session, CURLOPT_TIMEOUT, m_Timeout);
	}
	if (m_NoSignal) 
	{
		curl_easy_setopt(m_Session, CURLOPT_NOSIGNAL, 1L);
	}

	if (!m_Proxy.empty()) 
	{
		curl_easy_setopt(m_Session, CURLOPT_PROXY, m_Proxy.c_str());
		curl_easy_setopt(m_Session, CURLOPT_HTTPPROXYTUNNEL, 1L);

		if (!m_ProxyPwd.empty()) 
		{
			curl_easy_setopt(m_Session, CURLOPT_PROXYUSERPWD, m_ProxyPwd.c_str());
		}

		if (!m_Active) 
		{
			curl_easy_setopt(m_Session, CURLOPT_FTP_USE_EPSV, 1L);
		}
	}

	if (m_FtpProtocol == FTP_PROTO::FTPS || m_FtpProtocol == FTP_PROTO::FTPES)
		curl_easy_setopt(m_Session, CURLOPT_USE_SSL, CURLUSESSL_ALL);

	if (m_FtpProtocol == FTP_PROTO::SFTP)
	{
		curl_easy_setopt(m_Session, CURLOPT_SSH_AUTH_TYPES, CURLSSH_AUTH_AGENT);
	}


	if (!m_SSLCertFile.empty()) curl_easy_setopt(m_Session, CURLOPT_SSLCERT, m_SSLCertFile.c_str());

	if (!m_SSLKeyFile.empty()) curl_easy_setopt(m_Session, CURLOPT_SSLKEY, m_SSLKeyFile.c_str());

	if (!m_SSLKeyPwd.empty()) curl_easy_setopt(m_Session, CURLOPT_KEYPASSWD, m_SSLKeyPwd.c_str());

	curl_easy_setopt(m_Session, CURLOPT_SSL_VERIFYHOST, (m_Insecure) ? 0L : 2L);
	curl_easy_setopt(m_Session, CURLOPT_SSL_VERIFYPEER, (m_Insecure) ? 0L : 1L);


	// Perform the requested operation
	result = curl_easy_perform(m_Session);


	return result;
}

#ifdef _WIN32
std::wstring FTPClient::Utf8ToUtf16(const std::string& str)
{
	std::wstring ret;
	int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0);
	if (len > 0) 
	{
		ret.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len);
	}
	return ret;
}
#endif
