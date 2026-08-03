/**
 * 炼丹炉 TypeScript 版示例
 *
 * 运行(需 Node 18+):
 *   cd typescript
 *   npm install
 *   DINGBOT_TOKEN=xxx npm run example
 * 或直接:
 *   npx tsx Example.ts
 */
import { AlchemyFurnace, DingBot, Email, ServerChan } from './AlchemyFurnace';

const env = (k: string) => process.env[k] || '';

async function main() {
  // 1) 钉钉机器人
  const dt = env('DINGBOT_TOKEN');
  if (dt) {
    const af = new AlchemyFurnace('dingbot', dt, env('DINGBOT_SECRET'), env('DINGBOT_APPKEY'));
    await af.sendMessage('title', 'message');
    await af.sendMessageAt('title', 'message');
    // await af.sendMessage('title', `![image](${await af.getDingImageMediaid('image.jpg')})`);
  }

  // 2) 邮箱(需要 npm install nodemailer)
  const smtp = env('EMAIL_SMTP');
  const pwd = env('EMAIL_PASSWORD');
  const usr = env('EMAIL_USER');
  if (smtp && pwd && usr) {
    const af = new AlchemyFurnace('email', smtp, pwd, usr);
    await af.sendMessage('标题', '正文');
    await af.sendMessage('标题', '正文', 'to@xx.com');

    const email = new Email(smtp, pwd, usr);
    await email.send('测试', 'Hello from TS AlchemyFurnace', 'to@xx.com');
  }

  // 3) Server酱
  const sc = env('SERVERCHAN_KEY');
  if (sc) {
    const af = new AlchemyFurnace('serverchan', sc, '', '');
    await af.sendMessage('标题', '正文,支持 **Markdown**');

    const ch = new ServerChan(sc, '', '');
    await ch.send('测试', 'Hello from TS AlchemyFurnace');
  }
}

main().catch((e) => console.error(e));
