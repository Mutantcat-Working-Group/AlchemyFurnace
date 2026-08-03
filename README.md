<div align=center>
<img src="https://s2.loli.net/2024/04/10/PGYR7DdUOcZp5re.jpg" style="width:100px;"/>
<h2>炼丹炉</h2>
</div>

### 一、功能简述

炼丹炉(AlchemyFurnace,简称 af)是一个**主动向外部发送通知消息**的轻量库,核心特点:

- **多语言**:提供 Python / Java / Go / C++ / Rust / TypeScript / PHP / C# 八种语言实现,API 语义一致
- **零封装**:直接以代码文件形式交付,大多数语言只需一个文件即可集成
- **多通道**:支持**钉钉机器人**、**邮箱**、**Server酱**三种通知方式
- **零抛异常**:所有网络请求库内部兜底,不会因通知失败导致宿主程序崩溃
- **凭据安全**:示例一律通过环境变量读取敏感信息,避免硬编码

典型场景:训练模型时推送日志、服务异常时发告警、定时任务完成通知等。运行设备需要联网。

### 二、语言支持

| 语言   | 实现 | 版本号       | 单文件集成 | 第三方依赖 |
| ------ | ---- | ------------ | ---------- | ---------- |
| Python | ✅   | 1.0.20260803 | ✅         | `requests` |
| Java   | ✅   | 1.0.20260803 | ✅(单 .java) | `org.json` + `javax.mail`(Maven) |
| Go     | ✅   | 1.0.20260803 | ✅         | 仅标准库 |
| C++    | ✅   | 1.0.20260803 | ✅(单 .hpp) | libcurl + OpenSSL + nlohmann/json |
| Rust   | ✅   | 1.0.20260803 | ✅(单 .rs) | reqwest + lettre(Cargo) |
| TypeScript | ✅ | 1.0.20260803 | ✅(单 .ts) | `nodemailer`(可选,邮箱时需要) |
| PHP    | ✅   | 1.0.20260803 | ✅(单 .php) | `phpmailer/phpmailer`(Composer) |
| C#     | ✅   | 1.0.20260803 | ✅(单 .cs) | `MailKit`(NuGet) |

各语言的具体依赖、集成方式、完整示例代码见 **[多语言版文档](./README_MULTI_LANG.md)**。

### 三、快速上手

所有语言版本的调用模式一致:

```text
AlchemyFurnace(notice_way, token, secret0, secret1)
af.send_message(title, message[, to])   # 发送消息
af.send_message_at(title, message)      # 钉钉 @所有人(仅 dingbot 模式)
af.get_ding_image_mediaid(img_path)     # 上传图片(仅 dingbot 模式)
```

参数在不同 `notice_way` 下的含义:

| 参数     | dingbot                       | email                             | serverchan / sct                  |
| -------- | ----------------------------- | --------------------------------- | --------------------------------- |
| token    | 钉钉机器人 access_token 后半段 | SMTP 服务器地址,格式 `host:port`  | Server酱 SendKey                  |
| secret0  | 加签密钥                      | 发件人邮箱密码/授权码             | 可选,推送渠道 `channel`(`\|` 分隔) |
| secret1  | 机器人 AppKey(上传图片时需要) | 发件人邮箱地址                    | 可选,微信接收者 `openid`          |

#### 邮箱模式

在邮箱后台开启 SMTP 并获得**授权码**(非登录密码)。以 QQ 邮箱为例:

```python
from AlchemyFurnace import AlchemyFurnace

af = AlchemyFurnace(
    notice_way="email",
    token="smtp.qq.com:465",
    secret0="你的邮箱授权码",
    secret1="sender@qq.com",
)
af.send_message("标题", "正文")                      # 默认发给自己
af.send_message("标题", "正文", to="other@xx.com")   # 发给指定收件人
```

#### Server酱模式

在 [Server酱官网](https://sct.ftqq.com/) 获取 SendKey:

```python
from AlchemyFurnace import AlchemyFurnace

af = AlchemyFurnace(
    notice_way="serverchan",
    token="SCTxxxxxxxxxxxxxxxx",
    secret0="",  # 可选:渠道
    secret1="",  # 可选:微信 openid
)
af.send_message("标题", "正文,支持 **Markdown**")
```

#### 钉钉模式

在[钉钉开放平台](https://open.dingtalk.com/)创建机器人,获取 access_token、加签密钥、AppKey:

```python
from AlchemyFurnace import AlchemyFurnace

af = AlchemyFurnace(
    notice_way="dingbot",
    token="9b0e99c6927d659...",                  # access_token 后半段
    secret0="JOSLGXrxc1OpN9lMh74ZRjz2jY93...",   # 加签密钥
    secret1="dingevhxws5o44rbhpbd",              # AppKey(上传图片时需要)
)
af.send_message("标题", "正文,支持 **Markdown**")
af.send_message_at("标题", "正文")                # @所有人
af.send_message("标题", "![image](...)")         # 带图片
```

参照 `python/Example-DingBot.py`(其他语言目录也各有 `Example` 文件)。

### 四、凭据管理

**严禁将 token / 授权码硬编码到代码中**。各语言示例一律通过环境变量读取:

| 环境变量            | 用途                          |
| ------------------- | ----------------------------- |
| `DINGBOT_TOKEN`     | 钉钉机器人 access_token       |
| `DINGBOT_SECRET`    | 钉钉加签密钥                  |
| `DINGBOT_APPKEY`    | 钉钉机器人 AppKey(上传图片用) |
| `EMAIL_SMTP`        | SMTP 服务器地址 `host:port`   |
| `EMAIL_PASSWORD`    | 发件人邮箱授权码              |
| `EMAIL_USER`        | 发件人邮箱地址                |
| `SERVERCHAN_KEY`    | Server酱 SendKey              |

每个语言目录下都提供了 `.env.example` 占位模板和 `.gitignore`(已忽略 `.env` 文件),可在本地复制为 `.env` 后填入真实值。

### 五、项目结构

```
.
├── python/       # Python 3,AlchemyFurnace.py + Example-DingBot.py
├── java/         # Java 8+ / Maven
├── go/           # Go 1.x,仅标准库
├── c-cpp/        # C++11,libcurl + OpenSSL + nlohmann/json
├── rust/         # Rust 2021 / Cargo
├── typescript/   # TypeScript / JavaScript(Node 18+)
├── php/          # PHP 7.4+ / Composer
├── csharp/       # C# / .NET 7+
├── README.md
├── README_MULTI_LANG.md  # 各语言详细文档
└── CHANGELOG.md
```

### 六、开发进度

- [x] 通过钉钉机器人发送消息
- [x] 通过邮箱发送消息
- [x] 通过 Server酱(Turbo)发送消息
- [x] Java / Go / C++ / Rust / TypeScript / PHP / C# 多语言版
- [ ] 更多通知方式(企业微信、飞书、Telegram 等,欢迎贡献)

### 七、相关项目

- [Echoes(回声)](https://github.com/MutantCat-Working-Group/Echoes) — 同组织配套项目
- [多语言版详细文档](./README_MULTI_LANG.md) — 依赖、集成方式、各语言示例代码、功能对照表
