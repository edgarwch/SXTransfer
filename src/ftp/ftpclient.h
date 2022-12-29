#ifndef FTPCLIENT_H_
#define FTPCLIENT_H_
#include <curl/curl.h>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#ifndef __linux__
#include <direct.h> 
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>

class FTPClient
{

public:
	using CurlRead = size_t(*)(void*, size_t, size_t, void*);

	enum class FTP_PROTO:unsigned char
	{
		FTP,
		FTPS,
		FTPES,
		SFTP
	};

	FTPClient();
	~FTPClient();
	//setters
	void SetProxy(const std::string& Proxy);
	void SetProxyUserPwd(const std::string& ProxyPwd);
	void SetTimeout(const int& timeout) {};
	// getters
	std::string   GetProxy() const { return m_Proxy; }
	std::string   GetProxyUserPwd() const { return m_ProxyPwd; }
	int           GetTimeout() const { return m_Timeout; }
	int			  GetPort() const { return m_Port; }
	std::string   GetURL() const { return m_Server; }
	std::string   GetUsername() const { return m_UserName; }
	std::string   GetPassword() const { return m_Password; }
	FTP_PROTO  GetProtocol() const { return m_FtpProtocol; }

	void SetActive(const bool& Enable) { m_Active = Enable; }


	//session
	bool InitSession(const std::string& Host, const int& Port, const std::string& Login, const std::string& Password,
		const FTP_PROTO& FtpProtocol = FTP_PROTO::FTP);
	virtual bool CleanupSession();

	//functions
	//download
	bool DownloadFile(const std::string& LocalFilePathName, const std::string& RemoteFilePathName) ;
	bool DownloadFile(const std::string& LocalFilePathName, std::queue<std::string>& RemoteFilePathName) ;

	//upload
	bool UploadFile(CurlRead Read, void* InfileStream , const std::string& RemoteFilePathName, curl_off_t FileSize ,const bool& Creatdir = false) ;
	bool UploadFile(const std::string& LocalFilePathName, const std::string& RemoteFilePathName, const bool& Creatdir = false);
	
	//delete
	bool DeleteFiles(const std::string& RemoteFile);

	//create
	bool CreateDir(const std::string& DirName);

	//peek
	bool PeekDir(const std::string& RemoteFilePath, std::string& ReturnList);

	bool IsFileExists(const std::string& RemoteFilePath, const std::string& FileToFind);

	static size_t WriteStringCallback(void* Buf, size_t Size, size_t Nmemb, void* Stream);
	static size_t WriteFileCallback(void* Buf, size_t Size, size_t Nmemb, void* Stream);
	static size_t ReadStreamCallback(void* Buf, size_t Size, size_t Nmemb, void* Stream);
	//static size_t ThrowAwayCallback(void* buf, size_t size, size_t nmemb, void* data);
	//static size_t WriteToMemory(void* buf, size_t size, size_t nmemb, void* data);
	
#ifdef _WIN32
	static std::string AnsiToUtf8(const std::string& ansiStr);
	static std::wstring Utf8ToUtf16(const std::string& str);
#endif
private:
	CURLcode Perform();
	std::string parseURL(const std::string& URL);

	void replaceString(std::string& StrToReplace, const char& Search, const std::string& ReplacementStr);

	// SSL
	std::string m_SSLCertFile;
	std::string m_SSLKeyFile;
	std::string m_SSLKeyPwd;

	std::string m_ProxyPwd;
	std::string m_Proxy;
	std::string m_UserName;
	std::string m_Password;
	std::string m_Server;

	FTP_PROTO m_FtpProtocol;

	int m_Port;
	int m_Timeout;

	mutable CURL* m_Session;

	bool m_Active;
	bool m_NoSignal;
	bool m_Insecure;
	bool m_ProgressCallbackSet;

};
#endif // !FTPCLIENT_H_
