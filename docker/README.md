# Building and exporting Docker images behind a Proxy

## Prerequisites

- On Windows, make sure, you have the latest version of Docker for Windows installed and you use WSL2 to "host" the Docker daemon (do not use Hyper-V, since docker buildx is not working behind a proxy in this configuration properly)
- __Assumption__: You are working on Windows, and your non-auth proxy (CNTLM, px) is reachable on port 3128 (here: `http://docker.for.win.localhost:3128`)
  - __>> Linux only: <<__ Use _[http://172.17.0.1:3128](http://172.17.0.1:3128)_ as proxy address

## >> ARM64 target platform only <<

### Setup buildx builder

- If you are working behind a proxy, you need to specify this within your builder instance.
  - Create a new builder and set it as active build instance
  `docker buildx create --driver-opt env.http_proxy=http://docker.for.win.localhost:3128 --driver-opt env.https_proxy=http://docker.for.win.localhost:3128 --use --name proxy-builder`

## Build Docker image

- There might be other arguments necessary. This will be explained in the respective component readme files. This guide only deals with the parameters, which are relevant for Proxy configurations.

### >> ARM64 target platform only <<

- Build and export an image behind a proxy
  `docker buildx build --platform linux/arm64 -t <Tag> -o type=oci,dest=<path-to-image-file-name>.tar --build-arg HTTP_PROXY=http://docker.for.win.localhost:3128 --build-arg HTTPS_PROXY=http://docker.for.win.localhost:3128 -f <path-to-dockerfile> .`

### >> AMD64 target platform only <<

- Build an image behind a proxy
  `docker build -t <Tag> --build-arg HTTP_PROXY=http://docker.for.win.localhost:3128 --build-arg HTTPS_PROXY=http://docker.for.win.localhost:3128 -f <path-to-dockerfile> .`

## (Optional) Linux px proxy configuration

If you are running behind a px-proxy on Linux you need to ensure the binding between your docker network and proxy is configured.

1.) __~/.px/config.ini:__ Ensure binding ___"binds = [...], \<docker network proxy\>___ (e.g. 172.17.0.1:3128) exists

`[server]
binds = 127.0.0.1:3128, 172.17.0.1:3128#`

If not, add it and restart your proxy (e.g. via _osd-proxy-restart_ for osd)

To check that the binding exists you can call for your proxy port (e.g. 3128):
`netstat -ntlpn | grep -i 3128`

Which should show the your docker-network proxy (e.g. 172.17.0.1:3128):
`tcp       0     0 172.17.0.1:3128        0.0.0.0:*              LISTEN     12391/python3`

2.) __~/.docker/config.json:__ Ensure _http(s)Proxys_ in your docker-network have the same port as your host proxy (e.g. [http://172.17.0.1:3128])

```json
{
 "proxies":
 {
   "default":
   {
     "httpProxy": "http://172.17.0.1:3128",
     "httpsProxy": "http://172.17.0.1:3128"
   }
 }
}
```

3.) __/etc/systemd/system/docker.service.d/http_proxy.conf__: Ensure that the http(s)_proxies are set

```code
[Service]
Environment=HTTP_PROXY=http://localhost:3128/
Environment=HTTPS_PROXY=http://localhost:3128/
```

Afterwards you have to restart your docker daemon:
`sudo systemctl daemon-reload`
`sudo systemctl restart docker`

To check your env-variables for docker you can call:
`sudo systemctl show --property=Environment docker`
