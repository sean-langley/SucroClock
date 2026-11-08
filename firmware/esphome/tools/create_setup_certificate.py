#!/usr/bin/env python3
"""Create a unique local HTTPS certificate once; never overwrite a device key."""
import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import ipaddress
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID

parser = argparse.ArgumentParser()
parser.add_argument("--hostname", default="sucroclock.local")
parser.add_argument("--ip", help="Optional reserved device IP for the certificate SAN")
parser.add_argument("--output-dir", type=Path, help="Alternate directory, e.g. for an explicit certificate rotation")
args = parser.parse_args()
folder = args.output_dir or (Path(__file__).resolve().parents[1] / ".clock-private")
folder.mkdir(mode=0o700, exist_ok=True)
cert_file, key_file = folder / "https-cert.pem", folder / "https-key.pem"
if cert_file.exists() or key_file.exists():
    parser.exit(1, "A certificate or key already exists. Keep it for normal updates; do not regenerate it.\n")
if not args.hostname or "/" in args.hostname or ":" in args.hostname:
    parser.error("--hostname must be a DNS name without a scheme or port")
names = [x509.DNSName(args.hostname)]
if args.ip:
    try: names.append(x509.IPAddress(ipaddress.ip_address(args.ip)))
    except ValueError: parser.error("--ip must be a valid IPv4 or IPv6 address")
key = ec.generate_private_key(ec.SECP256R1())
name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, args.hostname)])
now = datetime.now(timezone.utc)
cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name)
        .public_key(key.public_key()).serial_number(x509.random_serial_number())
        .not_valid_before(now-timedelta(minutes=5)).not_valid_after(now+timedelta(days=3650))
        .add_extension(x509.SubjectAlternativeName(names),critical=False)
        .add_extension(x509.BasicConstraints(ca=False,path_length=None),critical=True)
        .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]),critical=False)
        .sign(key,hashes.SHA256()))
with key_file.open("xb") as output:
    key_file.chmod(0o600)
    output.write(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
with cert_file.open("xb") as output:
    output.write(cert.public_bytes(serialization.Encoding.PEM))
print(f"Created unique device key and certificate in {folder}.")
print("SHA-256:",cert.fingerprint(hashes.SHA256()).hex().upper())
