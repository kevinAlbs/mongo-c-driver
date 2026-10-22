#!/usr/bin/env bash
#-- disagg-cluster.sh -- Start / stop a local disaggregated-storage cluster
#   and smoke-test it with a wire-protocol "ping" (DRIVERS-3190).
#
# This is an experiment for running the SLS "slslocal" tool in Evergreen in
# docker-compose mode. All cluster components (SLS services, mongod shards,
# LocalStack) run as containers pulled from the SRE-owned mms-scratch ECR
# registry, so nothing else is installed: this needs only the docker and
# 'aws' CLIs and AWS credentials in the environment (as set by the
# ec2.assume_role command on the 'disagg-storage-tf-project-ro' role).
#
# Usage:
#   disagg-cluster.sh start   # docker login, launch the cluster, wait for health
#   disagg-cluster.sh ping    # wire-protocol hello + ping against a mongod node
#   disagg-cluster.sh stop    # destroy the cluster
#   disagg-cluster.sh smoke   # start, ping, stop (with cleanup on failure)
#
# Return: 0 on success, non-zero with an "ERROR:" message on failure.
# Side effects: writes cluster state under $HOME/.slslocal (persisted across
#   invocations by the mounts below), pulls multiple multi-GB docker images,
#   and creates docker networks/containers named sls-cell-*.

set -o errexit
set -o pipefail
set -o nounset
set -o verbose

ECR_ROOT=664315256653.dkr.ecr.us-east-1.amazonaws.com
DOCKER_ARCH=linux-x86-64
STATE_DIR="$HOME/.slslocal"
AWS_DIR="$STATE_DIR/aws"
TAG_FILE="$STATE_DIR/disagg-cluster-image-tag"

fail() {
  echo "ERROR: $*" >&2
  exit 1
}

[ "$(uname -m)" == "x86_64" ] || fail "expected an x86_64 host"
command -v docker >/dev/null 2>&1 || fail "'docker' is not installed on this host"
command -v aws >/dev/null 2>&1 || fail "the 'aws' CLI is not installed on this host"
command -v python3 >/dev/null 2>&1 || fail "'python3' is not installed on this host"
[ -n "${AWS_ACCESS_KEY_ID:-}" ] || fail "AWS_ACCESS_KEY_ID is not set (expected ec2.assume_role credentials)"

# SLS docker images are tagged '<arch>-<10gen/sls git sha>' for mainline
# builds and '<arch>-evg-patch-<version id>[-debug]' for patch builds; there
# is no 'latest' tag. The slslocal image and the SLS service images share the
# same tag, so resolve a tag that exists in BOTH the 'slslocal' and 'log'
# repositories, preferring a mainline (release) build.
#
# NOTE: this deliberately avoids 'slslocal --sls-ecr-tag latest', which
# resolves the stable tag by reading the s3://sls-versions bucket - a
# permission the read-only ECR role does not have.
resolve_sls_tag() {
  local tmpdir
  tmpdir=$(mktemp -d)
  for repo in slslocal log; do
    aws ecr describe-images --region us-east-1 \
      --repository-name "disagg-storage/$repo" \
      --filter tagStatus=TAGGED \
      --output json >"$tmpdir/$repo.json" ||
      fail "failed to list image tags in disagg-storage/$repo"
  done
  local status=0
  python3 - "$tmpdir/slslocal.json" "$tmpdir/log.json" <<'PY' || status=$?
import json, re, sys

def mainline(path, suffix=""):
    """Map mainline image tag -> push time for one ECR repository listing."""
    pattern = r"linux-x86-64-[0-9a-f]{40}" + suffix
    with open(path) as f:
        images = json.load(f)["imageDetails"]
    out = {}
    for i in images:
        for t in i.get("imageTags", []):
            if re.fullmatch(pattern, t):
                out[t] = i.get("imagePushedAt", 0)
    return out

def common(suffix=""):
    t1 = mainline(sys.argv[1], suffix)
    t2 = mainline(sys.argv[2], suffix)
    return {t: t2[t] for t in set(t1) & set(t2)}

common = common() or common("-debug")
if not common:
    sys.exit("ERROR: no common mainline linux-x86-64 image tag shared by the "
             "slslocal and log repositories")
# Pick the most recently pushed of the common tags.
print(max(common, key=common.get))
PY
  rm -rf "$tmpdir"
  [ "$status" -eq 0 ] || fail "failed to resolve an SLS image tag from ECR"
}

docker_login() {
  aws ecr get-login-password --region us-east-1 |
    docker login --username AWS --password-stdin "$ECR_ROOT" ||
    fail "docker login to '$ECR_ROOT' failed"
}

