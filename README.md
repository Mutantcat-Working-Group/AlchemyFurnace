<div align=center>
<img src="https://s2.loli.net/2024/04/10/PGYR7DdUOcZp5re.jpg" style="width:100px;"/>
<h2>炼丹炉</h2>
</div>

### 一、功能简述
- 各种编程语言的主动向外部发送信息的库，部分模式支持发送图片，以下简称为af。
- 直接以代码而不是包装后的各种形式提供，大多只有一个文件。
- 需要运行设备支持联网！训练模型时候的控制台日志，服务运行异常报错等均可使用本库发送提醒消息。

### 二、使用方法

1. 在你的程序中添加对应的程序文件。
2. 在代码中引入功能并按说明简单调用即可。
3. 请注意，如果使用的是带异常处理机制的编程语言，请在af操作时进行异常处理，因为网络请求可能出现一些特殊情况，导致的运行时异常或许会导致整个程序停止运行。

### 三、语言支持

| 语言   | 是否实现 | 版本号     | 说明                                       |
| ------ | -------- | ---------- | ------------------------------------------ |
| Python | ✅        | 1.0.260803 | 支持钉钉机器人、邮箱、Server酱通知方式 |
| Java   |          |            |                                            |
| Go     |          |            |                                            |
| C/C++  |          |            |                                            |
| Rust   |          |            |                                            |

### 四、详细说明

1. Python版本

   - 使用方法可参照Example.py

   - 功能列表

     | 性质 | 方法名                 | 参数列表                            | 说明                                                         |
     | ---- | ---------------------- | ----------------------------------- | ------------------------------------------------------------ |
     | 构造 | AlchemyFurnac          | notice_way, token, secret0, secret1 | af类的构造方法，每个参数都可缺省，但是注意标明参数名，并且留意自己需要的通知方式所需的参数，否则会造成后续操作的异常<br/>notice_way: 通知方式 （dingbot...）<br/>token: 发送的token或地址等信息<br/>secret0: 发送目标的密钥的第一部分<br/>secret1: 发送目标的密钥的第二部分<br/>对于dingbot模式，token代表Webhook地址后面的token，secret0代表加签的密钥或AppSecret，secret1代表机器人的AppKey |
     | 功能 | send_message           | title, message                      | 发送消息（必填token、secret0）<br/>title:消息标题<br/>message:消息内容<br/>对于dingbot模式，发送的消息为MarkDown消息 |
     | 功能 | get_ding_image_mediaid | img                                 | 通过图片获得钉钉开放平台的mediaid（钉钉模式下生效，必填secret1、secret0），用于在markdown中插入图片信息<br/>img:要发送的文件路径<br/>注意，此功能仅对dingbot模式有用 |
     | 功能 | send_message_at        | title, message                      | 发送消息并且@<br/>title:消息标题<br/>message:消息内容<br/>对于dingbot模式，会在群中艾特所有人 |

### 五、开发进度

- [x] 通过钉钉机器人发送消息
- [x] 通过邮箱发送消息
- [x] 通过Server酱发送消息

### 六、各模式参数说明

`AlchemyFurnace(notice_way, token, secret0, secret1)` 四个参数在不同 `notice_way` 下含义不同:

| 参数       | dingbot                        | email                                | serverchan / sct                     |
| ---------- | ------------------------------ | ------------------------------------ | ------------------------------------ |
| token      | 钉钉机器人 access_token 后半段 | SMTP 服务器地址,格式 `host:port`     | Server酱 SendKey                     |
| secret0    | 加签密钥                       | 发件人邮箱密码/授权码                | 可选,推送渠道 `channel`(多个 `\|` 分隔) |
| secret1    | 机器人 AppKey(上传图片时需要)  | 发件人邮箱地址                       | 可选,微信接收者 `openid`             |

#### 邮箱模式示例

请先在对应邮箱后台开启 SMTP 并获取授权码(非登录密码),例如 QQ 邮箱为 `smtp.qq.com:465`。

```python
from AlchemyFurnace import AlchemyFurnace

af = AlchemyFurnace(
    notice_way="email",
    token="smtp.qq.com:465",
    secret0="你的邮箱授权码",
    secret1="sender@qq.com",
)
af.send_message("标题", "正文")                       # 发给自己(用 secret1 作为收件人)
af.send_message("标题", "正文", to="someone@xx.com")  # 发给指定收件人
```

#### Server酱模式示例

在 [Server酱](https://sct.ftqq.com/) 获取 SendKey 后使用:

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

### 七、相关项目

- [MutantCat-Working-Group/Echoes: 回声 (github.com)](https://github.com/MutantCat-Working-Group/Echoes)
