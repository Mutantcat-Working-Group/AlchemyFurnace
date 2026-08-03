# 炼丹炉多语言版

各语言实现都放在同名子目录下,每个子目录是一个独立、可直接集成到项目中的单元。所有版本都支持三种通知方式(dingbot / email / serverchan),参数语义一致。

## 目录结构

```
.
├── python/       # Python 3(仅依赖 requests)
├── java/         # Java 8+ / Maven(依赖 org.json + javax.mail)
├── go/           # Go 1.x(仅标准库)
├── c-cpp/        # C++11(libcurl + OpenSSL + nlohmann/json)
├── rust/         # Rust 2021 / Cargo(reqwest + lettre)
├── typescript/   # TypeScript / JavaScript(Node 18+,可选 nodemailer)
├── php/          # PHP 7.4+ / Composer(phpmailer/phpmailer)
└── csharp/       # C# / .NET 7+(MailKit)
```

> **Java 注意**:`javax.mail` 在 Java 8 自带,Java 11+ 起需单独引入(`com.sun.mail:javax.mail`)。`pom.xml` 已包含。
>
> **C++ 注意**:`nlohmann/json` 是 header-only,把 `json.hpp` 放进 include 路径即可。macOS 上 `libcurl` 系统自带,Linux 通常预装,若缺请安装 `libcurl-devel`/`libcurl4-openssl-dev`。

## 各版本依赖一览

| 语言   | 依赖                                                                              | 集成方式                                                                       |
| ------ | --------------------------------------------------------------------------------- | ------------------------------------------------------------------------------ |
| Python | `requests`(HTTP),其余标准库                                                       | 把 `AlchemyFurnace.py` 复制进项目,`pip install requests`                       |
| Java   | `org.json`(JSON 解析,jar ~500KB) + `javax.mail`(JavaMail)                         | 把 `java/` 作为 Maven 模块引入;或把 `AlchemyFurnace.java` 连同 jar 一起放进项目 |
| Go     | 仅标准库                                                                          | 把 `AlchemyFurnace.go` 放进任意 package 即可                                   |
| C++    | `libcurl`(HTTP) + `OpenSSL`(TLS/HMAC/base64) + `nlohmann/json`(JSON,header-only)  | 把 `AlchemyFurnace.hpp` 放进 include 路径,编译时链接 `-lcurl -lssl -lcrypto`   |
| Rust   | `reqwest`(HTTP) + `lettre`(SMTP) + `serde`(序列化) + `hmac/sha2`(签名)            | 把 `rust/` 作为 workspace 成员,或在 `Cargo.toml` 添加对应依赖                  |
| TypeScript | Node 18+ 内置 `fetch` + `nodemailer`(邮箱可选)                                | `npm install`(可选 `nodemailer`),直接 `import` `AlchemyFurnace.ts`             |
| PHP    | `curl` 扩展 + `phpmailer/phpmailer`                                               | `composer require phpmailer/phpmailer`,直接 `require` `AlchemyFurnace.php`      |
| C#     | `MailKit`(NuGet) + `System.Net.Http`(.NET 内置)                                   | `dotnet add package MailKit`,把 `src/AlchemyFurnace.cs` 放进项目                |

## 参数语义(所有语言通用)

`AlchemyFurnace(notice_way, token, secret0, secret1)` 四个参数按 `notice_way` 复用:

| 参数        | dingbot                        | email                                | serverchan / sct                     |
| ----------- | ------------------------------ | ------------------------------------ | ------------------------------------ |
| token       | 钉钉机器人 access_token 后半段 | SMTP 服务器地址,格式 `host:port`     | Server酱 SendKey                     |
| secret0     | 加签密钥                       | 发件人邮箱密码/授权码                | 可选,推送渠道 `channel`(多个 `\|` 分隔) |
| secret1     | 机器人 AppKey(上传图片时需要)  | 发件人邮箱地址                       | 可选,微信接收者 `openid`             |

## 使用示例

### Python
```python
from AlchemyFurnace import AlchemyFurnace
af = AlchemyFurnace(notice_way="email", token="smtp.qq.com:465", secret0="授权码", secret1="me@qq.com")
af.send_message("标题", "正文")
af.send_message("标题", "正文", to="other@xx.com")
```
参照 `python/Example-DingBot.py`。

