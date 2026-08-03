// 炼丹炉 - 多通知方式统一入口(C++ 版本)
// https://github.com/MutantCat-Working-Group/AlchemyFurnace
//
// 依赖:
//   - libcurl (HTTP)
//   - OpenSSL (HMAC-SHA256、TLS、base64)
//   - nlohmann/json (JSON 解析,header-only,单文件 json.hpp 约 250KB)
//
// 编译示例(macOS 已自带 curl/openssl):
//   brew install nlohmann-json
//   g++ -std=c++11 -O2 AlchemyFurnace.cpp -o af_test \
//       -lcurl -lssl -lcrypto -I/usr/local/include \
//       Example.cpp
//
// 参数语义(按 notice_way 不同):
//   notice_way : dingbot | email | serverchan/sct
//   token      : dingbot    → 钉钉机器人 access_token 后半段
//                email      → SMTP 服务器地址,格式 host:port(如 smtp.qq.com:465)
//                serverchan → Server酱 SendKey
//   secret0    : dingbot    → 加签密钥
//                email      → 发件人邮箱密码/授权码
//                serverchan → 可选,推送渠道 channel(多个 | 分隔)
//   secret1    : dingbot    → 机器人 AppKey(上传图片时需要)
//                email      → 发件人邮箱地址
//                serverchan → 可选,微信接收者 openid

#ifndef AF_ALCHEMY_FURNACE_HPP
#define AF_ALCHEMY_FURNACE_HPP

#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <cstring>
#include <iostream>

#include <curl/curl.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <nlohmann/json.hpp>

namespace af {

using json = nlohmann::json;

// ============================================================
// 字符串工具
// ============================================================
static inline std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
static inline std::string url_encode(const std::string &v) {
    CURL *curl = curl_easy_init();
    if (!curl) return v;
    char *out = curl_easy_escape(curl, v.c_str(), (int)v.size());
    std::string r = out ? out : v;
    curl_free(out);
    curl_easy_cleanup(curl);
    return r;
}
static inline std::string base64_encode(const unsigned char *p, size_t len) {
    BIO *bio = BIO_new(BIO_s_mem());
    BIO *b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, p, (int)len);
    BIO_flush(bio);
    BUF_MEM *bptr = nullptr;
    BIO_get_mem_ptr(bio, &bptr);
    std::string out(bptr->data, bptr->length);
    BIO_free_all(bio);
    return out;
}
static inline std::string hmac_sha256_base64(const std::string &key, const std::string &data) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned int out_len = 0;
    HMAC(EVP_sha256(), key.data(), (int)key.size(),
         reinterpret_cast<const unsigned char *>(data.data()), data.size(),
         out, &out_len);
    return base64_encode(out, out_len);
}
static inline std::string replace_all(std::string s, const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}
static inline std::string now_ms_str() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(ms);
}

// ============================================================
// HTTP 客户端(libcurl 封装)
// ============================================================
struct HttpBody {
    std::string content_type;
    std::string body;
};
struct HttpResponse {
    long status = 0;
    std::string body;
};
static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *s = reinterpret_cast<std::string *>(userdata);
    s->append(reinterpret_cast<char *>(ptr), size * nmemb);
    return size * nmemb;
}
static HttpResponse http_request(const std::string &url,
                                const std::string &method = "GET",
                                const HttpBody *req = nullptr,
                                long timeout_s = 15) {
    HttpResponse resp;
    CURL *curl = curl_easy_init();
    if (!curl) return resp;
    std::string buf;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_s);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    struct curl_slist *headers = nullptr;
    if (req && method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req->body.size());
        if (!req->content_type.empty()) {
            headers = curl_slist_append(headers, ("Content-Type: " + req->content_type).c_str());
        }
    }
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    CURLcode rc = curl_easy_perform(curl);
    if (headers) curl_slist_free_all(headers);
    if (rc != CURLE_OK) {
        std::cerr << "http_request 失败: " << curl_easy_strerror(rc) << std::endl;
        curl_easy_cleanup(curl);
        return resp;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
    resp.body = buf;
    curl_easy_cleanup(curl);
    return resp;
}

// ============================================================
// 钉钉机器人
// ============================================================
class DingBot {
public:
    DingBot(const std::string &app_key = "", const std::string &token = "", const std::string &secret = "")
        : app_key_(app_key), token_(token), secret_(secret) {}

    void send_markdown(const std::string &title, const std::string &message, bool at_all = false) {
        json body;
        body["msgtype"] = "markdown";
        body["markdown"] = json{{"title", title}, {"text", message}};
        body["at"] = json{{"atMobiles", json::array()}, {"isAtAll", at_all}};
        std::string url = "https://oapi.dingtalk.com/robot/send?access_token=" + token_ + get_digest();
        HttpBody req{"application/json", body.dump()};
        http_request(url, "POST", &req);
    }

