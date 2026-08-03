using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using MailKit.Net.Smtp;
using MimeKit;

namespace AlchemyFurnace
{
    /// <summary>
    /// 炼丹炉 - 多通知方式统一入口
    /// https://github.com/MutantCat-Working-Group/AlchemyFurnace
    ///
    /// 参数语义(按 noticeWay 不同):
    ///   noticeWay : dingbot | email | serverchan/sct
    ///   token      : dingbot    → 钉钉机器人 access_token 后半段
    ///                email      → SMTP 服务器地址,格式 host:port(如 smtp.qq.com:465)
    ///                serverchan → Server酱 SendKey
    ///   secret0    : dingbot    → 加签密钥
    ///                email      → 发件人邮箱密码/授权码
    ///                serverchan → 可选,推送渠道 channel(多个 | 分隔)
    ///   secret1    : dingbot    → 机器人 AppKey(上传图片时需要)
    ///                email      → 发件人邮箱地址
    ///                serverchan → 可选,微信接收者 openid
    /// </summary>
    public class AlchemyFurnace
    {
        private static readonly HttpClient Http = new HttpClient { Timeout = TimeSpan.FromSeconds(15) };

        public string NoticeWay { get; set; } = "";
        public string Token { get; set; } = "";
        public string Secret0 { get; set; } = "";
        public string Secret1 { get; set; } = "";

        public AlchemyFurnace() { }

        public AlchemyFurnace(string noticeWay, string token, string secret0, string secret1)
        {
            NoticeWay = noticeWay;
            Token = token;
            Secret0 = secret0;
            Secret1 = secret1;
        }

        public void SendMessage(string title, string message, string to = null)
        {
            switch (NoticeWay)
            {
                case "dingbot":
                    new DingBot(Secret1, Token, Secret0).SendMarkdown(title, message, false);
                    break;
                case "email":
                    new Email(Token, Secret0, Secret1).Send(title, message, to);
                    break;
                case "serverchan":
                case "sct":
                    new ServerChan(Token, Secret0, Secret1).Send(title, message);
                    break;
                default:
                    Console.WriteLine("AlchemyFurnace: Unknown notice_way");
                    break;
            }
        }

        public void SendMessageAt(string title, string message)
        {
            if (NoticeWay == "dingbot")
                new DingBot(Secret1, Token, Secret0).SendMarkdown(title, message, true);
            else
                Console.WriteLine("AlchemyFurnace: SendMessageAt 当前仅支持 dingbot 模式");
        }

        public string GetDingImageMediaid(string img)
        {
            return new DingBot(Secret1, "", Secret0).GetMediaid(img);
        }
    }

    public class DingBot
    {
        private readonly string _appKey;
        private readonly string _token;
        private readonly string _secret;

        public DingBot(string appKey = "", string token = "", string secret = "")
        {
            _appKey = appKey;
            _token = token;
            _secret = secret;
        }

        private string GetDigest()
        {
            var ts = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds().ToString();
            var stringToSign = ts + "\n" + _secret;
            using var hmac = new HMACSHA256(Encoding.UTF8.GetBytes(_secret));
            var hash = hmac.ComputeHash(Encoding.UTF8.GetBytes(stringToSign));
            var sign = Uri.EscapeDataString(Convert.ToBase64String(hash));
            return "&timestamp=" + ts + "&sign=" + sign;
        }

        public void SendMarkdown(string title, string message, bool atAll = false)
        {
            var body = System.Text.Json.JsonSerializer.Serialize(new
            {
                msgtype = "markdown",
                markdown = new { title, text = message },
                at = new { atMobiles = new object[] { }, isAtAll = atAll }
            });
            var url = "https://oapi.dingtalk.com/robot/send?access_token=" + _token + GetDigest();
            _ = PostAsync(url, "application/json", body);
        }

        public string GetMediaid(string img)
        {
            var tokenUrl = "https://oapi.dingtalk.com/gettoken?appkey=" + _appKey + "&app_secret=" + _secret;
            var r = AlchemyFurnace.Http.GetStringAsync(tokenUrl).GetAwaiter().GetResult();
            string access = "";
            try { access = System.Text.Json.JsonDocument.Parse(r).RootElement.GetProperty("access_token").GetString(); }
            catch { }
            if (string.IsNullOrEmpty(access))
            {
                Console.Error.WriteLine("DingBot.get_mediaid 未获取到 access_token");
                return "error";
            }

            var uploadUrl = "https://oapi.dingtalk.com/media/upload?access_token=" + access + "&type=image";
            var content = new MultipartFormDataContent();
            var bytes = File.ReadAllBytes(img);
            content.Add(new ByteArrayContent(bytes), "media", Path.GetFileName(img));
            var resp = AlchemyFurnace.Http.PostAsync(uploadUrl, content).GetAwaiter().GetResult();
            var body = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            if (!resp.IsSuccessStatusCode)
            {
                Console.Error.WriteLine($"DingBot.get_mediaid 上传失败 status={(int)resp.StatusCode}");
                return "error";
            }
            try
            {
                return System.Text.Json.JsonDocument.Parse(body).RootElement.GetProperty("media_id").GetString() ?? "error";
            }
            catch { return "error"; }
        }

