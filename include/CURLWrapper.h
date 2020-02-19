#pragma once
#include <string>
#include <map>
#include <regex>
#include <curl/curl.h>
#include <ostream>
#include <sstream>
using std::string;
using std::map;
using std::regex;
using std::stringstream;
namespace Cyan
{
	class CookieContainer : public map<string, string>
	{
	public:
		class CookieValue
		{
			friend class CookieContainer;
			friend std::ostream& operator <<(std::ostream&, const CookieValue&);
		public:
			void operator=(const string& value)
			{
				auto it = cc->find(CookieKey);
				if (it == cc->end())
				{
					cc->insert(std::make_pair(CookieKey, value));
				}
				else
				{
					it->second = value;
				}

			}
			operator string() const
			{
				auto it = cc->find(CookieKey);
				if (it != cc->end())
				{
					return it->second;
				}
				throw "未找到Key对应的Cookie";
			}
		private:
			CookieContainer* cc;
			string CookieKey;
			CookieValue(CookieContainer* value, const string& key) :cc(value), CookieKey(key) {}
		}; // class CookieValue

		CookieContainer() {}
		CookieContainer(const string& cookies)
		{
			parse(cookies);
		}
		void parse(string cookies)
		{
			std::smatch match;
			std::regex pattern(R"(([^=]+)=([^;]+);?\s*)");
			while (std::regex_search(cookies, match, pattern))
			{
				map<string, string>::insert(std::make_pair(match[1].str(), match[2].str()));
				cookies = match.suffix().str();
			}
		}
		CookieValue operator[](const string& CookieKey)
		{
			return CookieValue(this, CookieKey);
		}
		string toString() const
		{
			stringstream ss;
			for (auto& Cookie : *this)
			{
				ss << Cookie.first << "=" << Cookie.second << "; ";
			}
			return ss.str();
		}
		bool exist(const string& CookieKey)
		{
			return (map<string, string>::find(CookieKey) != map<string, string>::end());
		}
		bool remove(const string& CookieKey)
		{
			auto it = map<string, string>::find(CookieKey);
			if (it == map<string, string>::end())
			{
				return false;
			}
			else
			{
				map<string, string>::erase(it);
				return true;
			}
		}
		virtual ~CookieContainer() {}
	}; // class CookieContainer
	std::ostream& operator <<(std::ostream& os, const CookieContainer::CookieValue& v)
	{
		os << static_cast<string>(v);
		return os;
	}
	class HTTP
	{
	public:
		struct Response
		{
			bool Ready = false;
			CURLcode CURLCode;
			string ErrorMsg;
			string Content;
		};
		const string DefaultContentType = "application/x-www-form-urlencoded";
		const string DefaultAccept = "*/*";
		const string DefaultUserAgent = 
			"Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
			"AppleWebKit/537.36 (KHTML, like Gecko) "
			"Chrome/64.0.3282.140 Safari/537.36 Edge/17.17134";
		HTTP():
			curl(nullptr),
			slist(nullptr),
			followRedirect(true),
			timeout(120), 
			maxRedirs(5),
			cookieContainer(),
			contentType(DefaultContentType),
			accept(DefaultAccept)
		{
			errbuf[0] = 0;
			curl_global_init(CURL_GLOBAL_ALL);
		}
		HTTP& FollowRedirect(bool redirect)
		{
			followRedirect = redirect;
			return *this;
		}
		HTTP& SetTimeout(size_t seconds)
		{
			timeout = seconds;
			return *this;
		}
		HTTP& SetMaxRedirs(size_t times)
		{
			maxRedirs = times;
			return *this;
		}
		HTTP& SetCookieContainer(const CookieContainer& cc)
		{
			cookieContainer = cc;
			return *this;
		}
		HTTP& SetContentType(const string& ContentType)
		{
			contentType = ContentType;
			return *this;
		}
		HTTP& SetAccept(const string& Accept)
		{
			accept = Accept;
			return *this;
		}
		HTTP& SetUserAgent(const string& UserAgent)
		{
			userAgent = UserAgent;
			return *this;
		}
		HTTP& AddHeader(const string& name, const string& value)
		{
			string t = name + ": " + value;
			slist = curl_slist_append(slist, t.data());
			return *this;
		}
		CookieContainer& GetCookieContainer()
		{
			return cookieContainer;
		}

		string URLEncode(const string& str)
		{
			auto encoded = curl_easy_escape(curl, str.data(), str.size());	// TODO:错误处理
			return string(encoded);
		}

