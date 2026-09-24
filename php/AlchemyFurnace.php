<?php
/**
 * 炼丹炉 PHP 版
 * https://github.com/MutantCat-Working-Group/AlchemyFurnace
 * 由异猫工作群（mutantcat.org）发行
 *
 * 依赖(通过 Composer):
 *   composer require phpmailer/phpmailer
 *
 * 或直接下载 PHPMailer 源码放到项目里。
 * HTTP 使用 PHP 内置 curl 扩展,无需额外安装。
 *
 * 参数语义(按 notice_way 不同):
 *   notice_way : dingbot | email | serverchan/sct
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

namespace AlchemyFurnace;

/**
 * HTTP 工具
 */
class Http
{
    /**
     * @return array{status: int, body: string}
     */
    public static function request(string $url, string $method = 'GET', ?array $body = null, int $timeout = 15): array
    {
        $ch = curl_init();
        if (!$ch) {
            return ['status' => 0, 'body' => ''];
        }
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_TIMEOUT, $timeout);
        curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, $timeout);
        curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
        curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, true);
        curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, 2);

        if ($method === 'POST' && $body !== null) {
            curl_setopt($ch, CURLOPT_POST, true);
            curl_setopt($ch, CURLOPT_POSTFIELDS, $body['body']);
            if (!empty($body['content_type'])) {
                curl_setopt($ch, CURLOPT_HTTPHEADER, ['Content-Type: ' . $body['content_type']]);
            }
        }

        $out = curl_exec($ch);
        $status = (int) curl_getinfo($ch, CURLINFO_HTTP_CODE);
        if ($out === false) {
            error_log('Http::request 失败: ' . curl_error($ch));
            $out = '';
        }
        curl_close($ch);
        return ['status' => $status, 'body' => (string) $out];
    }
}

/**
 * 钉钉机器人
 */
class DingBot
{
    public function __construct(
        private string $appKey = '',
        private string $token = '',
        private string $secret = '',
    ) {}

    private function getDigest(): string
    {
        $ts = (string) (int) (microtime(true) * 1000);
        $stringToSign = $ts . "\n" . $this->secret;
        $hmac = hash_hmac('sha256', $stringToSign, $this->secret, true);
        $sign = urlencode(base64_encode($hmac));
        return '&timestamp=' . $ts . '&sign=' . $sign;
    }

    public function sendMarkdown(string $title, string $message, bool $atAll = false): void
    {
        $body = json_encode([
            'msgtype' => 'markdown',
            'markdown' => ['title' => $title, 'text' => $message],
            'at' => ['atMobiles' => [], 'isAtAll' => $atAll],
        ], JSON_UNESCAPED_UNICODE);

        $url = 'https://oapi.dingtalk.com/robot/send?access_token=' . $this->token . $this->getDigest();
        Http::request($url, 'POST', ['content_type' => 'application/json', 'body' => $body]);
    }

    public function getMediaid(string $img): string
    {
        $tokenUrl = 'https://oapi.dingtalk.com/gettoken?appkey=' . urlencode($this->appKey) . '&app_secret=' . urlencode($this->secret);
        $r = Http::request($tokenUrl);
        if ($r['status'] !== 200) {
            error_log("DingBot.get_mediaid 获取 token 失败 status={$r['status']}");
            return 'error';
        }
        $t = json_decode($r['body'], true);
        $access = $t['access_token'] ?? '';
        if ($access === '') {
            error_log('DingBot.get_mediaid 未获取到 access_token');
            return 'error';
        }

        $uploadUrl = 'https://oapi.dingtalk.com/media/upload?access_token=' . urlencode($access) . '&type=image';
        $r2 = Http::request($uploadUrl, 'POST', [
            'content_type' => 'multipart/form-data; name="media"; filename="' . basename($img) . '"; type=image',
            'body' => file_get_contents($img),
        ]);
        if ($r2['status'] !== 200) {
            error_log("DingBot.get_mediaid 上传失败 status={$r2['status']}");
            return 'error';
        }
        $u = json_decode($r2['body'], true);
        return $u['media_id'] ?? 'error';
    }
}

/**
 * 邮箱(基于 PHPMailer)
 */
class Email
{
    private string $host = '';
    private int $port = 0;
    private string $user = '';
    private string $password = '';

    public function __construct(string $smtpServer, string $password, string $user)
    {
        $this->user = $user;
        $this->password = $password;
        $idx = strrpos($smtpServer, ':');
        if ($idx !== false) {
            $this->host = substr($smtpServer, 0, $idx);
            $this->port = (int) substr($smtpServer, $idx + 1);
        } else {
            $this->host = $smtpServer;
        }
    }

