package af;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.DataOutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Base64;
import java.util.Properties;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import javax.mail.Authenticator;
import javax.mail.PasswordAuthentication;
import javax.mail.Session;
import javax.mail.Transport;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.MimeMessage;

import org.json.JSONObject;

/**
 * 炼丹炉 - 多通知方式统一入口
 * https://github.com/MutantCat-Working-Group/AlchemyFurnace
 *
 * 参数语义(按 noticeWay 不同):
 *   noticeWay : dingbot | email | serverchan/sct
 *   token      : dingbot    → 钉钉机器人 access_token 后半段
 *                email      → SMTP 服务器地址,格式 host:port(如 smtp.qq.com:465)
 *                serverchan → Server酱 SendKey
 *   secret0    : dingbot    → 加签密钥
 *                email      → 发件人邮箱密码/授权码
 *                serverchan → 可选,推送渠道 channel(多个 | 分隔)
 *   secret1    : dingbot    → 机器人 AppKey(上传图片时需要)
 *                email      → 发件人邮箱地址
 *                serverchan → 可选,微信接收者 openid
 *
 * 依赖: Java 8+ 标准库 + org.json(JSON 解析,单 jar 约 500KB,可随项目一起引入)
 */
public class AlchemyFurnace {
    private String noticeWay;
    private String token;
    private String secret0;
    private String secret1;

    public AlchemyFurnace() { this("", "", "", ""); }

    public AlchemyFurnace(String noticeWay, String token, String secret0, String secret1) {
        this.noticeWay = noticeWay == null ? "" : noticeWay;
        this.token = token == null ? "" : token;
        this.secret0 = secret0 == null ? "" : secret0;
        this.secret1 = secret1 == null ? "" : secret1;
    }

    public void send_message(String title, String message) { send_message(title, message, null); }

    public void send_message(String title, String message, String to) {
        switch (noticeWay) {
            case "dingbot":
                dingbot(title, message, false);
                break;
            case "email":
                new Email(token, secret0, secret1).send(title, message, to);
                break;
            case "serverchan":
            case "sct":
                new ServerChan(token, secret0, secret1).send(title, message);
                break;
            default:
                System.out.println("AlchemyFurnace: Unknown notice_way");
        }
    }

    public void send_message_at(String title, String message) {
        if ("dingbot".equals(noticeWay)) {
            dingbot(title, message, true);
        } else {
            System.out.println("AlchemyFurnace: send_message_at 当前仅支持 dingbot 模式");
        }
    }

    public String get_ding_image_mediaid(String img) {
        return new DingBot("", secret0, secret1).get_mediaid(img);
    }

    private void dingbot(String title, String message, boolean atAll) {
        new DingBot(token, secret0, secret1).send_markdown(title, message, atAll);
    }

    // ---------- setter 链式调用(可选) ----------
    public AlchemyFurnace noticeWay(String v) { this.noticeWay = v; return this; }
    public AlchemyFurnace token(String v)      { this.token = v; return this; }
    public AlchemyFurnace secret0(String v)    { this.secret0 = v; return this; }
    public AlchemyFurnace secret1(String v)    { this.secret1 = v; return this; }

    // ============================================================
    // 钉钉机器人
    // ============================================================
    public static class DingBot {
        private String appKey;
        private String token;
        private String secret;

        public DingBot() { this("", "", ""); }
        public DingBot(String appKey, String token, String secret) {
            this.appKey = appKey == null ? "" : appKey;
            this.token = token == null ? "" : token;
            this.secret = secret == null ? "" : secret;
        }

