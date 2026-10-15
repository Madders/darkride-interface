This is for building docker images for the server stack and ice server.

`server/Dockerfile` builds the domain server, assignment client and ice server from source on Ubuntu 24.04
and packs them into a slim runtime image. From the repository root (a checkout with LF line endings):

```bash
docker build -f docker/server/Dockerfile -t darkride-server .
docker compose -f docker/server/docker-compose.yml up -d    # or: docker run --network host -v $PWD/my-world:/data darkride-server
```

The world's web interface is then on http://localhost:40100. The ice server is in the same image:
`docker run --network host darkride-server /opt/darkride/ice-server` (UDP 7337).

The old GitLab pipeline (`.gitlab/build/linux.gitlab-ci.yml`, `.gitlab/deploy.gitlab-ci.yml`) and
`ice-server/Dockerfile` packaged AppDirs built by Tivoli's CI, which no longer exists.