		const Response Get(const string& URL)
		{
			Response resp;
			if (curl == NULL)
			{
				curl = curl_easy_init();
			}

			// 初始化CURL失败，获得错误描述、清理CURL、返回
			if (!curl)								
			{
				resp.Ready = false;
				resp.ErrorMsg = GetErrorStr();
				resp.CURLCode = curlCode;
				resp.Content = "";
				CURLCleanup();
				return resp;
			}

			string tStr = execute(URL);

			if (curlCode == CURLcode::CURLE_OK)
			{
				resp.Ready = true;
				resp.CURLCode = curlCode;
				resp.ErrorMsg = "";
				resp.Content = tStr;
			}
			else
			{
				resp.Ready = false;
				resp.ErrorMsg = GetErrorStr();
				resp.CURLCode = curlCode;
				resp.Content = "";
			}

			CURLCleanup();
			return resp; 
		}

		const Response Post(const string& URL, const string& Data)
		{
			Response resp;
			if (curl == NULL)
			{
				curl = curl_easy_init();
			}

			// 初始化CURL失败，获得错误描述、清理CURL、返回
			if (!curl)
			{
				resp.Ready = false;
				resp.ErrorMsg = GetErrorStr();
				resp.CURLCode = curlCode;
				resp.Content = "";
				CURLCleanup();
				return resp;
			}

			// Post只是在Get的基础上增加两项设置
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, Data.data());

			string tStr = execute(URL);

			if (curlCode == CURLcode::CURLE_OK)
			{
				resp.Ready = true;
				resp.CURLCode = curlCode;
				resp.ErrorMsg = "";
				resp.Content = tStr;
			}
			else
			{
				resp.Ready = false;
				resp.ErrorMsg = GetErrorStr();
				resp.CURLCode = curlCode;
				resp.Content = "";
			}

			CURLCleanup();
			return resp;
		}
		
		~HTTP()
		{
			curl_slist_free_all(slist);
			curl_global_cleanup();
		}

	private:
		CURL* curl;
		CURLcode curlCode;
		char errbuf[CURL_ERROR_SIZE];
		struct curl_slist* slist;

		bool followRedirect;
		size_t timeout;
		size_t maxRedirs;
		CookieContainer cookieContainer;
		string contentType;
		string accept;
		string userAgent;

		string GetErrorStr()
		{
			string errorMsg;
			size_t len = strlen(errbuf);
			if (len)
				errorMsg = errbuf;
			else
				errorMsg = curl_easy_strerror(curlCode);
			return errorMsg;
		}

		void CURLCleanup()
		{
			curl_easy_cleanup(curl);
			curl = nullptr;
			curl_slist_free_all(slist);
			slist = nullptr;
		}

		static size_t reWriter(char* buffer, size_t size, size_t nmemb, string* content)
		{
			unsigned long sizes = size * nmemb;
			content->append(buffer, sizes);
			return sizes;
		}

		static size_t heWriter(char* buffer, size_t size, size_t nmemb, string* content)
		{
			unsigned long sizes = size * nmemb;
			content->append(buffer, sizes);
			return sizes;
		}

		void AutoCookies(string& reHeader)
		{
			std::smatch match;
			std::regex pattern(R"(Set-Cookie:([^=]+)=([^;]+);?\s*)");
			while (std::regex_search(reHeader, match, pattern))
			{
				cookieContainer.insert(std::make_pair(match[1].str(), match[2].str()));
				reHeader = match.suffix().str();
			}
		}

		string execute(const string& URL)
		{
			string reStr;
			string reHeader;
			// 自定义HTTP头
			slist = curl_slist_append(slist, ("User-Agent: " + userAgent).data());
			slist = curl_slist_append(slist, ("Accept: " + accept).data());
			slist = curl_slist_append(slist, ("ContentType: " + contentType).data());
			// curl基础设置
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
			curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, followRedirect);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout);
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long)maxRedirs);
			curl_easy_setopt(curl, CURLOPT_COOKIE, cookieContainer.toString().data());
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
			// 设置处理返回内容的功能函数
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, reWriter);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&reStr);
			// 设置处理HTTP头部的功能函数
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, heWriter);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*)&reHeader);
			curl_easy_setopt(curl, CURLOPT_URL, URL.data());
			curlCode = curl_easy_perform(curl);
			// 从HTTP头部读取新的Cookie并加入到CookieContainer中
			AutoCookies(reHeader);

			return reStr;
		}

	};
}// namespace Cyan