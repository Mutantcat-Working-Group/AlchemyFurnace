use base64::Engine;
use hmac::{Hmac, Mac};
use serde::Deserialize;
use sha2::Sha256;
use std::time::{SystemTime, UNIX_EPOCH};

type HmacSha256 = Hmac<Sha256>;

// ============================================================
// 钉钉机器人
// ============================================================
pub struct DingBot {
    pub app_key: String,
    pub token: String,
    pub secret: String,
}

impl DingBot {
    pub fn new(app_key: impl Into<String>, token: impl Into<String>, secret: impl Into<String>) -> Self {
        Self { app_key: app_key.into(), token: token.into(), secret: secret.into() }
    }

    fn timestamp_ms() -> u64 {
        SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_millis() as u64
    }

    fn get_digest(&self) -> String {
        let ts = Self::timestamp_ms().to_string();
        let string_to_sign = format!("{}\n{}", ts, self.secret);
        let mut mac = HmacSha256::new_from_slice(self.secret.as_bytes()).expect("HMAC can take key");
        mac.update(string_to_sign.as_bytes());
        let sign = base64::engine::general_purpose::STANDARD.encode(mac.finalize().into_bytes());
        let sign = urlencoding::encode(&sign);
        format!("&timestamp={}&sign={}", ts, sign)
    }

    pub fn send_markdown(&self, title: &str, message: &str, at_all: bool) {
        let body = serde_json::json!({
            "msgtype": "markdown",
            "markdown": { "title": title, "text": message },
            "at": { "atMobiles": [], "isAtAll": at_all }
        });
        let url = format!(
            "https://oapi.dingtalk.com/robot/send?access_token={}{}",
            self.token,
            self.get_digest()
        );
        let client = reqwest::blocking::Client::builder().timeout(std::time::Duration::from_secs(15)).build().ok();
        if let Some(c) = client {
            if let Err(e) = c.post(&url).json(&body).send() {
                eprintln!("DingBot.send_markdown 失败: {}", e);
            }
        }
    }

    pub fn get_mediaid(&self, img: &str) -> String {
        let token_url = format!(
            "https://oapi.dingtalk.com/gettoken?appkey={}&app_secret={}",
            self.app_key, self.secret
        );
        let client = match reqwest::blocking::Client::builder().timeout(std::time::Duration::from_secs(15)).build() {
            Ok(c) => c,
            Err(e) => { eprintln!("DingBot.get_mediaid 创建客户端失败: {}", e); return "error".into(); }
        };
        let resp = match client.get(&token_url).send() {
            Ok(r) => r,
            Err(e) => { eprintln!("DingBot.get_mediaid 获取 token 失败: {}", e); return "error".into(); }
        };
        if !resp.status().is_success() {
            eprintln!("DingBot.get_mediaid 获取 token 失败, status={}", resp.status());
            return "error".into();
        }
        let json: serde_json::Value = match resp.json() {
            Ok(v) => v,
            Err(_) => { eprintln!("DingBot.get_mediaid 解析失败"); return "error".into(); }
        };
        let access = json["access_token"].as_str().unwrap_or("");
        if access.is_empty() { eprintln!("DingBot.get_mediaid 未获取到 access_token"); return "error".into(); }

        let upload_url = format!(
            "https://oapi.dingtalk.com/media/upload?access_token={}&type=image",
            access
        );
        let file_bytes = match std::fs::read(img) {
            Ok(v) => v,
            Err(e) => { eprintln!("DingBot.get_mediaid 读取文件失败: {}", e); return "error".into(); }
        };
        let part = reqwest::blocking::multipart::Part::bytes(file_bytes).file_name(img.to_string());
        let form = reqwest::blocking::multipart::Form::new().part("media", part);
        let resp2 = match client.post(&upload_url).multipart(form).send() {
            Ok(r) => r,
            Err(e) => { eprintln!("DingBot.get_mediaid 上传失败: {}", e); return "error".into(); }
        };
        if !resp2.status().is_success() {
            eprintln!("DingBot.get_mediaid 上传失败, status={}", resp2.status());
            return "error".into();
        }
        let json2: serde_json::Value = match resp2.json() {
            Ok(v) => v,
            Err(_) => { eprintln!("DingBot.get_mediaid 解析 media_id 失败"); return "error".into(); }
        };
        json2["media_id"].as_str().unwrap_or("error").to_string()
    }
}

// ============================================================
// 邮箱(lettre)
// ============================================================
pub struct Email {
    pub host: String,
    pub port: u16,
    pub user: String,
    pub password: String,
}

impl Email {
    pub fn new(smtp_server: &str, password: impl Into<String>, user: impl Into<String>) -> Self {
        let user = user.into();
        let password = password.into();
        let (host, port) = if let Some(idx) = smtp_server.rfind(':') {
            let (h, p) = smtp_server.split_at(idx);
            let p = p.trim_start_matches(':');
            (h.to_string(), p.parse::<u16>().unwrap_or(0))
        } else {
            (smtp_server.to_string(), 0)
        };
        Self { host, port, user, password }
    }

    pub fn send(&self, subject: &str, content: &str, to: Option<&str>) -> bool {
        if self.host.is_empty() || self.user.is_empty() || self.password.is_empty() {
            eprintln!("Email.send 失败: 未配置 SMTP/发件人/密码");
            return false;
        }
        let to_addr = to.unwrap_or(&self.user);
        let ports: Vec<u16> = if self.port > 0 { vec![self.port] } else { vec![465, 587, 25] };
        for p in ports {
            if self.try_port(p, subject, content, to_addr) { return true; }
        }
        eprintln!("Email.send 全部加密方式均失败");
        false
    }

