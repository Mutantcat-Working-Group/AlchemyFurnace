# AlchemyFurnace Go 示例容器：用于在独立环境中验证/使用通知 CLI。
FROM golang:1.24-alpine AS build

ARG VERSION=1.0.20260920

WORKDIR /src

COPY go/go.mod ./
COPY go/ .

RUN CGO_ENABLED=0 go build -trimpath \
    -ldflags "-s -w" \
    -o /out/alchemyfurnace ./cmd/example

FROM alpine:3.21

RUN apk add --no-cache ca-certificates \
    && adduser -D -H -u 10001 alchemyfurnace

COPY --from=build /out/alchemyfurnace /usr/local/bin/alchemyfurnace

USER alchemyfurnace

ENTRYPOINT ["/usr/local/bin/alchemyfurnace"]