# Materialize the assumed-role credentials as a 'default' AWS profile for the
# aws CLI inside the slslocal container (slslocal reads a named profile; the
# 'default' profile avoids depending on the mms-scratch SSO login).
write_aws_profile() {
  mkdir -p "$AWS_DIR" "$STATE_DIR"
  umask 077
  cat >"$AWS_DIR/credentials" <<EOF
[default]
aws_access_key_id = ${AWS_ACCESS_KEY_ID}
aws_secret_access_key = ${AWS_SECRET_ACCESS_KEY}
aws_session_token = ${AWS_SESSION_TOKEN:-}
EOF
  cat >"$AWS_DIR/config" <<EOF
[default]
region = us-east-1
EOF
}

# Run the slslocal container with the same mounts as the 'just slslocal'
# recipe in 10gen/sls: the docker socket (to drive the host daemon) and the
# state directory both as the container home and at its host path (compose
# bind-mount paths are resolved by the host daemon).
run_slslocal() {
  local tag
  tag=$(cat "$TAG_FILE" 2>/dev/null) || true
  [ -n "$tag" ] || tag=$(resolve_sls_tag)
  docker run --rm \
    -v /var/run/docker.sock:/var/run/docker.sock \
    --privileged \
    --network=host \
    -v "$STATE_DIR":/root \
    -v "$STATE_DIR":"$STATE_DIR" \
    -v "$AWS_DIR":/root/.aws \
    -e SLSLOCAL_HOST_HOME="$HOME" \
    -e AWS_ACCESS_KEY_ID \
    -e AWS_SECRET_ACCESS_KEY \
    -e AWS_SESSION_TOKEN \
    -e AWS_REGION=us-east-1 \
    -e AWS_DEFAULT_REGION=us-east-1 \
    "$ECR_ROOT/disagg-storage/slslocal:$tag" "$@"
}

start() {
  docker_login
  write_aws_profile
  local tag
  tag=$(resolve_sls_tag)
  echo "$tag" >"$TAG_FILE"
  local image="$ECR_ROOT/disagg-storage/slslocal:$tag"
  docker pull "$image" || fail "docker pull of '$image' failed"
  echo "Starting disagg cluster (SLS image tag: $tag)"
  # --localstack routes all S3/SQS access through a LocalStack container, so
  # no real AWS access is needed beyond the ECR image pulls.
  # The mongod image tag is left at slslocal's baked-in default (the pinned
  # mongodb-mongo-master disagg build from the SLS manifest).
  run_slslocal --compose launch --sls-ecr-tag "$tag" --localstack --aws-profile-name default ||
    fail "slslocal launch failed"
  echo "Waiting for the cluster to become healthy (up to 10 minutes)"
  run_slslocal --compose wait --timeout 10m ||
    fail "cluster did not become healthy within 10 minutes"
}

stop() {
  echo "Stopping disagg cluster"
  run_slslocal --compose destroy || fail "slslocal destroy failed"
}

