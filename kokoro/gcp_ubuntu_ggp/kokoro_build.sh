#!/bin/bash

# Fail on any error.
set -e

DIR="/mnt/github/orbitprofiler"
SCRIPT="${DIR}/kokoro/gcp_ubuntu_ggp/kokoro_build.sh"

if [ "$0" == "$SCRIPT" ]; then
  # We are inside the docker container

  echo "Installing conan configuration (profiles, settings, etc.)..."
  conan config install "${DIR}/contrib/conan/configs/linux"
  cp -v "${DIR}/kokoro/conan/config/remotes.json" ~/.conan/remotes.json
  conan config set general.revisions_enabled=True
  conan config set general.parallel_download=8

  CONAN_PROFILE="ggp_relwithdebinfo"

  # This variable is picked up by the `conan package` call.
  # The current public key from keystore will be embedded into
  # the install_signed_package.sh script.
  export ORBIT_SIGNING_PUBLIC_KEY_FILE="/mnt/keystore/74938_SigningPublicGpg"

  # Building Orbit
  conan install -u -pr ${CONAN_PROFILE} -if "${DIR}/build/" \
          --build outdated -o debian_packaging=True "${DIR}"
  conan build -bf "${DIR}/build/" "${DIR}"
  conan package -bf "${DIR}/build/" "${DIR}"

  rm -rf ~/.gnupg/
  rm -rf /dev/shm/signing.gpg
  mkdir -p ~/.gnupg
  chmod 700 ~/.gnupg
  echo "allow-loopback-pinentry" > ~/.gnupg/gpg-agent.conf

  GPG_OPTIONS="--pinentry-mode loopback --batch --no-tty --yes --no-default-keyring --keyring /dev/shm/signing.gpg --passphrase-file /mnt/keystore/74938_SigningPrivateGpgKeyPassword"

  gpg ${GPG_OPTIONS} --import /mnt/keystore/74938_SigningPrivateGpg

  for deb in ${DIR}/build/package/*.deb; do
    gpg ${GPG_OPTIONS} --output "$deb.asc" --detach-sign "$deb"
  done

  # Uploading prebuilt packages of our dependencies
  if [ -f /mnt/keystore/74938_orbitprofiler_artifactory_access_token ]; then
    conan user -r artifactory -p "$(cat /mnt/keystore/74938_orbitprofiler_artifactory_access_token | tr -d '\n')" kokoro
    # The llvm-package is very large and not needed as a prebuilt because it is not consumed directly.
    conan remove 'llvm/*@orbitdeps/stable'
    conan upload -r artifactory --all --no-overwrite all --confirm  --parallel \*
  else
    echo "No access token available. Won't upload packages."
  fi

  # Package the Debian package and the signature into a zip for integration in the installer.
  cd "${DIR}/build/package"
  mkdir -p Orbit/collector
  cp OrbitProfiler*.deb Orbit/collector/
  cp OrbitProfiler*.deb.asc Orbit/collector/
  zip Collector.zip -r Orbit
  rm -rf Orbit
  cd -

  # Uncomment the three lines below to print the external ip into the log and
  # keep the vm alive for two hours. This is useful to debug build failures that
  # can not be resolved by looking into sponge alone. Also comment out the
  # "set -e" at the top of this file (otherwise a failed build will exit this
  # script immediately).
  # external_ip=$(curl -s -H "Metadata-Flavor: Google" http://metadata/computeMetadata/v1/instance/network-interfaces/0/access-configs/0/external-ip)
  # echo "INSTANCE_EXTERNAL_IP=${external_ip}"
  # sleep 7200;

else
  gcloud auth configure-docker --quiet
  docker run --rm --network host -v ${KOKORO_ARTIFACTS_DIR}:/mnt gcr.io/orbitprofiler/clang7_gpg:latest $SCRIPT
fi