        private static async Task<HttpResponseMessage> PostAsync(string url, string contentType, string body)
        {
            try
            {
                var c = new StringContent(body, Encoding.UTF8, contentType);
                return await AlchemyFurnace.Http.PostAsync(url, c);
            }
            catch (Exception e)
            {
                Console.Error.WriteLine($"http POST 失败: {e.Message}");
                return null;
            }
        }
    }

    public class Email
    {
        private readonly string _host;
        private readonly int _port;
        private readonly string _user;
        private readonly string _password;

        public Email(string smtpServer = "", string password = "", string user = "")
        {
            _user = user;
            _password = password;
            var idx = smtpServer.LastIndexOf(':');
            if (idx > 0)
            {
                _host = smtpServer.Substring(0, idx);
                int.TryParse(smtpServer.Substring(idx + 1), out _port);
            }
            else
            {
                _host = smtpServer;
                _port = 0;
            }
        }

        public bool Send(string subject, string content, string to = null)
        {
            if (string.IsNullOrEmpty(_host) || string.IsNullOrEmpty(_user) || string.IsNullOrEmpty(_password))
            {
                Console.Error.WriteLine("Email.Send 失败: 未配置 SMTP/发件人/密码");
                return false;
            }
            var toAddr = to ?? _user;
            var ports = _port > 0 ? new[] { _port } : new[] { 465, 587, 25 };
            foreach (var p in ports)
            {
                if (TryPort(p, subject, content, toAddr)) return true;
            }
            Console.Error.WriteLine("Email.Send 全部加密方式均失败");
            return false;
        }

        private bool TryPort(int port, string subject, string content, string to)
        {
            using var msg = new MimeMessage();
            msg.From.Add(new MailboxAddress("", _user));
            msg.To.Add(new MailboxAddress("", to));
            msg.Subject = subject;
            msg.Body = new TextPart("plain") { Text = content };

            try
            {
                using var client = new SmtpClient();
                var ssl = port == 465;
                client.Connect(_host, port, ssl);
                client.Authenticate(_user, _password);
                client.Send(msg);
                client.Disconnect(true);
                return true;
            }
            catch
            {
                return false;
            }
        }
    }

    public class ServerChan
    {
        private readonly string _sendkey;
        private readonly string _channel;
        private readonly string _openid;

        public ServerChan(string sendkey = "", string channel = "", string openid = "")
        {
            _sendkey = sendkey;
            _channel = channel;
            _openid = openid;
        }

        public bool Send(string title, string desp = "")
        {
            if (string.IsNullOrEmpty(_sendkey)) { Console.Error.WriteLine("ServerChan.Send 失败: 未配置 SendKey"); return false; }
            if (string.IsNullOrEmpty(title) && string.IsNullOrEmpty(desp)) { Console.Error.WriteLine("ServerChan.Send 失败: title 与 desp 不能同时为空"); return false; }

            var form = new System.Collections.Generic.List<KeyValuePair<string, string>>
            {
                new KeyValuePair<string, string>("title", title),
                new KeyValuePair<string, string>("desp", desp),
            };
            if (!string.IsNullOrEmpty(_channel)) form.Add(new KeyValuePair<string, string>("channel", _channel));
            if (!string.IsNullOrEmpty(_openid)) form.Add(new KeyValuePair<string, string>("openid", _openid));

            var url = $"https://sctapi.ftqq.com/{_sendkey}.send";
            try
            {
                var content = new FormUrlEncodedContent(form);
                var resp = AlchemyFurnace.Http.PostAsync(url, content).GetAwaiter().GetResult();
                var body = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();
                if (!resp.IsSuccessStatusCode) { Console.Error.WriteLine($"ServerChan.Send 请求失败 status={(int)resp.StatusCode}"); return false; }
                var j = System.Text.Json.JsonDocument.Parse(body);
                if (j.RootElement.GetProperty("code").GetInt32() == 0) return true;
                Console.Error.WriteLine("ServerChan.Send 返回失败: " + body);
                return false;
            }
            catch (Exception e)
            {
                Console.Error.WriteLine($"ServerChan.Send 失败: {e.Message}");
                return false;
            }
        }
    }
}
