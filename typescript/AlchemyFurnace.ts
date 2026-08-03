/**
 * 炼丹炉 TypeScript / JavaScript 版
 * https://github.com/MutantCat-Working-Group/AlchemyFurnace
 *
 * 依赖(可选,推荐):
 *   npm install nodemailer          # 邮箱发送
 *   npm install --save-dev @types/nodemailer
 * HTTP 使用 Node 18+ 全局 fetch,无需安装。
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
 */

import * as crypto from 'crypto';
import * as fs from 'fs';
import * as path from 'path';

// ============================================================
// HTTP 工具
// ============================================================
async function httpRequest(
  url: string,
  method: string = 'GET',
  body?: { contentType: string; body: string },
  timeoutMs: number = 15000,
): Promise<{ status: number; body: string }> {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeoutMs);
  try {
    const init: RequestInit = { method, signal: ctrl.signal };
    if (body) {
      init.headers = { 'Content-Type': body.contentType };
      init.body = body.body;
    }
    const r = await fetch(url, init);
    const text = await r.text();
    return { status: r.status, body: text };
  } catch (e: any) {
    console.error(`httpRequest 失败: ${e?.message ?? e}`);
    return { status: 0, body: '' };
  } finally {
    clearTimeout(timer);
  }
}

// ============================================================
// 钉钉机器人
// ============================================================
export class DingBot {
  constructor(
    private appKey: string = '',
    private token: string = '',
    private secret: string = '',
  ) {}

  private getDigest(): string {
    const ts = Date.now().toString();
    const stringToSign = `${ts}\n${this.secret}`;
    const hmac = crypto.createHmac('sha256', this.secret).update(stringToSign).digest('base64');
    const sign = encodeURIComponent(hmac);
    return `&timestamp=${ts}&sign=${sign}`;
  }

  async sendMarkdown(title: string, message: string, atAll: boolean = false): Promise<void> {
    const body = {
      msgtype: 'markdown',
      markdown: { title, text: message },
      at: { atMobiles: [], isAtAll: atAll },
    };
    const url = `https://oapi.dingtalk.com/robot/send?access_token=${this.token}${this.getDigest()}`;
    await httpRequest(url, 'POST', { contentType: 'application/json', body: JSON.stringify(body) });
  }

  async getMediaid(img: string): Promise<string> {
    const tokenUrl = `https://oapi.dingtalk.com/gettoken?appkey=${this.appKey}&app_secret=${this.secret}`;
    const r = await httpRequest(tokenUrl);
    if (r.status !== 200) { console.error(`DingBot.get_mediaid 获取 token 失败 status=${r.status}`); return 'error'; }
    let token: string | undefined;
    try { token = JSON.parse(r.body).access_token; } catch {}
    if (!token) { console.error('DingBot.get_mediaid 未获取到 access_token'); return 'error'; }

    const uploadUrl = `https://oapi.dingtalk.com/media/upload?access_token=${token}&type=image`;
    const file = fs.readFileSync(img);
    const boundary = '----AfBoundary' + Date.now();
    const disp = `--${boundary}\r\nContent-Disposition: form-data; name="media"; filename="${path.basename(img)}"\r\nContent-Type: application/octet-stream\r\n\r\n`;
    const end = `\r\n--${boundary}--\r\n`;
    const payload = Buffer.concat([Buffer.from(disp), file, Buffer.from(end)]);
    const r2 = await httpRequest(uploadUrl, 'POST', {
      contentType: `multipart/form-data; boundary=${boundary}`,
      body: payload.toString('latin1'),
    });
    if (r2.status !== 200) { console.error(`DingBot.get_mediaid 上传失败 status=${r2.status}`); return 'error'; }
    try { return JSON.parse(r2.body).media_id ?? 'error'; } catch { return 'error'; }
  }
}

// ============================================================
// 邮箱(基于 nodemailer)
// ============================================================
export class Email {
  private host: string;
  private port: number;
  private user: string;
  private password: string;

