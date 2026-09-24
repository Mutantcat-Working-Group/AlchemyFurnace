// 炼丹炉 - 多通知方式统一入口(Go 版本)
// https://github.com/MutantCat-Working-Group/AlchemyFurnace
// 由异猫工作群（mutantcat.org）发行
//
// 仅使用 Go 标准库,无第三方依赖。
//
// 参数语义(按 NoticeWay 不同):
//
//	NoticeWay : dingbot | email | serverchan/sct
//	Token      : dingbot    → 钉钉机器人 access_token 后半段
//	             email      → SMTP 服务器地址,格式 host:port(如 smtp.qq.com:465)
//	             serverchan → Server酱 SendKey
//	Secret0    : dingbot    → 加签密钥
//	             email      → 发件人邮箱密码/授权码
//	             serverchan → 可选,推送渠道 channel(多个 | 分隔)
//	Secret1    : dingbot    → 机器人 AppKey(上传图片时需要)
//	             email      → 发件人邮箱地址
//	             serverchan → 可选,微信接收者 openid
package alchemyfurnace

import (
	"bytes"
	"crypto/hmac"
	"crypto/sha256"
	"crypto/tls"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"mime/multipart"
	"net"
	"net/http"
	"net/smtp"
	"net/url"
	"os"
	"strings"
	"time"
)

// AlchemyFurnace 统一入口
type AlchemyFurnace struct {
	NoticeWay string
	Token     string
	Secret0   string
	Secret1   string
}

// NewAlchemyFurnace 构造
func NewAlchemyFurnace(noticeWay, token, secret0, secret1 string) *AlchemyFurnace {
	return &AlchemyFurnace{NoticeWay: noticeWay, Token: token, Secret0: secret0, Secret1: secret1}
}

// SendMessage 发送消息(to 仅 email 模式生效)
func (a *AlchemyFurnace) SendMessage(title, message, to string) {
	switch a.NoticeWay {
	case "dingbot":
		NewDingBot(a.Token, a.Secret0, a.Secret1).SendMarkdown(title, message, false)
	case "email":
		NewEmail(a.Token, a.Secret0, a.Secret1).Send(title, message, to)
	case "serverchan", "sct":
		NewServerChan(a.Token, a.Secret0, a.Secret1).Send(title, message)
	default:
		fmt.Println("AlchemyFurnace: Unknown notice_way")
	}
}

// SendMessageAt 发送并@(仅 dingbot 生效)
func (a *AlchemyFurnace) SendMessageAt(title, message string) {
	if a.NoticeWay == "dingbot" {
		NewDingBot(a.Token, a.Secret0, a.Secret1).SendMarkdown(title, message, true)
	} else {
		fmt.Println("AlchemyFurnace: SendMessageAt 当前仅支持 dingbot 模式")
	}
}

// GetDingImageMediaID 上传图片获取 media_id(仅 dingbot 生效)
func (a *AlchemyFurnace) GetDingImageMediaID(img string) string {
	return NewDingBot("", a.Secret0, a.Secret1).GetMediaID(img)
}

// ============================================================
// 钉钉机器人
// ============================================================

type DingBot struct {
	AppKey string
	Token  string
	Secret string
}

func NewDingBot(appKey, token, secret string) *DingBot {
	return &DingBot{AppKey: appKey, Token: token, Secret: secret}
}

func (d *DingBot) getDigest() string {
	timestamp := fmt.Sprintf("%d", time.Now().UnixNano()/1e6)
	stringToSign := timestamp + "\n" + d.Secret
	mac := hmac.New(sha256.New, []byte(d.Secret))
	mac.Write([]byte(stringToSign))
	sign := base64.StdEncoding.EncodeToString(mac.Sum(nil))
	return "&timestamp=" + timestamp + "&sign=" + url.QueryEscape(sign)
}

