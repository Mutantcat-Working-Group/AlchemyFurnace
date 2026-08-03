<?php
/**
 * 炼丹炉 PHP 版示例
 *
 * 运行(需 PHP 7.4+ 且启用 curl/json 扩展):
 *   cd php
 *   composer install
 *   DINGBOT_TOKEN=xxx php Example.php
 */

require_once __DIR__ . '/AlchemyFurnace.php';

use AlchemyFurnace\AlchemyFurnace;
use AlchemyFurnace\DingBot;
use AlchemyFurnace\Email;
use AlchemyFurnace\ServerChan;

// 1) 钉钉机器人
$dt = getenv('DINGBOT_TOKEN') ?: '';
if ($dt) {
    $af = new AlchemyFurnace('dingbot', $dt, getenv('DINGBOT_SECRET') ?: '', getenv('DINGBOT_APPKEY') ?: '');
    $af->send_message('title', 'message');
    $af->send_message_at('title', 'message');
    // $af->send_message('title', '![image](' . $af->get_ding_image_mediaid('image.jpg') . ')');
}

// 2) 邮箱(需 composer install phpmailer/phpmailer)
$smtp = getenv('EMAIL_SMTP') ?: '';
$pwd  = getenv('EMAIL_PASSWORD') ?: '';
$usr  = getenv('EMAIL_USER') ?: '';
if ($smtp && $pwd && $usr) {
    $af = new AlchemyFurnace('email', $smtp, $pwd, $usr);
    $af->send_message('标题', '正文');
    $af->send_message('标题', '正文', 'to@xx.com');

    $email = new Email($smtp, $pwd, $usr);
    $email->send('测试', 'Hello from PHP AlchemyFurnace', 'to@xx.com');
}

// 3) Server酱
$sc = getenv('SERVERCHAN_KEY') ?: '';
if ($sc) {
    $af = new AlchemyFurnace('serverchan', $sc, '', '');
    $af->send_message('标题', '正文,支持 **Markdown**');

    $ch = new ServerChan($sc, '', '');
    $ch->send('测试', 'Hello from PHP AlchemyFurnace');
}
