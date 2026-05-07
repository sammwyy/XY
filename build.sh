#!/usr/bin/env sh
set -eu

IMAGE_NAME="${XYON_DOCKER_IMAGE:-xyon-ps2}"
EXAMPLE_ID="${1:-render_images}"

docker build -t "$IMAGE_NAME" .
if [ "$EXAMPLE_ID" = "all" ]; then
  docker run --rm -v "$(pwd):/xyon" -w /xyon "$IMAGE_NAME" make all
else
  docker run --rm -v "$(pwd):/xyon" -w /xyon "$IMAGE_NAME" make -C "examples/$EXAMPLE_ID" all
fi
