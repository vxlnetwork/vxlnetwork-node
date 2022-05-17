#!/bin/bash

sudo mkdir -p /etc/docker && echo '{"ipv6":true,"fixed-cidr-v6":"2001:db8:1::/64"}' | sudo tee /etc/docker/daemon.json && sudo service docker restart

ci/build-docker-image.sh docker/ci/Dockerfile-base vxlnetworkcurrency/vxlnetwork-env:base
if [[ "${COMPILER:-}" != "" ]]; then
    ci/build-docker-image.sh docker/ci/Dockerfile-gcc vxlnetworkcurrency/vxlnetwork-env:${COMPILER}
else
    ci/build-docker-image.sh docker/ci/Dockerfile-gcc vxlnetworkcurrency/vxlnetwork-env:gcc
    ci/build-docker-image.sh docker/ci/Dockerfile-clang-6 vxlnetworkcurrency/vxlnetwork-env:clang-6
fi
