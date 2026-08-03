// 炼丹炉 C++ 版示例
// g++ -std=c++11 -O2 -I/usr/local/include -I. Example.cpp -o af_example -lcurl -lssl -lcrypto
#include "AlchemyFurnace.hpp"
#include <cstdlib>
#include <iostream>

static std::string env(const char *k, const std::string &def = "") {
    const char *v = std::getenv(k);
    return v ? std::string(v) : def;
}

int main() {
    // 1) 钉钉
    std::string dt = env("DINGBOT_TOKEN");
    if (!dt.empty()) {
        af::AlchemyFurnace af("dingbot", dt, env("DINGBOT_SECRET"), env("DINGBOT_APPKEY"));
        af.send_message("title", "message");
        af.send_message_at("title", "message");
    }

    // 2) 邮箱
    std::string smtp = env("EMAIL_SMTP");
    std::string epwd = env("EMAIL_PASSWORD");
    std::string eusr = env("EMAIL_USER");
    if (!smtp.empty() && !epwd.empty() && !eusr.empty()) {
        af::AlchemyFurnace af("email", smtp, epwd, eusr);
        af.send_message("标题", "正文");
        af.send_message("标题", "正文", "to@xx.com");

        af::Email email(smtp, epwd, eusr);
        email.send("测试", "Hello from C++ AlchemyFurnace", "to@xx.com");
    }

    // 3) Server酱
    std::string sc = env("SERVERCHAN_KEY");
    if (!sc.empty()) {
        af::AlchemyFurnace af("serverchan", sc, "", "");
        af.send_message("标题", "正文,支持 **Markdown**");

        af::ServerChan ch(sc, "", "");
        ch.send("测试", "Hello from C++ AlchemyFurnace");
    }
    return 0;
}
