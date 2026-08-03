package af;

import af.AlchemyFurnace.Email;
import af.AlchemyFurnace.ServerChan;

/**
 * 炼丹炉示例 - 演示三种通知方式。
 *
 * 直接 mvn compile exec:java -Dexec.mainClass="af.Example" 运行;
 * 也可以打成 jar 后 java -cp target/alchemy-furnace-1.0.260803.jar:target/dependency/* af.Example
 *
 * 本示例从环境变量读取凭据,避免敏感信息硬编码。
 */
public class Example {
    public static void main(String[] args) {
        // 1) 钉钉机器人
        String dingToken = System.getenv("DINGBOT_TOKEN");
        String dingSecret = System.getenv("DINGBOT_SECRET");
        String dingAppKey = System.getenv("DINGBOT_APPKEY");
        if (dingToken != null) {
            AlchemyFurnace af = new AlchemyFurnace("dingbot", dingToken, dingSecret, dingAppKey);
            af.send_message("title", "message");
            af.send_message_at("title", "message");
            // af.send_message("title", "![image](" + af.get_ding_image_mediaid("image.jpg") + ")");
        }

        // 2) 邮箱
        String smtpServer = System.getenv("EMAIL_SMTP");
        String emailPwd = System.getenv("EMAIL_PASSWORD");
        String emailUser = System.getenv("EMAIL_USER");
        if (smtpServer != null && emailPwd != null && emailUser != null) {
            AlchemyFurnace emailAf = new AlchemyFurnace("email", smtpServer, emailPwd, emailUser);
            emailAf.send_message("标题", "正文");
            emailAf.send_message("标题", "正文", "someone@xx.com");

            Email email = new Email(smtpServer, emailPwd, emailUser);
            email.send("测试", "Hello from AlchemyFurnace", "to@xx.com");
        }

        // 3) Server酱
        String scKey = System.getenv("SERVERCHAN_KEY");
        if (scKey != null) {
            AlchemyFurnace scAf = new AlchemyFurnace("serverchan", scKey, "", "");
            scAf.send_message("标题", "正文,支持 **Markdown**");

            ServerChan sc = new ServerChan(scKey, "", "");
            sc.send("测试", "Hello from AlchemyFurnace");
        }
    }
}