  constructor(smtpServer: string, password: string, user: string) {
    this.user = user;
    this.password = password;
    const idx = smtpServer.lastIndexOf(':');
    if (idx > 0) {
      this.host = smtpServer.slice(0, idx);
      const p = parseInt(smtpServer.slice(idx + 1), 10);
      this.port = Number.isFinite(p) ? p : 0;
    } else {
      this.host = smtpServer;
      this.port = 0;
    }
  }

  async send(subject: string, content: string, to?: string): Promise<boolean> {
    if (!this.host || !this.user || !this.password) {
      console.error('Email.send 失败: 未配置 SMTP/发件人/密码');
      return false;
    }
    const toAddr = to || this.user;

    // 动态导入 nodemailer,避免未安装时直接报错
    let nodemailer: any;
    try {
      nodemailer = await import('nodemailer');
    } catch {
      console.error('Email.send 失败: 请先 npm install nodemailer');
      return false;
    }

    const ports = this.port > 0 ? [this.port] : [465, 587, 25];
    for (const p of ports) {
      try {
        const transporter = nodemailer.createTransport({
          host: this.host,
          port: p,
          secure: p === 465,
          requireTLS: p === 587,
          auth: { user: this.user, pass: this.password },
        });
        await transporter.sendMail({ from: this.user, to: toAddr, subject, text: content });
        return true;
      } catch { /* 继续尝试下一个端口 */ }
    }
    console.error('Email.send 全部加密方式均失败');
    return false;
  }
}

// ============================================================
// Server酱(Turbo)
// ============================================================
export class ServerChan {
  constructor(
    private sendkey: string = '',
    private channel: string = '',
    private openid: string = '',
  ) {}

  async send(title: string, desp: string = ''): Promise<boolean> {
    if (!this.sendkey) { console.error('ServerChan.send 失败: 未配置 SendKey'); return false; }
    if (!title && !desp) { console.error('ServerChan.send 失败: title 与 desp 不能同时为空'); return false; }
    const form = new URLSearchParams();
    form.set('title', title);
    form.set('desp', desp);
    if (this.channel) form.set('channel', this.channel);
    if (this.openid) form.set('openid', this.openid);

    const url = `https://sctapi.ftqq.com/${this.sendkey}.send`;
    const r = await httpRequest(url, 'POST', {
      contentType: 'application/x-www-form-urlencoded',
      body: form.toString(),
    });
    if (r.status !== 200) { console.error(`ServerChan.send 请求失败 status=${r.status}`); return false; }
    try {
      const j = JSON.parse(r.body);
      if (j.code === 0) return true;
      console.error(`ServerChan.send 返回失败: code=${j.code} message=${j.message}`);
      return false;
    } catch (e: any) {
      console.error(`ServerChan.send 解析失败: ${e?.message ?? e}`);
      return false;
    }
  }
}

// ============================================================
// 统一入口
// ============================================================
export class AlchemyFurnace {
  constructor(
    private noticeWay: string = '',
    private token: string = '',
    private secret0: string = '',
    private secret1: string = '',
  ) {}

  async sendMessage(title: string, message: string, to?: string): Promise<void> {
    switch (this.noticeWay) {
      case 'dingbot':
        await new DingBot(this.secret1, this.token, this.secret0).sendMarkdown(title, message, false);
        break;
      case 'email':
        await new Email(this.token, this.secret0, this.secret1).send(title, message, to);
        break;
      case 'serverchan':
      case 'sct':
        await new ServerChan(this.token, this.secret0, this.secret1).send(title, message);
        break;
      default:
        console.error('AlchemyFurnace: Unknown notice_way');
    }
  }

  async sendMessageAt(title: string, message: string): Promise<void> {
    if (this.noticeWay === 'dingbot') {
      await new DingBot(this.secret1, this.token, this.secret0).sendMarkdown(title, message, true);
    } else {
      console.error('AlchemyFurnace: sendMessageAt 当前仅支持 dingbot 模式');
    }
  }

  async getDingImageMediaid(img: string): Promise<string> {
    return new DingBot(this.secret1, '', this.secret0).getMediaid(img);
  }
}

export default AlchemyFurnace;
