import json
import hmac
import hashlib
import base64
import urllib.parse
import smtplib
from email.mime.text import MIMEText
from email.header import Header
from time import time
import requests


# 炼丹炉
# https://github.com/MutantCat-Working-Group/AlchemyFurnace
# 由异猫工作群（mutantcat.org）发行
#
# 参数语义(按 notice_way 不同,同名字段含义不同):
#   notice_way : 通知方式 (dingbot | email | serverchan/sct)
#   token      : dingbot → 钉钉机器人 access_token 后半段
#                email   → SMTP 服务器地址,格式 host:port(如 smtp.qq.com:465)
#                serverchan → SendKey(形如 SCTxxxx)
#   secret0    : dingbot → 加签密钥
#                email   → 发件人邮箱密码/授权码
#                serverchan → 可选,推送渠道 channel(多个用 | 分隔)
#   secret1    : dingbot → 机器人 AppKey(上传图片时需要)
#                email   → 发件人邮箱地址
#                serverchan → 可选,微信接收者 openid
class AlchemyFurnace:
    def __init__(self, notice_way="", token="", secret0="", secret1=""):
        self.notice_way = notice_way
        self.token = token
        self.secret0 = secret0
        self.secret1 = secret1

    def send_message(self, title, message, to=None):
        if self.notice_way == "dingbot":
            self.__dingbot(title, message)
        elif self.notice_way == "email":
            Email(smtp_server=self.token, password=self.secret0, user=self.secret1).send(
                subject=title, content=message, to=to
            )
        elif self.notice_way in ("serverchan", "sct"):
            ServerChan(sendkey=self.token, channel=self.secret0, openid=self.secret1).send(
                title=title, desp=message
            )
        else:
            print("AlchemyFurnace: Unknown notice_way")

    def send_message_at(self, title, message):
        if self.notice_way == "dingbot":
            self.__dingbot_at_all(title, message)
        else:
            print("AlchemyFurnace: send_message_at 当前仅支持 dingbot 模式")

    def get_ding_image_mediaid(self, img):
        return DingBot(app_key=self.secret1, secret=self.secret0).get_mediaid(img)

    def __dingbot(self, title, message):
        DingBot(token=self.token, secret=self.secret0, app_key=self.secret1).send_markdown(title, message)

    def __dingbot_at_all(self, title, message):
        DingBot(token=self.token, secret=self.secret0, app_key=self.secret1).send_markdown_at_all(title, message)


# 钉钉机器人通知方式实现类
# https://github.com/MutantCat-Working-Group/AlchemyFurnace
class DingBot:
    def __init__(self, app_key="", token="", secret=""):
        self.app_key = app_key
        self.token = token
        self.secret = secret

    def get_digest(self):
        timestamp = str(round(time() * 1000))
        secret_enc = self.secret.encode('utf-8')
        string_to_sign = '{}\n{}'.format(timestamp, self.secret)
        string_to_sign_enc = string_to_sign.encode('utf-8')
        hmac_code = hmac.new(secret_enc, string_to_sign_enc, digestmod=hashlib.sha256).digest()
        sign = urllib.parse.quote_plus(base64.b64encode(hmac_code))
        return f"&timestamp={timestamp}&sign={sign}"

    def send_markdown(self, title, message):
        data = {
            "msgtype": "markdown",
            "markdown": {"title": title, "text": message},
            "at": {"atMobiles": [], "isAtAll": False},
        }
        webhook_url = 'https://oapi.dingtalk.com/robot/send?access_token=' + self.token
        try:
            req = requests.post(webhook_url + self.get_digest(), json=data)
            return req
        except requests.RequestException as e:
            print(f"DingBot.send_markdown 请求失败: {e}")
            return None

    def send_markdown_at_all(self, title, message):
        data = {
            "msgtype": "markdown",
            "markdown": {"title": title, "text": message},
            "at": {"atMobiles": [], "isAtAll": True},
        }
        webhook_url = 'https://oapi.dingtalk.com/robot/send?access_token=' + self.token
        try:
            req = requests.post(webhook_url + self.get_digest(), json=data)
            return req
        except requests.RequestException as e:
            print(f"DingBot.send_markdown_at_all 请求失败: {e}")
            return None

    # 上传本地文件并获得图片 media_id(必须填写 app_key 和 secret)
    def get_mediaid(self, img):
        try:
            token_url = (
                'https://oapi.dingtalk.com/gettoken?appkey='
                + self.app_key
                + '&app_secret='
                + self.secret
            )
            token_resp = requests.get(token_url)
            if token_resp.status_code != 200:
                print(f"DingBot.get_mediaid 获取 token 失败, status={token_resp.status_code}")
                return 'error'
            access_token = token_resp.json().get('access_token')
            if not access_token:
                print("DingBot.get_mediaid 返回中未包含 access_token")
                return 'error'

            url = (
                'https://oapi.dingtalk.com/media/upload?access_token='
                + access_token
                + '&type=image'
            )
            with open(img, 'rb') as f:
                files = {'media': f}
                data = {'access_token': access_token, 'type': 'image'}
                response = requests.post(url, files=files, data=data)
            if response.status_code == 200:
                return response.json().get('media_id', 'error')
            else:
                print(f"DingBot.get_mediaid 上传失败, status={response.status_code}")
                return 'error'
        except requests.RequestException as e:
            print(f"DingBot.get_mediaid 请求失败: {e}")
            return 'error'


