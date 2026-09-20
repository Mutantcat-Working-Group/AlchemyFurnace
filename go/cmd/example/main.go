// 炼丹炉 Go 版示例
// 运行: cd go && go run ./cmd/example
package main

import (
	"os"

	af "com.mutantcat/alchemyfurnace"
)

func main() {
	// 1) 钉钉机器人
	if dt := os.Getenv("DINGBOT_TOKEN"); dt != "" {
		a := af.NewAlchemyFurnace("dingbot", dt, os.Getenv("DINGBOT_SECRET"), os.Getenv("DINGBOT_APPKEY"))
		a.SendMessage("title", "message", "")
		a.SendMessageAt("title", "message")
		// a.SendMessage("title", "![image]("+a.GetDingImageMediaID("image.jpg")+")", "")
	}

	// 2) 邮箱
	smtp, pwd, usr := os.Getenv("EMAIL_SMTP"), os.Getenv("EMAIL_PASSWORD"), os.Getenv("EMAIL_USER")
	if smtp != "" && pwd != "" && usr != "" {
		a := af.NewAlchemyFurnace("email", smtp, pwd, usr)
		a.SendMessage("标题", "正文", "")
		a.SendMessage("标题", "正文", "to@xx.com")

		email := af.NewEmail(smtp, pwd, usr)
		email.Send("测试", "Hello from Go AlchemyFurnace", "to@xx.com")
	}

	// 3) Server酱
	if sc := os.Getenv("SERVERCHAN_KEY"); sc != "" {
		a := af.NewAlchemyFurnace("serverchan", sc, "", "")
		a.SendMessage("标题", "正文,支持 **Markdown**", "")

		ch := af.NewServerChan(sc, "", "")
		ch.Send("测试", "Hello from Go AlchemyFurnace")
	}
}
