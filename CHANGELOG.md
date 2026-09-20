### 1.0.20260920
  - 多语言实现版本统一为 1.0.20260920

### 1.0.20260803
  - 新增邮箱通知方式(Email 类),仅依赖 Python 标准库,支持 SSL/STARTTLS 自动探测
  - 新增 Server酱(Turbo 版)通知方式(ServerChan 类),使用 sctapi.ftqq.com 接口
  - 新增 TypeScript / PHP / C# 三种语言实现,API 语义与其他版本一致
  - 修复 DingBot.get_mediaid 中 gettoken 接口参数错误(appsecret → app_secret)
  - 为所有网络请求增加异常捕获与失败提示,避免宿主程序因网络抖动崩溃
  - 示例文件改为从环境变量读取凭据,避免敏感信息泄露
  - 版本号统一为 1.0.20260803(原 CHANGELOG 1.0.20240410 与 README 1.0.240410 不一致)

### 1.0.20240410
  - Python 版支持通过钉钉发送消息