### Java
```java
import af.AlchemyFurnace;
AlchemyFurnace af = new AlchemyFurnace("serverchan", "SCTxxxx", "", "");
af.send_message("标题", "正文");
```
参照 `java/src/main/java/af/Example.java`,运行:`mvn compile exec:java -Dexec.mainClass="af.Example"`。

### Go
```go
import "yourmodule/go"
af := go.NewAlchemyFurnace("dingbot", token, secret, appkey)
af.SendMessage("title", "message", "")
```
参照 `go/cmd/example/main.go`,运行:`cd go && go run ./cmd/example`。

### C++
```cpp
#include "AlchemyFurnace.hpp"
af::AlchemyFurnace af("email", "smtp.qq.com:465", "授权码", "me@qq.com");
af.send_message("标题", "正文");
af.send_message("标题", "正文", "to@xx.com");
```
参照 `c-cpp/Example.cpp`,编译:`g++ -std=c++11 -O2 -I<json.hpp 所在目录> Example.cpp -o af_example -lcurl -lssl -lcrypto`。

### Rust
```rust
use alchemy_furnace::AlchemyFurnace;
let af = AlchemyFurnace::new("serverchan", "SCTxxxx", "", "");
af.send_message("标题", "正文", None);
```
参照 `rust/src/example.rs`,运行:`cd cargo run --bin example`。

### TypeScript
```typescript
import { AlchemyFurnace } from './AlchemyFurnace';
const af = new AlchemyFurnace('email', 'smtp.qq.com:465', '授权码', 'me@qq.com');
await af.sendMessage('标题', '正文');
await af.sendMessage('标题', '正文', 'other@xx.com');
```
参照 `typescript/Example.ts`,运行:`cd typescript && npm install && npx tsx Example.ts`。

### PHP
```php
require_once __DIR__ . '/AlchemyFurnace.php';
use AlchemyFurnace\AlchemyFurnace;
$af = new AlchemyFurnace('serverchan', 'SCTxxxx', '', '');
$af->send_message('标题', '正文');
```
参照 `php/Example.php`,运行:`cd php && composer install && php Example.php`。

### C#
```csharp
using AlchemyFurnace;
var af = new AlchemyFurnace("dingbot", token, secret, appkey);
af.SendMessage("title", "message");
```
参照 `csharp/example/Program.cs`,运行:`cd csharp && dotnet run --project example`。

## 凭据管理

所有示例都通过环境变量读取敏感信息,避免硬编码:

- `DINGBOT_TOKEN` / `DINGBOT_SECRET` / `DINGBOT_APPKEY`
- `EMAIL_SMTP` / `EMAIL_PASSWORD` / `EMAIL_USER`
- `SERVERCHAN_KEY`

每个语言目录下都提供了 `.env.example` 占位模板与 `.gitignore`(忽略 `.env`)。

## 各版本实现对照

| 功能            | Python | Java | Go | C++ | Rust | TS | PHP | C# |
| --------------- | :----: | :--: | :-: | :-: | :--: | :-: | :-: | :-: |
| 钉钉 markdown   |   ✅   |  ✅  | ✅  | ✅  |  ✅  | ✅  | ✅  | ✅  |
| 钉钉 @所有人    |   ✅   |  ✅  | ✅  | ✅  |  ✅  | ✅  | ✅  | ✅  |
| 钉钉上传图片    |   ✅   |  ✅  | ✅  | ✅  |  ✅  | ✅  | ✅  | ✅  |
| 邮箱            |   ✅   |  ✅  | ✅  | ✅  |  ✅  | ✅  | ✅  | ✅  |
| Server酱        |   ✅   |  ✅  | ✅  | ✅  |  ✅  | ✅  | ✅  | ✅  |
| 异常兜底        |   ✅   |  ✅  | ✅  | ✅  |  ✅  | ✅  | ✅  | ✅  |
| 凭据脱敏        |   ✅   |  ✅  | ✅  | ✅  |  ✅  | ✅  | ✅  | ✅  |

## 后续计划

以下语言/通道在计划或欢迎贡献:

- **Kotlin**(JVM,Android)
- **Swift**(Apple 平台)
- **Dart / Flutter**
- **Ruby**
- **C / C**(单独的纯 C 版)
- 通知通道:企业微信、飞书、Telegram Bot、Bark、Slack、PushPlus、Discord Webhook 等

欢迎提交 Pull Request。