        private String getDigest() {
            try {
                long timestamp = System.currentTimeMillis();
                String stringToSign = timestamp + "\n" + secret;
                Mac mac = Mac.getInstance("HmacSHA256");
                mac.init(new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
                byte[] hmacCode = mac.doFinal(stringToSign.getBytes(StandardCharsets.UTF_8));
                String sign = URLEncoder.encode(Base64.getEncoder().encodeToString(hmacCode), "UTF-8");
                return "&timestamp=" + timestamp + "&sign=" + sign;
            } catch (Exception e) {
                System.err.println("DingBot.getDigest 失败: " + e.getMessage());
                return "";
            }
        }

        public void send_markdown(String title, String message) { send_markdown(title, message, false); }

        public void send_markdown(String title, String message, boolean atAll) {
            JSONObject body = new JSONObject();
            body.put("msgtype", "markdown");
            JSONObject markdown = new JSONObject();
            markdown.put("title", title);
            markdown.put("text", message);
            body.put("markdown", markdown);
            JSONObject at = new JSONObject();
            at.put("atMobiles", new java.util.ArrayList<String>());
            at.put("isAtAll", atAll);
            body.put("at", at);
            String url = "https://oapi.dingtalk.com/robot/send?access_token=" + token;
            httpPostJson(url + getDigest(), body.toString());
        }

        public String get_mediaid(String img) {
            try {
                String tokenUrl = "https://oapi.dingtalk.com/gettoken?appkey=" + appKey + "&app_secret=" + secret;
                String tokenResp = httpGet(tokenUrl);
                JSONObject tokenJson = new JSONObject(tokenResp);
                String accessToken = tokenJson.optString("access_token", null);
                if (accessToken == null) {
                    System.err.println("DingBot.get_mediaid 未获取到 access_token");
                    return "error";
                }
                String uploadUrl = "https://oapi.dingtalk.com/media/upload?access_token=" + accessToken + "&type=image";
                String resp = httpPostMultipart(uploadUrl, img);
                JSONObject json = new JSONObject(resp);
                return json.optString("media_id", "error");
            } catch (Exception e) {
                System.err.println("DingBot.get_mediaid 失败: " + e.getMessage());
                return "error";
            }
        }
    }

    // ============================================================
    // 邮箱
    // ============================================================
    public static class Email {
        private String host;
        private int port;
        private String user;
        private String password;

        public Email() { this("", "", ""); }
        public Email(String smtpServer, String password, String user) {
            this.user = user == null ? "" : user;
            this.password = password == null ? "" : password;
            if (smtpServer != null && smtpServer.contains(":")) {
                int idx = smtpServer.lastIndexOf(':');
                this.host = smtpServer.substring(0, idx);
                try { this.port = Integer.parseInt(smtpServer.substring(idx + 1)); }
                catch (NumberFormatException e) { this.port = 0; }
            } else {
                this.host = smtpServer == null ? "" : smtpServer;
                this.port = 0;
            }
        }

        public boolean send(String subject, String content) { return send(subject, content, null); }

        public boolean send(String subject, String content, String to) {
            if (host.isEmpty() || user.isEmpty() || password.isEmpty()) {
                System.err.println("Email.send 失败: 未配置 SMTP/发件人/密码");
                return false;
            }
            String toAddr = (to == null || to.isEmpty()) ? user : to;
            int[] ports = port > 0 ? new int[]{ port } : new int[]{ 465, 587, 25 };
            for (int p : ports) {
                try {
                    tryPort(p, subject, content, toAddr);
                    return true;
                } catch (Exception e) {
                    // 继续尝试下一个端口/加密方式
                }
            }
            System.err.println("Email.send 全部加密方式均失败");
            return false;
        }

        private void tryPort(int port, String subject, String content, String to) throws Exception {
            Properties props = new Properties();
            props.put("mail.smtp.host", host);
            props.put("mail.smtp.port", String.valueOf(port));
            props.put("mail.smtp.auth", "true");
            boolean ssl = (port == 465);
            boolean starttls = (port == 587);
            if (ssl) {
                props.put("mail.smtp.ssl.enable", "true");
            } else if (starttls) {
                props.put("mail.smtp.starttls.enable", "true");
            }
            Session session = Session.getInstance(props, new Authenticator() {
                @Override protected PasswordAuthentication getPasswordAuthentication() {
                    return new PasswordAuthentication(user, password);
                }
            });
            MimeMessage msg = new MimeMessage(session);
            msg.setFrom(new InternetAddress(user));
            msg.setRecipient(javax.mail.Message.RecipientType.TO, new InternetAddress(to));
            msg.setSubject(subject, "UTF-8");
            msg.setText(content, "UTF-8");
            Transport.send(msg);
        }
    }

    // ============================================================
    // Server酱(Turbo)
    // ============================================================
    public static class ServerChan {
        private String sendkey;
        private String channel;
        private String openid;

        public ServerChan() { this("", "", ""); }
        public ServerChan(String sendkey, String channel, String openid) {
            this.sendkey = sendkey == null ? "" : sendkey;
            this.channel = channel == null ? "" : channel;
            this.openid = openid == null ? "" : openid;
        }

