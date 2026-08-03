use std::env;
use alchemy_furnace::{AlchemyFurnace, Email, ServerChan};

fn main() {
    // 1) 钉钉
    if let Ok(dt) = env::var("DINGBOT_TOKEN") {
        let af = AlchemyFurnace::new("dingbot", &dt, env::var("DINGBOT_SECRET").unwrap_or_default(), env::var("DINGBOT_APPKEY").unwrap_or_default());
        af.send_message("title", "message", None);
        af.send_message_at("title", "message");
        // af.send_message("title", &format!("![image]({})", af.get_ding_image_mediaid("image.jpg")), None);
    }

    // 2) 邮箱
    if let (Ok(smtp), Ok(pwd), Ok(usr)) = (env::var("EMAIL_SMTP"), env::var("EMAIL_PASSWORD"), env::var("EMAIL_USER")) {
        let af = AlchemyFurnace::new("email", &smtp, &pwd, &usr);
        af.send_message("标题", "正文", None);
        af.send_message("标题", "正文", Some("to@xx.com"));

        let email = Email::new(&smtp, &pwd, &usr);
        email.send("测试", "Hello from Rust AlchemyFurnace", Some("to@xx.com"));
    }

    // 3) Server酱
    if let Ok(sc) = env::var("SERVERCHAN_KEY") {
        let af = AlchemyFurnace::new("serverchan", &sc, "", "");
        af.send_message("标题", "正文,支持 **Markdown**", None);

        let sc = ServerChan::new(&sc, "", "");
        sc.send("测试", "Hello from Rust AlchemyFurnace");
    }
}
