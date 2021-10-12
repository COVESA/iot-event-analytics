# Building Docker Images

- Workflow: .github/workflows/docker_images.yml
- Inputs: doPerformPush = Default false. If true, push the docker image to the configured registry
- Manual Test: clear && act workflow_dispatch -e ./.github/workflows/testevent-build-image.json -s GITHUB_TOKEN=<PAT>

## Creating a GitHub Personal Access Token for local testing
- Create personal access token (PAT): https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/creating-a-personal-access-token
- Permissions: https://docs.github.com/en/packages/learn-github-packages/about-permissions-for-github-packages#about-scopes-and-permissions-for-package-registries

## Corporate Proxy
- Create file ~/.docker/config.json (see https://docs.docker.com/network/proxy/)