ping_cluster() {
  python3 - <<'PY' || fail "wire-protocol ping failed"
import glob, json, os, socket, struct, sys

# --- Find the mongod endpoint from the generated ports.json ---
paths = glob.glob(os.path.expanduser("~/.slslocal/**/ports.json"), recursive=True)
if not paths:
    sys.exit("ERROR: no ports.json found under ~/.slslocal (did the cluster start?)")
with open(max(paths, key=os.path.getmtime)) as f:
    ports = json.load(f)
shards = ports.get("shards_by_log_id") or {}
if not shards:
    sys.exit("ERROR: ports.json has no mongod shards: " + json.dumps(ports)[:500])
node = next(iter(next(iter(shards.values())).values()))
host, port = node.rsplit(":", 1)
print(f"Pinging mongod at {host}:{port}")

# --- Minimal BSON + OP_MSG encoding ---
def cstr(s):
    return s.encode() + b"\x00"

def enc_int32(key, val):
    return b"\x10" + cstr(key) + struct.pack("<i", val)

def enc_str(key, val):
    b = val.encode() + b"\x00"
    return b"\x02" + cstr(key) + struct.pack("<i", len(b)) + b

def doc(*elements):
    body = b"".join(elements)
    return struct.pack("<i", len(body) + 5) + body + b"\x00"

def op_msg(document, request_id):
    body = b"\x00" + document  # flagBits = 0, then a single kind-0 body section
    return struct.pack("<iiii", 16 + 4 + len(body), request_id, 0, 2013) + \
        struct.pack("<I", 0) + body

def read_reply(sock):
    header = b""
    while len(header) < 16:
        chunk = sock.recv(16 - len(header))
        if not chunk:
            raise RuntimeError("connection closed by server")
        header += chunk
    (total, _, response_to, op_code) = struct.unpack("<iiii", header)
    if op_code != 2013:
        raise RuntimeError(f"unexpected reply opcode {op_code}")
    body = b""
    while len(body) < total - 16:
        chunk = sock.recv(total - 16 - len(body))
        if not chunk:
            raise RuntimeError("connection closed by server")
        body += chunk
    flags = struct.unpack("<I", body[:4])[0]
    if flags & 1:  # checksumPresent
        body = body[: body.rfind(b"\x01" + b"\x00" * 20)]  # strip kind byte + checksum
    kind = body[4]
    if kind != 0:
        raise RuntimeError(f"unexpected reply section kind {kind}")
    return response_to, body[5:]

def skip_value(etype, bson, pos):
    """Return the offset just past a BSON value of the given element type."""
    if etype in (0x01, 0x09, 0x11):  # double, UTC datetime, timestamp
        return pos + 8
    if etype in (0x02, 0x0D, 0x0E):  # string, code, symbol
        return pos + 4 + struct.unpack("<i", bson[pos:pos + 4])[0]
    if etype in (0x03, 0x04, 0x0F):  # document, array, code-with-scope
        return pos + struct.unpack("<i", bson[pos:pos + 4])[0]
    if etype == 0x05:  # binary: int32 length + subtype byte + bytes
        return pos + 4 + 1 + struct.unpack("<i", bson[pos:pos + 4])[0]
    if etype == 0x07:  # ObjectId
        return pos + 12
    if etype == 0x08:  # bool
        return pos + 1
    if etype in (0x0A, 0xFF, 0x7F):  # null, MinKey, MaxKey
        return pos
    if etype == 0x10:  # int32
        return pos + 4
    if etype == 0x12:  # int64
        return pos + 8
    if etype == 0x13:  # decimal128
        return pos + 16
    if etype == 0x0B:  # regex: two cstrings
        p = bson.index(b"\x00", pos) + 1
        return bson.index(b"\x00", p) + 1
    if etype == 0x0C:  # dbPointer: string + 12 bytes
        return pos + 4 + struct.unpack("<i", bson[pos:pos + 4])[0] + 12
    raise RuntimeError(f"unhandled BSON element type {etype}")

def reply_fields(bson):
    """Decode only the top-level int32/bool fields of a BSON document."""
    fields = {}
    size = struct.unpack("<i", bson[:4])[0]
    pos = 4
    while pos < size - 1:
        etype = bson[pos]
        pos += 1
        key_end = bson.index(b"\x00", pos)
        key = bson[pos:key_end].decode()
        pos = key_end + 1
        if etype == 0x01:  # double (e.g. 'ok')
            fields[key] = struct.unpack("<d", bson[pos:pos + 8])[0]
        elif etype == 0x10:  # int32
            fields[key] = struct.unpack("<i", bson[pos:pos + 4])[0]
        elif etype == 0x08:  # bool
            fields[key] = bson[pos] != 0
        pos = skip_value(etype, bson, pos)
    return fields

with socket.create_connection((host, int(port)), timeout=30) as sock:
    sock.settimeout(30)

    # Handshake: hello (also proves this is really a mongod).
    sock.sendall(op_msg(doc(enc_int32("hello", 1), enc_int32("helloOk", 1), enc_str("$db", "admin")), 1))
    response_to, reply = read_reply(sock)
    if response_to != 1:
        raise RuntimeError("hello reply has wrong responseTo")
    hello = reply_fields(reply)
    if hello.get("ok") != 1:
        raise RuntimeError(f"hello failed: {hello}")
    print(f"hello ok (wire version {hello.get('maxWireVersion')}, isWritablePrimary={hello.get('isWritablePrimary')})")

    # The actual smoke test: ping.
    sock.sendall(op_msg(doc(enc_int32("ping", 1), enc_str("$db", "admin")), 2))
    response_to, reply = read_reply(sock)
    if response_to != 2:
        raise RuntimeError("ping reply has wrong responseTo")
    ping = reply_fields(reply)
    if ping.get("ok") != 1:
        raise RuntimeError(f"ping failed: {ping}")
    print(f"ping ok: {ping}")
print("Disagg cluster smoke test PASSED")
PY
}

smoke() {
  trap 'stop || true' EXIT
  start
  ping_cluster
  stop
  trap - EXIT
}

case "${1:-}" in
  start) start ;;
  ping) ping_cluster ;;
  stop) stop ;;
  smoke) smoke ;;
  *)
    echo "usage: $0 start|ping|stop|smoke" >&2
    exit 2
    ;;
esac
