#include "stdafx.h"
#include "graffiti.h"
#include "platform.h"

#include <cryptopp/rsa.h>
#include <cryptopp/rng.h>

namespace Graffiti
{

static bool s_privateKeyGenerated;
static CryptoPP::RSA::PrivateKey s_privateKey;

static const uint8_t s_publicKeyValve[] = {
    0x30, 0x81, 0x9D, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00,
    0x03, 0x81, 0x8B, 0x00, 0x30, 0x81, 0x87, 0x02, 0x81, 0x81, 0x00, 0xBE, 0xDB, 0x4B, 0xD8, 0x78, 0x96, 0xAB,
    0x74, 0xDE, 0xC8, 0xB1, 0xB3, 0x8C, 0xC9, 0xC7, 0x1A, 0x7B, 0xBF, 0xE6, 0x4D, 0x4B, 0x25, 0x9A, 0xE3, 0x50,
    0xDD, 0xF8, 0x8E, 0x89, 0x9C, 0x2B, 0x6B, 0x39, 0xEF, 0xEE, 0xA2, 0x09, 0xF4, 0x51, 0x33, 0xDC, 0xA0, 0x6A,
    0xD3, 0xE0, 0xB0, 0xC2, 0x74, 0x24, 0x30, 0xEC, 0x38, 0x96, 0x6F, 0x4F, 0xED, 0x46, 0xB8, 0xA7, 0x1F, 0xFD,
    0xDF, 0x28, 0x02, 0x1A, 0x87, 0x19, 0xAD, 0x12, 0x6B, 0xFB, 0xBF, 0x5E, 0x78, 0xCE, 0x11, 0x44, 0x36, 0x22,
    0xA2, 0x51, 0xBE, 0xBE, 0xBB, 0xB1, 0x8D, 0x80, 0x83, 0xAA, 0xE5, 0x57, 0x7F, 0xDE, 0x35, 0x39, 0x06, 0x11,
    0x5C, 0x74, 0xF8, 0x6C, 0x66, 0xBD, 0x29, 0x5E, 0x56, 0x9B, 0x6E, 0x51, 0x50, 0x92, 0x8E, 0x29, 0x90, 0xF9,
    0xAA, 0x47, 0xB1, 0x7B, 0x29, 0xA1, 0xE4, 0xEF, 0x50, 0xF9, 0xAF, 0x0B, 0xAF, 0x02, 0x01, 0x11
};

static void PatchPublicKeys(const CryptoPP::RSA::PrivateKey &privateKey)
{
    CryptoPP::RSA::PublicKey publicKey{ privateKey };

    std::string publicKeyString;
    CryptoPP::StringSink publicKeySink{ publicKeyString };
    publicKey.Save(publicKeySink);

    if (publicKeyString.size() != sizeof(s_publicKeyValve))
    {
        assert(false);
        return;
    }

    if (!Platform::PatchGraffitiPublicKey("client", s_publicKeyValve, publicKeyString.data(), sizeof(s_publicKeyValve)))
    {
        Platform::Print("Failed to patch graffiti public key in client! Graffiti will not work\n");
    }

    if (!Platform::PatchGraffitiPublicKey("server", s_publicKeyValve, publicKeyString.data(), sizeof(s_publicKeyValve)))
    {
        Platform::Print("Failed to patch graffiti public key in server! Graffiti will not work\n");
    }
}

void Initialize()
{
    if (s_privateKeyGenerated)
    {
        return;
    }

    CryptoPP::LC_RNG random{ 1 };
    s_privateKey.GenerateRandomWithKeySize(random, 1024);
    s_privateKeyGenerated = true;

    PatchPublicKeys(s_privateKey);
}

template<typename T>
inline void AppendBytes(std::vector<uint8_t> &buffer, const T &variable)
{
    const uint8_t *data = reinterpret_cast<const uint8_t *>(&variable);
    buffer.insert(buffer.end(), data, data + sizeof(T));
}

static void BuildBufferToSign(std::vector<uint8_t> &buffer, const PlayerDecalDigitalSignature &message)
{
    buffer.reserve(80);

    for (int i = 0; i < 3; i++)
    {
        AppendBytes(buffer, message.endpos(i));
    }

    for (int i = 0; i < 3; i++)
    {
        AppendBytes(buffer, message.startpos(i));
    }

    for (int i = 0; i < 3; i++)
    {
        AppendBytes(buffer, message.right(i));
    }

    for (int i = 0; i < 3; i++)
    {
        AppendBytes(buffer, message.normal(i));
    }

    AppendBytes(buffer, message.tx_defidx());
    AppendBytes(buffer, message.tint_id());
    AppendBytes(buffer, message.entindex());
    AppendBytes(buffer, message.hitbox());
    AppendBytes(buffer, message.creationtime());
    AppendBytes(buffer, message.accountid());
    AppendBytes(buffer, message.rtime());
    AppendBytes(buffer, message.trace_id());

    assert(buffer.size() == 80);
}

bool SignMessage(PlayerDecalDigitalSignature &message)
{
    if (!s_privateKeyGenerated)
    {
        return false;
    }

    std::vector<uint8_t> bufferToSign;
    BuildBufferToSign(bufferToSign, message);

    CryptoPP::LC_RNG random{ 1 };
    CryptoPP::RSASS<CryptoPP::PKCS1v15, CryptoPP::SHA1>::Signer signer{ s_privateKey };

    std::string signature;
    signature.resize(signer.MaxSignatureLength());

    size_t length = signer.SignMessage(random,
        bufferToSign.data(),
        bufferToSign.size(),
        reinterpret_cast<uint8_t *>(signature.data()));

    signature.resize(length);

    message.set_signature(signature);
    return true;
}

} // namespace Graffiti