// SendMarkdown 发送 markdown 消息
func (d *DingBot) SendMarkdown(title, message string, atAll bool) {
	body := map[string]interface{}{
		"msgtype": "markdown",
		"markdown": map[string]string{
			"title": title,
			"text":  message,
		},
		"at": map[string]interface{}{
			"atMobiles": []string{},
			"isAtAll":   atAll,
		},
	}
	raw, _ := json.Marshal(body)
	url := "https://oapi.dingtalk.com/robot/send?access_token=" + d.Token + d.getDigest()
	resp, err := httpPostJSON(url, raw)
	if err != nil {
		fmt.Printf("DingBot.SendMarkdown 失败: %v\n", err)
		return
	}
	_ = resp
}

// GetMediaID 上传图片获取 media_id
func (d *DingBot) GetMediaID(img string) string {
	tokenURL := "https://oapi.dingtalk.com/gettoken?appkey=" + d.AppKey + "&app_secret=" + d.Secret
	resp, err := http.Get(tokenURL)
	if err != nil {
		fmt.Printf("DingBot.GetMediaID 获取 token 失败: %v\n", err)
		return "error"
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		fmt.Printf("DingBot.GetMediaID 获取 token 失败, status=%d\n", resp.StatusCode)
		return "error"
	}
	var tokenData struct {
		AccessToken string `json:"access_token"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&tokenData); err != nil || tokenData.AccessToken == "" {
		fmt.Println("DingBot.GetMediaID 未获取到 access_token")
		return "error"
	}

	uploadURL := "https://oapi.dingtalk.com/media/upload?access_token=" + tokenData.AccessToken + "&type=image"
	resp2, err := httpPostFile(uploadURL, "media", img)
	if err != nil {
		fmt.Printf("DingBot.GetMediaID 上传失败: %v\n", err)
		return "error"
	}
	defer resp2.Body.Close()
	var uploadData struct {
		MediaID string `json:"media_id"`
	}
	if err := json.NewDecoder(resp2.Body).Decode(&uploadData); err != nil {
		return "error"
	}
	return uploadData.MediaID
}

// ============================================================
// 邮箱
// ============================================================

type Email struct {
	Host     string
	Port     int
	User     string
	Password string
}

func NewEmail(smtpServer, password, user string) *Email {
	host, port := splitHostPort(smtpServer)
	return &Email{Host: host, Port: port, User: user, Password: password}
}

func (e *Email) Send(subject, content, to string) bool {
	if e.Host == "" || e.User == "" || e.Password == "" {
		fmt.Println("Email.Send 失败: 未配置 SMTP/发件人/密码")
		return false
	}
	toAddr := to
	if toAddr == "" {
		toAddr = e.User
	}
	ports := []int{}
	if e.Port > 0 {
		ports = []int{e.Port}
	} else {
		ports = []int{465, 587, 25}
	}
	for _, p := range ports {
		if err := tryPort(e, p, subject, content, toAddr); err == nil {
			return true
		}
	}
	fmt.Println("Email.Send 全部加密方式均失败")
	return false
}

func tryPort(e *Email, port int, subject, content, to string) error {
	host := e.Host
	if strings.Contains(host, ":") && !strings.HasPrefix(host, "[") {
		host = "[" + host + "]"
	}
	addr := net.JoinHostPort(host, fmt.Sprintf("%d", port))
	msg := buildMessage(e.User, to, subject, content)

	if port == 465 {
		// SSL 直连
		conn, err := tls.Dial("tcp", addr, &tls.Config{ServerName: e.Host})
		if err != nil {
			return err
		}
		defer conn.Close()
		client, err := smtp.NewClient(conn, e.Host)
		if err != nil {
			return err
		}
		defer client.Close()
		return sendWithClient(client, e.Host, e.User, e.Password, to, msg)
	}

	// STARTTLS(587) 或明文(25)
	conn, err := net.DialTimeout("tcp", addr, 15*time.Second)
	if err != nil {
		return err
	}
	client, err := smtp.NewClient(conn, e.Host)
	if err != nil {
		conn.Close()
		return err
	}
	defer client.Close()
	if port == 587 {
		if err := client.StartTLS(&tls.Config{ServerName: e.Host}); err != nil {
			return err
		}
	}
	return sendWithClient(client, e.Host, e.User, e.Password, to, msg)
}

func sendWithClient(client *smtp.Client, host, user, password, to string, msg []byte) error {
	if err := client.Auth(smtp.PlainAuth("", user, password, host)); err != nil {
		return err
	}
	if err := client.Mail(user); err != nil {
		return err
	}
	if err := client.Rcpt(to); err != nil {
		return err
	}
	wc, err := client.Data()
	if err != nil {
		return err
	}
	defer wc.Close()
	_, err = wc.Write(msg)
	if err != nil {
		return err
	}
	return client.Quit()
}

func buildMessage(from, to, subject, body string) []byte {
	var b strings.Builder
	b.WriteString("From: " + from + "\r\n")
	b.WriteString("To: " + to + "\r\n")
	b.WriteString("Subject: =?UTF-8?B?" + base64.StdEncoding.EncodeToString([]byte(subject)) + "?=\r\n")
	b.WriteString("Content-Type: text/plain; charset=UTF-8\r\n")
	b.WriteString("Content-Transfer-Encoding: base64\r\n\r\n")
	b.WriteString(base64.StdEncoding.EncodeToString([]byte(body)))
	return []byte(b.String())
}

// ============================================================
// Server酱(Turbo)
// ============================================================

type ServerChan struct {
	SendKey string
	Channel string
	OpenID  string
}

func NewServerChan(sendkey, channel, openid string) *ServerChan {
	return &ServerChan{SendKey: sendkey, Channel: channel, OpenID: openid}
}

func (s *ServerChan) Send(title, desp string) bool {
	if s.SendKey == "" {
		fmt.Println("ServerChan.Send 失败: 未配置 SendKey")
		return false
	}
	if title == "" && desp == "" {
		fmt.Println("ServerChan.Send 失败: title 与 desp 不能同时为空")
		return false
	}
	form := url.Values{}
	form.Set("title", title)
	form.Set("desp", desp)
	if s.Channel != "" {
		form.Set("channel", s.Channel)
	}
	if s.OpenID != "" {
		form.Set("openid", s.OpenID)
	}
	url := fmt.Sprintf("https://sctapi.ftqq.com/%s.send", s.SendKey)
	resp, err := http.PostForm(url, form)
	if err != nil {
		fmt.Printf("ServerChan.Send 请求失败: %v\n", err)
		return false
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		fmt.Printf("ServerChan.Send 请求失败, status=%d\n", resp.StatusCode)
		return false
	}
	var result struct {
		Code    int    `json:"code"`
		Message string `json:"message"`
		Info    string `json:"info"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		fmt.Printf("ServerChan.Send 响应解析失败: %v\n", err)
		return false
	}
	if result.Code == 0 {
		return true
	}
	fmt.Printf("ServerChan.Send 返回失败: code=%d message=%s info=%s\n", result.Code, result.Message, result.Info)
	return false
}

// ============================================================
// 内部工具
// ============================================================

func httpPostJSON(url string, body []byte) (*http.Response, error) {
	req, err := http.NewRequest("POST", url, bytes.NewReader(body))
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", "application/json; charset=UTF-8")
	client := &http.Client{Timeout: 15 * time.Second}
	return client.Do(req)
}

func httpPostFile(url, field, filePath string) (*http.Response, error) {
	var buf bytes.Buffer
	w := multipart.NewWriter(&buf)
	fw, err := w.CreateFormFile(field, filePath)
	if err != nil {
		return nil, err
	}
	f, err := os.Open(filePath)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	if _, err = io.Copy(fw, f); err != nil {
		return nil, err
	}
	w.Close()

	req, err := http.NewRequest("POST", url, &buf)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Content-Type", w.FormDataContentType())
	client := &http.Client{Timeout: 15 * time.Second}
	return client.Do(req)
}

func splitHostPort(s string) (string, int) {
	if i := strings.LastIndex(s, ":"); i >= 0 {
		host := s[:i]
		port := 0
		fmt.Sscanf(s[i+1:], "%d", &port)
		return host, port
	}
	return s, 0
}