# 邮箱通知方式实现类
# 仅依赖 Python 标准库(smtplib / email),无第三方依赖
class Email:
    def __init__(self, smtp_server="", user="", password=""):
        # smtp_server 支持 "host:port" 或 "host";端口缺失时按加密方式使用默认值
        if ':' in smtp_server:
            host, port_str = smtp_server.rsplit(':', 1)
            self.host = host
            try:
                self.port = int(port_str)
            except ValueError:
                self.host = smtp_server
                self.port = 0
        else:
            self.host = smtp_server
            self.port = 0
        self.user = user
        self.password = password

    def send(self, subject, content, to=None) -> bool:
        if not self.host:
            print("Email.send 失败: 未配置 SMTP 服务器地址")
            return False
        if not self.user or not self.password:
            print("Email.send 失败: 未配置发件人地址或密码/授权码")
            return False

        to_addr = to or self.user
        msg = MIMEText(content, 'plain', 'utf-8')
        msg['From'] = Header(self.user)
        msg['To'] = Header(to_addr)
        msg['Subject'] = Header(subject, 'utf-8')

        # 决定端口:已指定按指定,未指定按加密方式给默认值
        port = self.port
        if port == 0:
            # 先尝试 465(SSL),失败再试 587(STARTTLS)
            return self._try_ssl(subject, content, msg, to_addr, host_port=465) or \
                   self._try_starttls(subject, content, msg, to_addr, host_port=587)
        else:
            return self._try_ssl(subject, content, msg, to_addr, host_port=port) or \
                   self._try_starttls(subject, content, msg, to_addr, host_port=port) or \
                   self._try_plain(subject, content, msg, to_addr, host_port=port)

    def _send_with(self, build_server, host_port, msg, to_addr):
        server = build_server()
        try:
            server.login(self.user, self.password)
            server.sendmail(self.user, [to_addr], msg.as_string())
            return True
        finally:
            try:
                server.quit()
            except Exception:
                pass

    def _try_ssl(self, subject, content, msg, to_addr, host_port):
        try:
            self._send_with(
                lambda: smtplib.SMTP_SSL(self.host, host_port, timeout=15),
                host_port, msg, to_addr,
            )
            return True
        except Exception:
            return False

    def _try_starttls(self, subject, content, msg, to_addr, host_port):
        try:
            def build():
                s = smtplib.SMTP(self.host, host_port, timeout=15)
                s.starttls()
                return s
            self._send_with(build, host_port, msg, to_addr)
            return True
        except Exception:
            return False

    def _try_plain(self, subject, content, msg, to_addr, host_port):
        try:
            self._send_with(
                lambda: smtplib.SMTP(self.host, host_port, timeout=15),
                host_port, msg, to_addr,
            )
            return True
        except Exception as e:
            print(f"Email.send 全部加密方式均失败: {e}")
            return False


# Server酱(Turbo 版)通知方式实现类
# 官方文档:https://sct.ftqq.com/
# 旧版 sc.ftqq.com v1 已停用,这里默认走 Turbo 接口 sctapi.ftqq.com
class ServerChan:
    def __init__(self, sendkey="", channel="", openid=""):
        self.sendkey = sendkey
        self.channel = channel
        self.openid = openid

    def send(self, title, desp="") -> bool:
        if not self.sendkey:
            print("ServerChan.send 失败: 未配置 SendKey")
            return False
        if not (title or desp):
            print("ServerChan.send 失败: title 与 desp 不能同时为空")
            return False

        url = f'https://sctapi.ftqq.com/{self.sendkey}.send'
        payload = {"title": title, "desp": desp}
        if self.channel:
            payload["channel"] = self.channel
        if self.openid:
            payload["openid"] = self.openid

        try:
            resp = requests.post(url, data=payload, timeout=15)
            if resp.status_code != 200:
                print(f"ServerChan.send 请求失败, status={resp.status_code}")
                return False
            result = resp.json()
            if result.get("code") == 0:
                return True
            print(
                f"ServerChan.send 返回失败: code={result.get('code')}, "
                f"message={result.get('message')}, info={result.get('info')}"
            )
            return False
        except requests.RequestException as e:
            print(f"ServerChan.send 请求失败: {e}")
            return False
        except ValueError as e:
            print(f"ServerChan.send 响应解析失败: {e}")
            return False