    fn try_port(&self, port: u16, subject: &str, content: &str, to: &str) -> bool {
        use lettre::message::{Message, MultiPart};
        use lettre::transport::smtp::client::{Tls, TlsParameters};
        use lettre::{SmtpTransport, Transport};

        let msg = match Message::builder()
            .from(self.user.parse().unwrap_or_else(|_| "af@local".parse().unwrap()))
            .to(to.parse().unwrap_or_else(|_| "af@local".parse().unwrap()))
            .subject(subject)
            .multipart(MultiPart::alternative_plain_html(
                content.to_string(),
                format!("<pre>{}</pre>", content),
            )) {
            Ok(m) => m,
            Err(e) => { eprintln!("Email 构造消息失败: {}", e); return false; }
        };

        let tls = match TlsParameters::new(self.host.clone()) {
            Ok(t) => t,
            Err(e) => { eprintln!("Email TLS 参数失败: {}", e); return false; }
        };

        let result = if port == 465 {
            let mailer = SmtpTransport::relay(&self.host)
                .and_then(|b| Ok(b.port(port).tls(Tls::Wrapper(tls)).credentials(self.creds()).build()));
            mailer.and_then(|m| m.send(&msg).map_err(|e| e.into()))
        } else if port == 587 {
            let mailer = SmtpTransport::starttls_relay(&self.host)
                .and_then(|b| Ok(b.port(port).tls(Tls::Required(tls)).credentials(self.creds()).build()));
            mailer.and_then(|m| m.send(&msg).map_err(|e| e.into()))
        } else {
            let mailer = SmtpTransport::builder_dangerous(&self.host).port(port).credentials(self.creds()).build();
            mailer.send(&msg)
        };
        match result {
            Ok(_) => true,
            Err(e) => { /* 静默失败,尝试下一端口 */ let _ = e; false }
        }
    }

    fn creds(&self) -> lettre::transport::smtp::authentication::Credentials {
        lettre::transport::smtp::authentication::Credentials::new(self.user.clone(), self.password.clone())
    }
}

// ============================================================
// Server酱(Turbo)
// ============================================================
pub struct ServerChan {
    pub sendkey: String,
    pub channel: String,
    pub openid: String,
}

#[derive(Deserialize)]
struct ScResponse {
    code: i32,
    message: String,
}

impl ServerChan {
    pub fn new(sendkey: impl Into<String>, channel: impl Into<String>, openid: impl Into<String>) -> Self {
        Self { sendkey: sendkey.into(), channel: channel.into(), openid: openid.into() }
    }

    pub fn send(&self, title: &str, desp: &str) -> bool {
        if self.sendkey.is_empty() { eprintln!("ServerChan.send 失败: 未配置 SendKey"); return false; }
        if title.is_empty() && desp.is_empty() { eprintln!("ServerChan.send 失败: title 与 desp 不能同时为空"); return false; }
        let url = format!("https://sctapi.ftqq.com/{}.send", self.sendkey);
        let client = reqwest::blocking::Client::builder().timeout(std::time::Duration::from_secs(15)).build();
        let client = match client {
            Ok(c) => c,
            Err(e) => { eprintln!("ServerChan 创建客户端失败: {}", e); return false; }
        };
        let mut form = vec![("title", title.to_string()), ("desp", desp.to_string())];
        if !self.channel.is_empty() { form.push(("channel", self.channel.clone())); }
        if !self.openid.is_empty() { form.push(("openid", self.openid.clone())); }
        let resp = match client.post(&url).form(&form).send() {
            Ok(r) => r,
            Err(e) => { eprintln!("ServerChan.send 请求失败: {}", e); return false; }
        };
        if !resp.status().is_success() {
            eprintln!("ServerChan.send 请求失败, status={}", resp.status());
            return false;
        }
        let r: ScResponse = match resp.json() {
            Ok(v) => v,
            Err(e) => { eprintln!("ServerChan.send 解析失败: {}", e); return false; }
        };
        if r.code == 0 { true } else { eprintln!("ServerChan.send 返回失败: code={} message={}", r.code, r.message); false }
    }
}

// ============================================================
// 统一入口
// ============================================================
pub struct AlchemyFurnace {
    pub notice_way: String,
    pub token: String,
    pub secret0: String,
    pub secret1: String,
}

impl AlchemyFurnace {
    pub fn new(notice_way: impl Into<String>, token: impl Into<String>, secret0: impl Into<String>, secret1: impl Into<String>) -> Self {
        Self {
            notice_way: notice_way.into(),
            token: token.into(),
            secret0: secret0.into(),
            secret1: secret1.into(),
        }
    }

    pub fn send_message(&self, title: &str, message: &str, to: Option<&str>) {
        match self.notice_way.as_str() {
            "dingbot" => DingBot::new(&self.token, &self.secret0, &self.secret1).send_markdown(title, message, false),
            "email" => { Email::new(&self.token, &self.secret0, &self.secret1).send(title, message, to); }
            "serverchan" | "sct" => { ServerChan::new(&self.token, &self.secret0, &self.secret1).send(title, message); }
            _ => eprintln!("AlchemyFurnace: Unknown notice_way"),
        }
    }

    pub fn send_message_at(&self, title: &str, message: &str) {
        if self.notice_way == "dingbot" {
            DingBot::new(&self.token, &self.secret0, &self.secret1).send_markdown(title, message, true);
        } else {
            eprintln!("AlchemyFurnace: send_message_at 当前仅支持 dingbot 模式");
        }
    }

    pub fn get_ding_image_mediaid(&self, img: &str) -> String {
        DingBot::new(&self.secret1, "", &self.secret0).get_mediaid(img)
    }
}
