using System;
using AlchemyFurnace;

class Program
{
    static string Env(string k) => Environment.GetEnvironmentVariable(k) ?? "";

    static void Main()
    {
        // 1) 钉钉机器人
        var dt = Env("DINGBOT_TOKEN");
        if (dt != "")
        {
            var af = new AlchemyFurnace.AlchemyFurnace("dingbot", dt, Env("DINGBOT_SECRET"), Env("DINGBOT_APPKEY"));
            af.SendMessage("title", "message");
            af.SendMessageAt("title", "message");
            // af.SendMessage("title", $"![image]({af.GetDingImageMediaid("image.jpg")})");
        }

        // 2) 邮箱
        var smtp = Env("EMAIL_SMTP");
        var pwd = Env("EMAIL_PASSWORD");
        var usr = Env("EMAIL_USER");
        if (smtp != "" && pwd != "" && usr != "")
        {
            var af = new AlchemyFurnace.AlchemyFurnace("email", smtp, pwd, usr);
            af.SendMessage("标题", "正文");
            af.SendMessage("标题", "正文", "to@xx.com");

            var email = new Email(smtp, pwd, usr);
            email.Send("测试", "Hello from C# AlchemyFurnace", "to@xx.com");
        }

        // 3) Server酱
        var sc = Env("SERVERCHAN_KEY");
        if (sc != "")
        {
            var af = new AlchemyFurnace.AlchemyFurnace("serverchan", sc, "", "");
            af.SendMessage("标题", "正文,支持 **Markdown**");

            var ch = new ServerChan(sc, "", "");
            ch.Send("测试", "Hello from C# AlchemyFurnace");
        }
    }
}
