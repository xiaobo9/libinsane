#!/bin/sh

directory=doc/build

if [ -z "$DELIVERY_KEY" ] ; then
  echo "Delivery: No ssh private key provided."
  exit 0
fi

if ! which ssh-agent; then
  echo "ssh-agent not available. Installing"
  if ! (apt-get update -y && apt-get install openssh-client -y) && ! pacman --needed --noconfirm -S openssh ; then
    echo "Failed to install ssh-agent"
    exit 1
  fi
fi
if ! which rsync; then
  echo "rsync not available. Installing"
  if ! (apt-get update -y && apt-get install rsync -y) && ! pacman --needed --noconfirm -S rsync ; then
    echo "Failed to install rsync"
    exit 1
  fi
fi

mkdir -p ~/.ssh
chmod 700 ~/.ssh

rm -f ~/.ssh/known_hosts
echo "$DELIVERY_SERVER_KEY" > ~/.ssh/known_hosts
chmod 644 ~/.ssh/known_hosts

eval $(ssh-agent -s)
echo "$DELIVERY_KEY" | tr -d '\r' | ssh-add - > /dev/null

echo "Delivering: ${directory} (${CI_COMMIT_REF_NAME} - ${CI_COMMIT_SHORT_SHA})"
echo "To: $DELIVERY_USER@$DELIVERY_SERVER:$DELIVERY_DOC_PATH"

out_name="libinsane/$(date "+%Y%m%d_%H%M%S")_${CI_COMMIT_REF_NAME}_${CI_COMMIT_SHORT_SHA}"
latest_name="libinsane/latest"

ls -lh "${directory}"

if ! rsync -rtz "${directory}/" "${DELIVERY_USER}@${DELIVERY_SERVER}:${DELIVERY_DOC_PATH}/${out_name}" ; then
  echo "rsync failed"
  exit 1
fi

echo "Updating symlink 'latest' ..."
if ! ssh "${DELIVERY_USER}@${DELIVERY_SERVER}" -- ln -fs \
    ${out_name} \
    ${DELIVERY_DOC_PATH}/${latest_name} ; then
  echo ln failed
  exit 1
fi

echo Success
exit 0
