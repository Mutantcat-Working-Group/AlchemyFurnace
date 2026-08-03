"""
钉钉机器人示例(炼丹炉 Python 版)

使用前请设置环境变量:
  export DINGBOT_TOKEN=xxxx   # 钉钉机器人 access_token 后半段
  export DINGBOT_SECRET=xxxx  # 加签密钥
  export DINGBOT_APPKEY=xxxx  # 机器人 AppKey(上传图片时需要)

也可在 python/ 目录下创建 .env 文件(已加入 .gitignore,不会被提交),例如:
  DINGBOT_TOKEN=xxxx
  DINGBOT_SECRET=xxxx
  DINGBOT_APPKEY=xxxx
"""
import os

from AlchemyFurnace import AlchemyFurnace

# 创建炼丹炉实例
alchemy_furnace = AlchemyFurnace(
    notice_way="dingbot",
    token=os.getenv("DINGBOT_TOKEN", ""),
    secret0=os.getenv("DINGBOT_SECRET", ""),
    secret1=os.getenv("DINGBOT_APPKEY", ""),
)

# 发送消息
alchemy_furnace.send_message("title", "message")

# 发送消息并@所有人
alchemy_furnace.send_message_at("title", "message")

# 上传一张图片之后发过去(需要填写 secret1 与 secret0)
# alchemy_furnace.send_message(
#     "title", "![image](" + alchemy_furnace.get_ding_image_mediaid("image.jpg") + ")"
# )
