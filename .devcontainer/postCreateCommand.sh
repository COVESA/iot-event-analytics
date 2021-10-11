#!/bin/bash

# Install homebrew
# sudo /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

export HTTP_PROXY=http://host.docker.internal:3128/
export HTTPS_PROXY=http://host.docker.internal:3128/

# Update packages
sudo apt update

# Install NodeJS
sudo apt install nodejs

# nektos/act is to simulate GitHub Actions locally
curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash

# Docker-Compose
sudo curl -L "https://github.com/docker/compose/releases/download/1.29.2/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