        public boolean send(String title, String desp) {
            if (sendkey.isEmpty()) {
                System.err.println("ServerChan.send 失败: 未配置 SendKey");
                return false;
            }
            if ((title == null || title.isEmpty()) && (desp == null || desp.isEmpty())) {
                System.err.println("ServerChan.send 失败: title 与 desp 不能同时为空");
                return false;
            }
            StringBuilder body = new StringBuilder();
            append(body, "title", title == null ? "" : title);
            append(body, "desp", desp == null ? "" : desp);
            append(body, "channel", channel);
            append(body, "openid", openid);
            String url = "https://sctapi.ftqq.com/" + sendkey + ".send";
            try {
                String resp = httpPostForm(url, body.toString());
                JSONObject json = new JSONObject(resp);
                if (json.optInt("code", -1) == 0) return true;
                System.err.println("ServerChan.send 返回失败: " + json.toString());
                return false;
            } catch (Exception e) {
                System.err.println("ServerChan.send 失败: " + e.getMessage());
                return false;
            }
        }

        private void append(StringBuilder sb, String k, String v) {
            if (v == null || v.isEmpty()) return;
            if (sb.length() > 0) sb.append('&');
            sb.append(k).append('=').append(urlEncode(v));
        }
    }

    // ============================================================
    // HTTP 工具(内部)
    // ============================================================
    static String httpGet(String urlStr) throws Exception {
        URL url = new URL(urlStr);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod("GET");
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(15000);
        return readResponse(conn);
    }

    static String httpPostJson(String urlStr, String json) {
        try {
            URL url = new URL(urlStr);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("POST");
            conn.setConnectTimeout(15000);
            conn.setReadTimeout(15000);
            conn.setDoOutput(true);
            conn.setRequestProperty("Content-Type", "application/json; charset=UTF-8");
            try (OutputStream os = conn.getOutputStream()) {
                os.write(json.getBytes(StandardCharsets.UTF_8));
            }
            return readResponse(conn);
        } catch (Exception e) {
            System.err.println("httpPostJson 失败: " + e.getMessage());
            return "";
        }
    }

    static String httpPostForm(String urlStr, String formBody) throws Exception {
        URL url = new URL(urlStr);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod("POST");
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(15000);
        conn.setDoOutput(true);
        conn.setRequestProperty("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
        try (OutputStream os = conn.getOutputStream()) {
            os.write(formBody.getBytes(StandardCharsets.UTF_8));
        }
        return readResponse(conn);
    }

    static String httpPostMultipart(String urlStr, String filePath) throws Exception {
        String boundary = "----AfBoundary" + System.currentTimeMillis();
        URL url = new URL(urlStr);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod("POST");
        conn.setConnectTimeout(15000);
        conn.setReadTimeout(15000);
        conn.setDoOutput(true);
        conn.setRequestProperty("Content-Type", "multipart/form-data; boundary=" + boundary);

        try (DataOutputStream dos = new DataOutputStream(conn.getOutputStream())) {
            dos.writeBytes("--" + boundary + "\r\n");
            dos.writeBytes("Content-Disposition: form-data; name=\"media\"; filename=\"" + new File(filePath).getName() + "\"\r\n");
            dos.writeBytes("Content-Type: application/octet-stream\r\n\r\n");
            try (FileInputStream fis = new FileInputStream(filePath)) {
                byte[] buf = new byte[8192];
                int n;
                while ((n = fis.read(buf)) > 0) dos.write(buf, 0, n);
            }
            dos.writeBytes("\r\n--" + boundary + "--\r\n");
            dos.flush();
        }
        return readResponse(conn);
    }

    private static String readResponse(HttpURLConnection conn) throws Exception {
        int code = conn.getResponseCode();
        InputStream is = (code >= 200 && code < 300) ? conn.getInputStream() : conn.getErrorStream();
        if (is == null) return "";
        try (BufferedReader br = new BufferedReader(new InputStreamReader(is, StandardCharsets.UTF_8))) {
            StringBuilder sb = new StringBuilder();
            String line;
            while ((line = br.readLine()) != null) sb.append(line);
            return sb.toString();
        } finally {
            conn.disconnect();
        }
    }

    private static String urlEncode(String s) {
        try { return URLEncoder.encode(s, "UTF-8"); }
        catch (Exception e) { return s; }
    }
}