    std::string get_mediaid(const std::string &img) {
        std::string token_url = "https://oapi.dingtalk.com/gettoken?appkey=" + app_key_ + "&app_secret=" + secret_;
        auto r = http_request(token_url);
        if (r.status != 200 || r.body.empty()) {
            std::cerr << "DingBot.get_mediaid 获取 token 失败 status=" << r.status << std::endl;
            return "error";
        }
        try {
            json t = json::parse(r.body);
            std::string access = t.value("access_token", "");
            if (access.empty()) { std::cerr << "DingBot.get_mediaid 未获取到 access_token\n"; return "error"; }
            std::string upload_url = "https://oapi.dingtalk.com/media/upload?access_token=" + access + "&type=image";
            auto r2 = http_upload(upload_url, "media", img);
            if (r2.status != 200) { std::cerr << "DingBot.get_mediaid 上传失败 status=" << r2.status << std::endl; return "error"; }
            json u = json::parse(r2.body);
            return u.value("media_id", "error");
        } catch (const std::exception &e) {
            std::cerr << "DingBot.get_mediaid 解析失败: " << e.what() << std::endl;
            return "error";
        }
    }

private:
    std::string get_digest() {
        std::string ts = now_ms_str();
        std::string sign = hmac_sha256_base64(secret_, ts + "\n" + secret_);
        return "&timestamp=" + ts + "&sign=" + url_encode(sign);
    }

    HttpResponse http_upload(const std::string &url, const std::string &field, const std::string &path) {
        HttpResponse resp;
        CURL *curl = curl_easy_init();
        if (!curl) return resp;
        curl_mime *mime = curl_mime_init(curl);
        curl_mimepart *part = curl_mime_addpart(mime);
        curl_mime_name(part, field.c_str());
        curl_mime_filedata(part, path.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        std::string buf;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        CURLcode rc = curl_easy_perform(curl);
        if (rc != CURLE_OK) std::cerr << "http_upload 失败: " << curl_easy_strerror(rc) << std::endl;
        else curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
        resp.body = buf;
        curl_mime_free(mime);
        curl_easy_cleanup(curl);
        return resp;
    }

    std::string app_key_;
    std::string token_;
    std::string secret_;
};

// ============================================================
// 邮箱(基于 libcurl SMTP)
// ============================================================
class Email {
public:
    Email(const std::string &smtp_server = "", const std::string &password = "", const std::string &user = "")
        : user_(user), password_(password) {
        if (smtp_server.find(':') != std::string::npos) {
            auto pos = smtp_server.rfind(':');
            host_ = smtp_server.substr(0, pos);
            try { port_ = std::stoi(smtp_server.substr(pos + 1)); } catch (...) { port_ = 0; }
        } else {
            host_ = smtp_server;
            port_ = 0;
        }
    }

    bool send(const std::string &subject, const std::string &content, const std::string &to = "") {
        if (host_.empty() || user_.empty() || password_.empty()) {
            std::cerr << "Email.send 失败: 未配置 SMTP/发件人/密码\n";
            return false;
        }
        std::string to_addr = to.empty() ? user_ : to;
        std::vector<int> ports;
        if (port_ > 0) ports.push_back(port_);
        else ports = {465, 587, 25};

        for (int p : ports) {
            if (try_port(p, subject, content, to_addr)) return true;
        }
        std::cerr << "Email.send 全部加密方式均失败\n";
        return false;
    }

private:
    bool try_port(int port, const std::string &subject, const std::string &content, const std::string &to) {
        std::string url;
        bool ssl = (port == 465);
        if (ssl) url = "smtps://" + host_ + ":" + std::to_string(port);
        else url = "smtp://" + host_ + ":" + std::to_string(port);

        CURL *curl = curl_easy_init();
        if (!curl) return false;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERNAME, user_.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());
        curl_easy_setopt(curl, CURLOPT_USE_SSL, ssl ? CURLUSESSL_ALL : CURLUSESSL_TRY);
        if (port == 587) {
            curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        struct curl_slist *rcpts = curl_slist_append(nullptr, to.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, rcpts);

        std::string payload = build_payload(subject, content, to);
        payload_.assign(payload.begin(), payload.end());
        pos_ = 0;
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_cb);
        curl_easy_setopt(curl, CURLOPT_READDATA, this);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, (long)payload_.size());