    public function send(string $subject, string $content, ?string $to = null): bool
    {
        if ($this->host === '' || $this->user === '' || $this->password === '') {
            error_log('Email.send 失败: 未配置 SMTP/发件人/密码');
            return false;
        }
        $toAddr = $to ?: $this->user;

        // 兼容 Composer 自动加载与手动引入
        if (!class_exists(\PHPMailer\PHPMailer\PHPMailer::class)) {
            $candidates = [
                __DIR__ . '/vendor/autoload.php',
                dirname(__DIR__, 2) . '/vendor/autoload.php',
            ];
            foreach ($candidates as $c) {
                if (file_exists($c)) { require_once $c; break; }
            }
        }
        if (!class_exists(\PHPMailer\PHPMailer\PHPMailer::class)) {
            error_log('Email.send 失败: 请先 composer require phpmailer/phpmailer');
            return false;
        }

        $ports = $this->port > 0 ? [$this->port] : [465, 587, 25];
        foreach ($ports as $p) {
            if ($this->tryPort($p, $subject, $content, $toAddr)) {
                return true;
            }
        }
        error_log('Email.send 全部加密方式均失败');
        return false;
    }

    private function tryPort(int $port, string $subject, string $content, string $to): bool
    {
        $mail = new \PHPMailer\PHPMailer\PHPMailer(true);
        try {
            $mail->isSMTP();
            $mail->Host = $this->host;
            $mail->Port = $port;
            $mail->SMTPAuth = true;
            $mail->Username = $this->user;
            $mail->Password = $this->password;
            $mail->CharSet = 'UTF-8';

            if ($port === 465) {
                $mail->SMTPSecure = \PHPMailer\PHPMailer\PHPMailer::ENCRYPTION_SMTPS;
            } elseif ($port === 587) {
                $mail->SMTPSecure = \PHPMailer\PHPMailer\PHPMailer::ENCRYPTION_STARTTLS;
            }

            $mail->setFrom($this->user);
            $mail->addAddress($to);
            $mail->Subject = $subject;
            $mail->Body = $content;
            $mail->send();
            return true;
        } catch (\Throwable $e) {
            return false;
        }
    }
}

/**
 * Server酱(Turbo)
 */
class ServerChan
{
    public function __construct(
        private string $sendkey = '',
        private string $channel = '',
        private string $openid = '',
    ) {}

    public function send(string $title, string $desp = ''): bool
    {
        if ($this->sendkey === '') { error_log('ServerChan.send 失败: 未配置 SendKey'); return false; }
        if ($title === '' && $desp === '') { error_log('ServerChan.send 失败: title 与 desp 不能同时为空'); return false; }

        $body = http_build_query(array_filter([
            'title' => $title,
            'desp' => $desp,
            'channel' => $this->channel,
            'openid' => $this->openid,
        ], fn($v) => $v !== '' && $v !== null));

        $url = 'https://sctapi.ftqq.com/' . $this->sendkey . '.send';
        $r = Http::request($url, 'POST', ['content_type' => 'application/x-www-form-urlencoded', 'body' => $body]);
        if ($r['status'] !== 200) { error_log("ServerChan.send 请求失败 status={$r['status']}"); return false; }
        $j = json_decode($r['body'], true);
        if (($j['code'] ?? -1) === 0) return true;
        error_log("ServerChan.send 返回失败: code=" . ($j['code'] ?? 'null') . " message=" . ($j['message'] ?? ''));
        return false;
    }
}

/**
 * 统一入口
 */
class AlchemyFurnace
{
    public function __construct(
        private string $noticeWay = '',
        private string $token = '',
        private string $secret0 = '',
        private string $secret1 = '',
    ) {}

    public function send_message(string $title, string $message, ?string $to = null): void
    {
        switch ($this->noticeWay) {
            case 'dingbot':
                (new DingBot($this->secret1, $this->token, $this->secret0))->sendMarkdown($title, $message, false);
                break;
            case 'email':
                (new Email($this->token, $this->secret0, $this->secret1))->send($title, $message, $to);
                break;
            case 'serverchan':
            case 'sct':
                (new ServerChan($this->token, $this->secret0, $this->secret1))->send($title, $message);
                break;
            default:
                error_log('AlchemyFurnace: Unknown notice_way');
        }
    }

    public function send_message_at(string $title, string $message): void
    {
        if ($this->noticeWay === 'dingbot') {
            (new DingBot($this->secret1, $this->token, $this->secret0))->sendMarkdown($title, $message, true);
        } else {
            error_log('AlchemyFurnace: send_message_at 当前仅支持 dingbot 模式');
        }
    }

    public function get_ding_image_mediaid(string $img): string
    {
        return (new DingBot($this->secret1, '', $this->secret0))->getMediaid($img);
    }
}
