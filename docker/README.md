This is for building docker images for the server stack and ice server.

**Prebuilt image:** each release (and each push to the `darkride-builds` branch) is built and tested by
`.github/workflows/release.yml` and published as `ghcr.io/madders/darkride-server`: `:latest` is the newest release,
`:<version>` a particular one, `:darkride-builds` the branch. To use it instead of building, replace the `build:`
section of `server/docker-compose.yml` with `image: ghcr.io/madders/darkride-server:latest`, then
`docker compose pull && docker compose up -d`.

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