        std::string err;
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_buf_);
        err_buf_[0] = 0;
        CURLcode rc = curl_easy_perform(curl);
        curl_slist_free_all(rcpts);
        curl_easy_cleanup(curl);
        if (rc != CURLE_OK) {
            return false;
        }
        return true;
    }

    std::string build_payload(const std::string &subject, const std::string &content, const std::string &to) const {
        std::ostringstream ss;
        ss << "From: " << user_ << "\r\n";
        ss << "To: " << to << "\r\n";
        ss << "Subject: " << subject << "\r\n";
        ss << "Content-Type: text/plain; charset=UTF-8\r\n";
        ss << "Content-Transfer-Encoding: 8bit\r\n";
        ss << "\r\n" << content << "\r\n";
        return ss.str();
    }

    static size_t read_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
        auto *self = reinterpret_cast<Email *>(userdata);
        size_t total = size * nmemb;
        size_t remain = self->payload_.size() - self->pos_;
        size_t n = total < remain ? total : remain;
        if (n > 0) {
            memcpy(ptr, self->payload_.data() + self->pos_, n);
            self->pos_ += n;
        }
        return n;
    }

    std::string host_;
    int port_ = 0;
    std::string user_;
    std::string password_;
    mutable std::vector<char> payload_;
    mutable size_t pos_ = 0;
    char err_buf_[CURL_ERROR_SIZE];
};

// ============================================================
// Server酱(Turbo)
// ============================================================
class ServerChan {
public:
    ServerChan(const std::string &sendkey = "", const std::string &channel = "", const std::string &openid = "")
        : sendkey_(sendkey), channel_(channel), openid_(openid) {}

    bool send(const std::string &title, const std::string &desp = "") const {
        if (sendkey_.empty()) { std::cerr << "ServerChan.send 失败: 未配置 SendKey\n"; return false; }
        if (title.empty() && desp.empty()) { std::cerr << "ServerChan.send 失败: title 与 desp 不能同时为空\n"; return false; }
        std::string body;
        append(body, "title", title);
        append(body, "desp", desp);
        append(body, "channel", channel_);
        append(body, "openid", openid_);
        std::string url = "https://sctapi.ftqq.com/" + sendkey_ + ".send";
        HttpBody req{"application/x-www-form-urlencoded", body};
        auto r = http_request(url, "POST", &req);
        if (r.status != 200) { std::cerr << "ServerChan.send 请求失败 status=" << r.status << std::endl; return false; }
        try {
            json j = json::parse(r.body);
            if (j.value("code", -1) == 0) return true;
            std::cerr << "ServerChan.send 返回失败: " << j.dump() << std::endl;
            return false;
        } catch (const std::exception &e) {
            std::cerr << "ServerChan.send 解析失败: " << e.what() << std::endl;
            return false;
        }
    }

private:
    static void append(std::string &s, const std::string &k, const std::string &v) {
        if (v.empty()) return;
        if (!s.empty()) s += '&';
        s += k + "=" + url_encode(v);
    }
    std::string sendkey_;
    std::string channel_;
    std::string openid_;
};

// ============================================================
// 统一入口
// ============================================================
class AlchemyFurnace {
public:
    AlchemyFurnace(const std::string &notice_way = "",
                   const std::string &token = "",
                   const std::string &secret0 = "",
                   const std::string &secret1 = "")
        : notice_way_(notice_way), token_(token), secret0_(secret0), secret1_(secret1) {}

    void send_message(const std::string &title, const std::string &message, const std::string &to = "") {
        if (notice_way_ == "dingbot") {
            DingBot(token_, secret0_, secret1_).send_markdown(title, message, false);
        } else if (notice_way_ == "email") {
            Email(token_, secret0_, secret1_).send(title, message, to);
        } else if (notice_way_ == "serverchan" || notice_way_ == "sct") {
            ServerChan(token_, secret0_, secret1_).send(title, message);
        } else {
            std::cerr << "AlchemyFurnace: Unknown notice_way\n";
        }
    }

    void send_message_at(const std::string &title, const std::string &message) {
        if (notice_way_ == "dingbot") {
            DingBot(token_, secret0_, secret1_).send_markdown(title, message, true);
        } else {
            std::cerr << "AlchemyFurnace: send_message_at 当前仅支持 dingbot 模式\n";
        }
    }

    std::string get_ding_image_mediaid(const std::string &img) {
        return DingBot(secret1_, "", secret0_).get_mediaid(img);  // secret1=appkey, secret0=secret
    }

private:
    std::string notice_way_;
    std::string token_;
    std::string secret0_;
    std::string secret1_;
};

}  // namespace af

#endif  // AF_ALCHEMY_FURNACE_HPP
