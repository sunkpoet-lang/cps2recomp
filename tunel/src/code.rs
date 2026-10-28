// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork).
//
// The room code: everything a guest needs to reach the host, with no server of our own. A version byte, the host's
// endpoint id (its public key, 32 bytes), the URL of its relay and a 2-byte check (so a code cut short or mistyped is
// caught, not read as another relay), in base32 without padding: letters and digits only, about 112 characters. The host's own IP addresses are left out: Iroh reaches it through the relay and finds the
// direct path from there (and the code does not show where the host lives). Pasted codes may come split over lines,
// with spaces or in lower case (WhatsApp): all of that is dropped before decoding.

use anyhow::{bail, Result};
use iroh::{EndpointAddr, EndpointId, RelayUrl};

const VERSION: u8 = 1;
pub const MAX_LEN: usize = 120;

// FNV-1a folded to 16 bits
fn check(bytes: &[u8]) -> [u8; 2] {
    let mut h: u32 = 0x811c9dc5;
    for &b in bytes {
        h = (h ^ b as u32).wrapping_mul(0x01000193);
    }
    (((h >> 16) ^ h) as u16).to_le_bytes()
}

pub fn encode(id: &EndpointId, relay: &RelayUrl) -> String {
    let mut bytes = vec![VERSION];
    bytes.extend_from_slice(id.as_bytes());
    bytes.extend_from_slice(relay.to_string().as_bytes());
    let sum = check(&bytes);
    bytes.extend_from_slice(&sum);
    let code = data_encoding::BASE32_NOPAD.encode(&bytes);
    debug_assert!(code.len() <= MAX_LEN, "a room code of {} characters", code.len());
    code
}

pub fn decode(code: &str) -> Result<EndpointAddr> {
    let clean: String = code.chars().filter(char::is_ascii_alphanumeric).collect::<String>().to_ascii_uppercase();
    let Ok(mut bytes) = data_encoding::BASE32_NOPAD.decode(clean.as_bytes()) else { bail!("el código no es válido") };
    if bytes.len() < 1 + 32 + 8 + 2 || bytes[0] != VERSION {
        bail!("el código no es válido");
    }
    let sum = bytes.split_off(bytes.len() - 2);
    if sum != check(&bytes) {
        bail!("el código no es válido");
    }
    let key: [u8; 32] = bytes[1..33].try_into().expect("32 bytes");
    let Ok(id) = EndpointId::from_bytes(&key) else { bail!("el código no es válido") };
    let Ok(relay) = std::str::from_utf8(&bytes[33..]).unwrap_or("").parse::<RelayUrl>() else {
        bail!("el código no es válido")
    };
    Ok(EndpointAddr::new(id).with_relay_url(relay))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample() -> (EndpointId, RelayUrl) {
        let id = iroh::SecretKey::from_bytes(&[7; 32]).public();
        (id, "https://use1-1.relay.n0.iroh.link./".parse().unwrap())
    }

    #[test]
    fn round_trip_and_length() {
        let (id, relay) = sample();
        let code = encode(&id, &relay);
        assert!(code.len() <= MAX_LEN, "{} characters", code.len());
        assert!(code.chars().all(|c| c.is_ascii_alphanumeric()));
        let addr = decode(&code).unwrap();
        assert_eq!(addr.id, id);
        assert_eq!(addr.relay_urls().next(), Some(&relay));
    }

    #[test]
    fn pasted_from_whatsapp() {
        let (id, relay) = sample();
        let code = encode(&id, &relay);
        let (a, b) = code.split_at(50);
        let pasted = format!("  {}\n{} ", a.to_lowercase(), b);
        assert_eq!(decode(&pasted).unwrap().id, id);
    }

    #[test]
    fn cut_in_half() {
        let (id, relay) = sample();
        let code = encode(&id, &relay);
        let err = decode(&code[..code.len() / 2]).unwrap_err();
        assert!(err.to_string().contains("no es válido"));
        assert!(decode("hola").is_err());
    }
}
