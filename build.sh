#!/usr/bin/env sh
set -eu

IMAGE_NAME="${XY_DOCKER_IMAGE:-xy-ps2}"
EXAMPLE_ID="${1:-render_images}"

docker build -t "$IMAGE_NAME" .
if [ "$EXAMPLE_ID" = "all" ]; then
  docker run --rm -v "$(pwd):/xy" -w /xy "$IMAGE_NAME" make all
else
  docker run --rm -v "$(pwd):/xy" -w /xy "$IMAGE_NAME" make -C "examples/$EXAMPLE_ID" all
fi
